/** @fileoverview Shared overlay chrome (see overlay-chrome.hpp). */
#include "overlay-chrome.hpp"
#include "theme-colors.hpp"

#include <QFont>
#include <QFontMetricsF>
#include <QString>
#include <QStringList>
#include <QPainter>

#include <algorithm>

QFont chromeFont(int pixelSize, bool bold) {
  static const QFont base = [] {
    QFont font;
    font.setFamilies({QStringLiteral("Adwaita Sans"),
                      QStringLiteral("Noto Sans")});
    return font;
  }();
  QFont font = base;
  font.setPixelSize(pixelSize);
  font.setBold(bold);
  return font;
}

QFont chromeDefaultFont() {
  QFont font;
  font.setFamilies({QStringLiteral("Adwaita Sans"),
                    QStringLiteral("Noto Sans")});
  font.setPointSize(11);
  return font;
}

QFont chromeMonoFont(int pixelSize, bool bold) {
  static const QFont base = [] {
    QFont font;
    // Pinned stack, not the bare `monospace` alias: the alias resolves to
    // whatever fontconfig prefers, and a system mono face with a missing
    // fixed-pitch flag (seen on a Noto Sans Mono install reporting
    // fixedPitch=false) would silently give proportional numerals.
    // JetBrains Mono ships bundled (registered by loadCaptureFonts()) so it
    // is always available; the rest are common system fallbacks.
    font.setFamilies({QStringLiteral("JetBrains Mono"),
                      QStringLiteral("DejaVu Sans Mono"),
                      QStringLiteral("monospace")});
    return font;
  }();
  QFont font = base;
  font.setPixelSize(pixelSize);
  font.setBold(bold);
  return font;
}

QString captureTabLabel(CaptureKind kind) {
  switch (kind) {
  case CaptureKind::Region:
    return QStringLiteral("REGION");
  case CaptureKind::Fullscreen:
    return QStringLiteral("FULLSCREEN");
  }
  return {};
}

QString timingTabLabel(CaptureTiming timing) {
  switch (timing) {
  case CaptureTiming::Instant:
    return QStringLiteral("INSTANT");
  case CaptureTiming::Delayed:
    return QStringLiteral("DELAYED");
  }
  return {};
}

namespace {
QFont captureTabFont() {
  QFont font(QStringLiteral("Noto Sans"));
  font.setBold(true);
  font.setPixelSize(11);
  return font;
}
QColor captureTabAccent(CaptureKind kind) {
  Q_UNUSED(kind);
  // One accent for the active tab, whatever the kind: the label names the
  // mode, so color distinguishes state (active vs idle), not mode. Sourced
  // from the Noctalia theme with a Niri focus-ring fallback.
  return currentThemeColors().accent;
}
} // namespace

QVector<CaptureTab> captureTabLayout(const QRect &bounds) {
  static const CaptureKind order[] = {CaptureKind::Region,
                                       CaptureKind::Fullscreen};
  const QFontMetricsF metrics(captureTabFont());
  constexpr qreal kPad = 14.0;
  constexpr qreal kGap = 2.0;
  constexpr qreal kHeight = 26.0;
  // Flush to the top edge (see drawCaptureTabs' -30 background extension):
  // derived from kCaptureTabBarBottom rather than a separate magic number,
  // so the two can't drift apart.
  constexpr qreal kTop = kCaptureTabBarBottom - kHeight - 5.0;
  QVector<CaptureTab> tabs;
  qreal total = 0.0;
  for (const CaptureKind kind : order) {
    const qreal w = metrics.horizontalAdvance(captureTabLabel(kind)) + 2 * kPad;
    tabs.push_back({kind, QRectF(total, kTop, w, kHeight)});
    total += w + kGap;
  }
  total -= kGap;
  const qreal left = bounds.left() + (bounds.width() - total) / 2.0;
  for (CaptureTab &tab : tabs)
    tab.rect.translate(left, 0);
  return tabs;
}

int captureTabAt(const QVector<CaptureTab> &tabs, const QPointF &position) {
  for (int index = 0; index < tabs.size(); ++index) {
    if (tabs.at(index).rect.adjusted(-2, -6, 2, 6).contains(position))
      return index;
  }
  return -1;
}

