/** @fileoverview The chrome every full-screen overlay wears: the mode badge at
 *  the top, the hotkey guide in the corner, and the status pill along the
 *  bottom. */
#pragma once

#include <QColor>
#include <QPair>
#include <QRect>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

class QFont;
class QPainter;

/// The typeface for overlay chrome text (toolbar labels, hotkey legend,
/// tooltips): pinned in code, never the platform theme's system font, so
/// startup does not depend on a theme plugin and the look is the same on
/// every install. Adwaita Sans is a common system UI font; Noto Sans is
/// the fallback the tab strip already uses.
[[nodiscard]] QFont chromeFont(int pixelSize, bool bold = false);
/// The application-wide default font, installed by main() before any widget
/// exists: the same face and 11 pt size the gtk3 platform theme used to
/// supply, so text drawn with a painter's or widget's default font (pin
/// tips, overlay buttons) does not shrink or change family now that
/// the external desktop theme is bypassed.
[[nodiscard]] QFont chromeDefaultFont();
/// Monospace counterpart for numeric readouts: pinned stack headed by the
/// bundled JetBrains Mono, so the face never depends on what the `monospace`
/// alias happens to resolve to on a given install.
[[nodiscard]] QFont chromeMonoFont(int pixelSize, bool bold = false);

/// The kinds of capture the tab strip across the top offers. Region is
/// freeform selection; Fullscreen acts at once.
enum class CaptureKind { Region, Fullscreen };
struct CaptureTab {
  CaptureKind kind;
  QRectF rect;
};
/// When the capture happens: right away, or after a countdown so transient
/// UI (menus, hovers) can be arranged while the overlay is out of the way.
/// Instant is the default and preserves the existing behavior.
enum class CaptureTiming { Instant, Delayed };
struct TimingTab {
  CaptureTiming timing;
  QRectF rect;
};
/// Bounds for the delay stepper, in seconds. The default 3 s matches the
/// overlay's initial state; the editor clamps interactive edits into range.
constexpr int kCaptureDelayDefaultSecs = 3;
constexpr int kCaptureDelayMinSecs = 1;
constexpr int kCaptureDelayMaxSecs = 60;
[[nodiscard]] QString timingTabLabel(CaptureTiming timing);
/// Which stepper button is under a point, if any.
enum class DelayStepperButton { None, Minus, Plus };
/// Every clickable rect of the select-phase top bar: the capture-kind tabs,
/// the timing toggle, and (when delayed) the -/value/+ stepper. `barRect` is
/// the background uniting them. In the edit phase only `kinds` is used.
struct CaptureBarLayout {
  QVector<CaptureTab> kinds;
  QVector<TimingTab> timings;
  QRectF minusRect;
  QRectF valueRect;
  QRectF plusRect;
  QRectF barRect;
};
/// Bar positions for a surface of `bounds`. The stepper is present only when
/// `timing` is Delayed; its value cell is measured at the widest supported
/// value so tuning the seconds never shifts the bar.
[[nodiscard]] CaptureBarLayout
captureBarLayout(const QRect &bounds, CaptureTiming timing, int delaySecs);
/// Index of the kind tab under `position`, or -1.
[[nodiscard]] int captureBarKindAt(const CaptureBarLayout &bar,
                                   const QPointF &position);
/// Index of the timing tab under `position`, or -1.
[[nodiscard]] int captureBarTimingAt(const CaptureBarLayout &bar,
                                     const QPointF &position);
/// Stepper button under `position`, or None.
[[nodiscard]] DelayStepperButton
captureBarStepperAt(const CaptureBarLayout &bar, const QPointF &position);
/// Draws the whole strip; the active kind and timing are lit, the item under
/// `cursor` is hinted.
void drawCaptureBar(QPainter &painter, const CaptureBarLayout &bar,
                    CaptureKind activeKind, CaptureTiming activeTiming,
                    int delaySecs, const QPointF &cursor);
/// Visible height of the tab strip's background, from the top edge (the
/// strip is flush against it) to its rounded bottom — fixed regardless of
/// window size, since only the horizontal layout changes with the surface.
/// Chrome stacked below the strip anchors to this, not a guessed constant.
constexpr qreal kCaptureTabBarBottom = 31.0;
[[nodiscard]] QString captureTabLabel(CaptureKind kind);
/// Tab positions for a surface of `bounds`, hanging off the top edge.
[[nodiscard]] QVector<CaptureTab> captureTabLayout(const QRect &bounds);
/// Index of the tab under `position`, or -1.
[[nodiscard]] int captureTabAt(const QVector<CaptureTab> &tabs,
                               const QPointF &position);
/// Draws the strip; `active` is lit, the tab under `cursor` is hinted.
void drawCaptureTabs(QPainter &painter, const QVector<CaptureTab> &tabs,
                     CaptureKind active, const QPointF &cursor);

/// The badge naming what the overlay is doing, centered at the top, with the ×
/// that leaves it. Returns the whole badge; `closeRect` is the × alone, for
/// hit-testing the click that closes.
QRectF drawModeBadge(QPainter &painter, const QRect &bounds,
                     const QString &label, const QColor &accent,
                     QRectF *closeRect = nullptr);

/// A single, backgroundless column of `key  action` pairs along the bottom
/// left, growing upward, in low-opacity text. Hotkeys are a reference, not
/// UI: there is no card, no border, and no attempt to dodge the pointer or
/// dodge anything else — draw it early (right after the overlay's initial
/// dim fill, before the image, the tab strip, the toolbar, any popup) and
/// normal paint order does the rest, since whatever is drawn afterward
/// simply covers it wherever the two overlap.
void drawHotkeyLegend(QPainter &painter, const QRect &bounds,
                      const QVector<QPair<QString, QString>> &entries);

/// The instruction line along the bottom.
void drawStatusPill(QPainter &painter, const QRect &bounds,
                    const QString &text);
