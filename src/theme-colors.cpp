/** @fileoverview Theme file readers (see theme-colors.hpp). */
#include "theme-colors.hpp"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QRegularExpression>

namespace {

QColor colorFromCssValue(QString value) {
  value = value.trimmed();
  // Strip "!important" and trailing whitespace the template never emits but
  // a hand edit might.
  value.remove(QStringLiteral("!important"));
  value = value.trimmed();
  // GtkCSS may reference another role ("@window_bg_color"); only concrete
  // colors are usable here.
  if (value.startsWith(u'@'))
    return {};
  const QColor color(value);
  return color.isValid() ? color : QColor();
}

void applyCssRole(const QString &name, const QColor &color, ThemeColors &out) {
  if (!color.isValid())
    return;
  if (name == QStringLiteral("accent_color") ||
      name == QStringLiteral("accent_bg_color"))
    out.accent = color;
  else if (name == QStringLiteral("accent_fg_color"))
    out.onAccent = color;
  else if (name == QStringLiteral("window_bg_color"))
    out.surface = color;
  else if (name == QStringLiteral("window_fg_color"))
    out.onSurface = color;
  else if (name == QStringLiteral("destructive_bg_color") ||
           name == QStringLiteral("error_bg_color"))
    out.destructive = color;
}

QString readTextFile(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return {};
  return QString::fromUtf8(file.readAll());
}

QString configHome() {
  const QByteArray xdg = qgetenv("XDG_CONFIG_HOME");
  if (!xdg.isEmpty())
    return QString::fromLocal8Bit(xdg);
  return QDir::homePath() + QStringLiteral("/.config");
}

} // namespace

ThemeColors defaultThemeColors() {
  ThemeColors out;
  out.accent = QColor(QStringLiteral("#0a84ff"));
  out.onAccent = QColor(18, 18, 22);
  out.surface = QColor(18, 18, 22);
  out.onSurface = QColor(QStringLiteral("#f5f5f7"));
  out.destructive = QColor(QStringLiteral("#ff375f"));
  return out;
}

bool parseNoctaliaGtkCss(const QString &content, ThemeColors &out) {
  static const QRegularExpression pattern(
      QStringLiteral("@define-color\\s+([A-Za-z_]+)\\s+([^;]+);"));
  bool foundAccent = false;
  QRegularExpressionMatchIterator it = pattern.globalMatch(content);
  while (it.hasNext()) {
    const QRegularExpressionMatch match = it.next();
    const QColor color = colorFromCssValue(match.capturedView(2).toString());
    if (!color.isValid())
      continue;
    const QString name = match.captured(1);
    if (name == QStringLiteral("accent_color") ||
        name == QStringLiteral("accent_bg_color"))
      foundAccent = true;
    applyCssRole(name, color, out);
  }
  return foundAccent;
}

QColor parseNiriFocusRingAccent(const QString &content) {
  // focus-ring blocks are flat (`width` + three colors); match up to the
  // closing brace so a later file's block naturally overrides an earlier one
  // when contents are concatenated in include order.
  static const QRegularExpression block(
      QStringLiteral("focus-ring\\s*\\{([^}]*)\\}"));
  static const QRegularExpression color(
      // Lookbehind keeps `inactive-color` from matching as `active-color`.
      QStringLiteral("(?<![A-Za-z-])active-color\\s+\"([^\"]+)\""));
  QColor last;
  QRegularExpressionMatchIterator blocks = block.globalMatch(content);
  while (blocks.hasNext()) {
    const QString body = blocks.next().captured(1);
    QRegularExpressionMatchIterator colors = color.globalMatch(body);
    while (colors.hasNext()) {
      const QColor candidate(colors.next().captured(1).trimmed());
      if (candidate.isValid())
        last = candidate;
    }
  }
  return last;
}

QString themeGtkCssPath() {
  return configHome() + QStringLiteral("/gtk-4.0/noctalia.css");
}

QString themeNiriFragmentPath() {
  return configHome() + QStringLiteral("/niri/noctalia.kdl");
}

QString themeNiriLayoutPath() {
  return configHome() + QStringLiteral("/niri/cfg/layout.kdl");
}

ThemeColors resolveThemeColors(const QString &gtkCssContent, // NOLINT(bugprone-easily-swappable-parameters)
                               const QString &niriKdlContent) {
  ThemeColors out = defaultThemeColors();
  if (!gtkCssContent.isEmpty() &&
      parseNoctaliaGtkCss(gtkCssContent, out))
    return out;
  // No usable Noctalia accent: fall back to the Niri focus-ring color for
  // the accent role only, keeping default surface/text.
  const QColor accent = parseNiriFocusRingAccent(niriKdlContent);
  if (accent.isValid())
    out.accent = accent;
  return out;
}

ThemeColors loadThemeColors() {
  const QString gtkCss = readTextFile(themeGtkCssPath());
  ThemeColors probe = defaultThemeColors();
  if (!gtkCss.isEmpty() && parseNoctaliaGtkCss(gtkCss, probe))
    return probe;
  // Concatenate fragment + local override so the last active-color wins.
  QString niriKdl = readTextFile(themeNiriFragmentPath());
  const QString layout = readTextFile(themeNiriLayoutPath());
  if (!layout.isEmpty()) {
    niriKdl += u'\n';
    niriKdl += layout;
  }
  return resolveThemeColors(gtkCss, niriKdl);
}

const ThemeColors &currentThemeColors() {
  static const ThemeColors cached = loadThemeColors();
  return cached;
}