void drawCaptureTabs(QPainter &painter, const QVector<CaptureTab> &tabs,
                     CaptureKind active, const QPointF &cursor) {
  if (tabs.isEmpty())
    return;
  const ThemeColors &theme = currentThemeColors();
  // Hangs off the top edge like a tab strip: square at the top (drawn past
  // the edge so only the bottom corners round), not a floating pill.
  const QRectF bar = tabs.constFirst().rect.united(tabs.constLast().rect)
                         .adjusted(-5, -30, 5, 5);
  painter.setPen(QPen(withAlpha(theme.onSurface, 32), 1));
  painter.setBrush(withAlpha(theme.surface, 235));
  painter.drawRoundedRect(bar, 12, 12);
  painter.setFont(captureTabFont());
  const int hovered = captureTabAt(tabs, cursor);
  for (int index = 0; index < tabs.size(); ++index) {
    const CaptureTab &tab = tabs.at(index);
    painter.setPen(Qt::NoPen);
    if (tab.kind == active) {
      painter.setBrush(captureTabAccent(tab.kind));
      painter.drawRoundedRect(tab.rect, 9, 9);
      painter.setPen(theme.onAccent);
    } else {
      if (index == hovered) {
        painter.setBrush(withAlpha(theme.onSurface, 28));
        painter.drawRoundedRect(tab.rect, 9, 9);
      }
      painter.setPen(withAlpha(theme.onSurface, index == hovered ? 255 : 190));
    }
    painter.drawText(tab.rect, Qt::AlignCenter, captureTabLabel(tab.kind));
  }
}

namespace {
QString delayValueLabel(int delaySecs) {
  const int clamped =
      std::clamp(delaySecs, kCaptureDelayMinSecs, kCaptureDelayMaxSecs);
  return QStringLiteral("%1s").arg(clamped);
}
} // namespace

CaptureBarLayout captureBarLayout(const QRect &bounds, CaptureTiming timing,
                                  int delaySecs) {
  CaptureBarLayout bar;
  const QFontMetricsF metrics(captureTabFont());
  constexpr qreal kPad = 14.0;
  constexpr qreal kGap = 2.0;
  constexpr qreal kGroupGap = 10.0;
  constexpr qreal kHeight = 26.0;
  constexpr qreal kTop = kCaptureTabBarBottom - kHeight - 5.0;
  constexpr qreal kStepperButton = 26.0;
  constexpr qreal kValuePad = 8.0;
  const auto textWidth = [&](const QString &label) {
    return metrics.horizontalAdvance(label) + 2 * kPad;
  };
  struct Item {
    bool isKind;
    int index;
    qreal width;
  };
  QVector<Item> items;
  static const CaptureKind kindOrder[] = {CaptureKind::Region,
                                          CaptureKind::Fullscreen};
  for (int index = 0; index < 2; ++index)
    items.push_back(
        {true, index, textWidth(captureTabLabel(kindOrder[index]))});
  static const CaptureTiming timingOrder[] = {CaptureTiming::Instant,
                                              CaptureTiming::Delayed};
  const int timingStart = items.size();
  for (int index = 0; index < 2; ++index)
    items.push_back(
        {false, index, textWidth(timingTabLabel(timingOrder[index]))});
  // The value cell is measured at the widest supported value so stepping
  // the seconds never re-centers the bar under the pointer.
  static_cast<void>(delaySecs);
  qreal stepperWidth = 0.0;
  qreal valueWidth = 0.0;
  if (timing == CaptureTiming::Delayed) {
    valueWidth = metrics.horizontalAdvance(
                     delayValueLabel(kCaptureDelayMaxSecs)) +
                 2 * kValuePad;
    stepperWidth = kStepperButton + kGap + valueWidth + kGap + kStepperButton;
  }
  qreal total = 0.0;
  for (const Item &item : items)
    total += item.width + kGap;
  total -= kGap;
  // One wider gap separates the kind and timing groups (replacing the plain
  // gap there), plus another before the stepper when it is shown.
  total += kGroupGap;
  if (timing == CaptureTiming::Delayed)
    total += kGroupGap + stepperWidth;
  // The timing group follows the kind group after one wider gap.
  qreal left = bounds.left() + (bounds.width() - total) / 2.0;
  qreal x = left;
  for (int pos = 0; pos < items.size(); ++pos) {
    const Item &item = items.at(pos);
    if (pos == timingStart)
      x += kGroupGap;
    const QRectF rect(x, kTop, item.width, kHeight);
    if (item.isKind)
      bar.kinds.push_back({kindOrder[item.index], rect});
    else
      bar.timings.push_back({timingOrder[item.index], rect});
    x += item.width + kGap;
  }
  if (timing == CaptureTiming::Delayed) {
    x -= kGap; // drop the trailing inter-item gap, then take a group gap
    x += kGroupGap;
    bar.minusRect = QRectF(x, kTop, kStepperButton, kHeight);
    x += kStepperButton + kGap;
    bar.valueRect = QRectF(x, kTop, valueWidth, kHeight);
    x += valueWidth + kGap;
    bar.plusRect = QRectF(x, kTop, kStepperButton, kHeight);
    x += kStepperButton + kGap;
  }
  QRectF united;
  bool first = true;
  const auto include = [&](const QRectF &rect) {
    if (rect.isEmpty())
      return;
    united = first ? rect : united.united(rect);
    first = false;
  };
  for (const CaptureTab &tab : bar.kinds)
    include(tab.rect);
  for (const TimingTab &tab : bar.timings)
    include(tab.rect);
  include(bar.minusRect);
  include(bar.valueRect);
  include(bar.plusRect);
  bar.barRect = united.adjusted(-5, -30, 5, 5);
  return bar;
}

int captureBarKindAt(const CaptureBarLayout &bar, const QPointF &position) {
  for (int index = 0; index < bar.kinds.size(); ++index) {
    if (bar.kinds.at(index).rect.adjusted(-2, -6, 2, 6).contains(position))
      return index;
  }
  return -1;
}

int captureBarTimingAt(const CaptureBarLayout &bar, const QPointF &position) {
  for (int index = 0; index < bar.timings.size(); ++index) {
    if (bar.timings.at(index).rect.adjusted(-2, -6, 2, 6).contains(position))
      return index;
  }
  return -1;
}

DelayStepperButton captureBarStepperAt(const CaptureBarLayout &bar,
                                       const QPointF &position) {
  if (!bar.minusRect.isEmpty() &&
      bar.minusRect.adjusted(-2, -6, 2, 6).contains(position))
    return DelayStepperButton::Minus;
  if (!bar.plusRect.isEmpty() &&
      bar.plusRect.adjusted(-2, -6, 2, 6).contains(position))
    return DelayStepperButton::Plus;
  return DelayStepperButton::None;
}

void drawCaptureBar(QPainter &painter, const CaptureBarLayout &bar,
                    CaptureKind activeKind, CaptureTiming activeTiming,
                    int delaySecs, const QPointF &cursor) {
  if (bar.kinds.isEmpty())
    return;
  const ThemeColors &theme = currentThemeColors();
  painter.setPen(QPen(withAlpha(theme.onSurface, 32), 1));
  painter.setBrush(withAlpha(theme.surface, 235));
  painter.drawRoundedRect(bar.barRect, 12, 12);
  painter.setFont(captureTabFont());
  const int hoveredKind = captureBarKindAt(bar, cursor);
  for (int index = 0; index < bar.kinds.size(); ++index) {
    const CaptureTab &tab = bar.kinds.at(index);
    painter.setPen(Qt::NoPen);
    if (tab.kind == activeKind) {
      painter.setBrush(captureTabAccent(tab.kind));
      painter.drawRoundedRect(tab.rect, 9, 9);
      painter.setPen(theme.onAccent);
    } else {
      if (index == hoveredKind) {
        painter.setBrush(withAlpha(theme.onSurface, 28));
        painter.drawRoundedRect(tab.rect, 9, 9);
      }
      painter.setPen(withAlpha(theme.onSurface, index == hoveredKind ? 255 : 190));
    }
    painter.drawText(tab.rect, Qt::AlignCenter, captureTabLabel(tab.kind));
  }
  // Group separator between the kind tabs and the timing toggle.
  if (!bar.kinds.isEmpty() && !bar.timings.isEmpty()) {
    const qreal left = bar.kinds.constLast().rect.right();
    const qreal right = bar.timings.constFirst().rect.left();
    const qreal x = (left + right) / 2.0;
    const qreal top = bar.kinds.constFirst().rect.top() + 5.0;
    const qreal bottom = bar.kinds.constFirst().rect.bottom() - 5.0;
    painter.setPen(QPen(withAlpha(theme.onSurface, 48), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(QPointF(x, top), QPointF(x, bottom));
  }
  const int hoveredTiming = captureBarTimingAt(bar, cursor);
  for (int index = 0; index < bar.timings.size(); ++index) {
    const TimingTab &tab = bar.timings.at(index);
    painter.setPen(Qt::NoPen);
    if (tab.timing == activeTiming) {
      painter.setBrush(captureTabAccent(CaptureKind::Region));
      painter.drawRoundedRect(tab.rect, 9, 9);
      painter.setPen(theme.onAccent);
    } else {
      if (index == hoveredTiming) {
        painter.setBrush(withAlpha(theme.onSurface, 28));
        painter.drawRoundedRect(tab.rect, 9, 9);
      }
      painter.setPen(
          withAlpha(theme.onSurface, index == hoveredTiming ? 255 : 190));
    }
    painter.drawText(tab.rect, Qt::AlignCenter, timingTabLabel(tab.timing));
  }
  if (bar.minusRect.isEmpty())
    return;
  const DelayStepperButton hoveredStepper = captureBarStepperAt(bar, cursor);
  const auto drawStepper = [&](const QRectF &rect, const QString &glyph,
                               DelayStepperButton button) {
    if (hoveredStepper == button) {
      painter.setPen(Qt::NoPen);
      painter.setBrush(withAlpha(theme.onSurface, 28));
      painter.drawRoundedRect(rect, 9, 9);
    }
    painter.setPen(withAlpha(theme.onSurface,
                             hoveredStepper == button ? 255 : 190));
    painter.drawText(rect, Qt::AlignCenter, glyph);
  };
  drawStepper(bar.minusRect, QStringLiteral("–"), DelayStepperButton::Minus);
  painter.setPen(withAlpha(theme.onSurface, 190));
  painter.drawText(bar.valueRect, Qt::AlignCenter,
                   delayValueLabel(delaySecs));
  drawStepper(bar.plusRect, QStringLiteral("+"), DelayStepperButton::Plus);
}

QRectF drawModeBadge(QPainter &painter, const QRect &bounds,
                     const QString &label, const QColor &accent,
                     QRectF *closeRect) {
  QFont badgeFont(QStringLiteral("Noto Sans"));
  badgeFont.setBold(true);
  badgeFont.setPixelSize(11);
  painter.setFont(badgeFont);
  const QString badge = label + QStringLiteral("  ×");
  const int badgeWidth = painter.fontMetrics().horizontalAdvance(badge) + 24;
  const QRectF badgeRect((bounds.width() - badgeWidth) / 2.0, 12, badgeWidth,
                         32);
  const ThemeColors &theme = currentThemeColors();
  painter.setPen(QPen(withAlpha(theme.onSurface, 32), 1));
  painter.setBrush(withAlpha(theme.surface, 235));
  painter.drawRoundedRect(badgeRect, 10, 10);
  painter.setPen(accent);
  painter.drawText(badgeRect, Qt::AlignCenter, badge);
  if (closeRect) {
    // The × and a little around it, so the click that closes has a target
    // rather than a pixel.
    const qreal closeWidth =
        painter.fontMetrics().horizontalAdvance(QStringLiteral("×")) + 18;
    *closeRect = QRectF(badgeRect.right() - closeWidth, badgeRect.top(),
                        closeWidth, badgeRect.height());
  }
  return badgeRect;
}

void drawHotkeyLegend(QPainter &painter, const QRect &bounds,
                      const QVector<QPair<QString, QString>> &entries) {
  if (entries.isEmpty())
    return;
  const QFont font = chromeFont(11);
  const QFontMetricsF metrics(font);
  constexpr qreal keyGap = 10;    // between a key and what it does
  constexpr qreal marginLeft = 14;
  constexpr qreal marginBottom = 14;
  constexpr qreal rowHeight = 17;
  qreal keyWidth = 0.0;
  for (const auto &entry : entries)
    keyWidth = std::max(keyWidth, metrics.horizontalAdvance(entry.first));
  painter.setFont(font);
  // Bottom-left, growing upward: entry 0 is the bottom-most row. No card, no
  // border, no dodging the pointer or anything else — the caller draws this
  // early, so real chrome painted afterward simply covers it where the two
  // overlap, and the low opacity keeps it out of the way where nothing does.
  for (int index = 0; index < entries.size(); ++index) {
    const qreal y =
        bounds.height() - marginBottom - (index + 1) * rowHeight;
    const ThemeColors &theme = currentThemeColors();
    painter.setPen(withAlpha(theme.onSurface, 165));
    painter.drawText(QRectF(marginLeft, y, keyWidth, rowHeight - 2),
                     Qt::AlignLeft | Qt::AlignVCenter, entries.at(index).first);
    painter.setPen(withAlpha(theme.onSurface, 130));
    painter.drawText(
        QRectF(marginLeft + keyWidth + keyGap, y,
               bounds.width() - marginLeft - keyWidth - keyGap - 14,
               rowHeight - 2),
        Qt::AlignLeft | Qt::AlignVCenter, entries.at(index).second);
  }
}

void drawStatusPill(QPainter &painter, const QRect &bounds,
                    const QString &text) {
  QFont font(QStringLiteral("Noto Sans"));
  font.setPixelSize(13);
  painter.setFont(font);
  const int width = painter.fontMetrics().horizontalAdvance(text) + 28;
  const QRectF pill((bounds.width() - width) / 2.0, bounds.height() - 42.0,
                    width, 30);
  const ThemeColors &theme = currentThemeColors();
  painter.setPen(QPen(withAlpha(theme.onSurface, 32), 1));
  painter.setBrush(withAlpha(theme.surface, 232));
  painter.drawRoundedRect(pill, 10, 10);
  painter.setPen(theme.onSurface);
  painter.drawText(pill, Qt::AlignCenter, text);
}
