/** @fileoverview Tests theme-color parsing: Noctalia CSS roles, Niri KDL
 *  focus-ring fallback, and defaults when nothing is readable. */
#include "theme-colors-smoke.hpp"

#include "theme-colors.hpp"

bool runThemeColorsSmoke(QString &error) {
  const ThemeColors defaults = defaultThemeColors();
  if (defaults.accent != QColor(QStringLiteral("#0a84ff")) ||
      defaults.surface != QColor(18, 18, 22) ||
      !defaults.onSurface.isValid() || !defaults.onAccent.isValid() ||
      !defaults.destructive.isValid()) {
    error = QStringLiteral("default theme colors changed unexpectedly");
    return false;
  }

  // Noctalia GTK CSS: semantic roles map onto chrome roles.
  ThemeColors css = defaults;
  const QString gtkCss =
      QStringLiteral("@define-color accent_color #7aa2f7;\n"
                     "@define-color accent_fg_color #16161e;\n"
                     "@define-color window_bg_color #1a1b26;\n"
                     "@define-color window_fg_color #c0caf5;\n"
                     "@define-color destructive_bg_color #f7768e;\n");
  if (!parseNoctaliaGtkCss(gtkCss, css)) {
    error = QStringLiteral("noctalia CSS accent not detected");
    return false;
  }
  if (css.accent != QColor(QStringLiteral("#7aa2f7")) ||
      css.onAccent != QColor(QStringLiteral("#16161e")) ||
      css.surface != QColor(QStringLiteral("#1a1b26")) ||
      css.onSurface != QColor(QStringLiteral("#c0caf5")) ||
      css.destructive != QColor(QStringLiteral("#f7768e"))) {
    error = QStringLiteral("noctalia CSS roles misapplied");
    return false;
  }

  // Non-color references and garbage never poison the result.
  ThemeColors dirty = defaults;
  if (parseNoctaliaGtkCss(
          QStringLiteral("@define-color accent_color @window_bg_color;\n"
                         "@define-color window_bg_color nonsense;\n"),
          dirty)) {
    error = QStringLiteral("invalid CSS values reported as accent");
    return false;
  }
  if (dirty.accent != defaults.accent || dirty.surface != defaults.surface) {
    error = QStringLiteral("invalid CSS values overwrote defaults");
    return false;
  }

  // Niri KDL: last focus-ring active-color wins (fragment + local override).
  // `inactive-color` must not match as `active-color`.
  const QColor single = parseNiriFocusRingAccent(
      QStringLiteral("layout {\n focus-ring {\n width 1\n active-color "
                     "\"#7aa2f7\"\n inactive-color \"#1a1b26\"\n urgent-color "
                     "\"#f7768e\"\n }\n}\n"));
  if (single != QColor(QStringLiteral("#7aa2f7"))) {
    error = QStringLiteral("niri focus-ring accent not parsed");
    return false;
  }
  const QColor last = parseNiriFocusRingAccent(
      QStringLiteral("focus-ring { active-color \"#7aa2f7\" }\n"
                     "focus-ring { width 1 active-color \"#565f89\" }\n"));
  if (last != QColor(QStringLiteral("#565f89"))) {
    error = QStringLiteral("niri focus-ring override order wrong");
    return false;
  }
  if (parseNiriFocusRingAccent(QStringLiteral("layout { gaps 8 }\n")).isValid()) {
    error = QStringLiteral("missing niri accent reported valid");
    return false;
  }

  // Resolution order: Noctalia first, Niri accent fallback, defaults last.
  const ThemeColors viaCss = resolveThemeColors(
      gtkCss, QStringLiteral("focus-ring { active-color \"#565f89\" }\n"));
  if (viaCss.accent != QColor(QStringLiteral("#7aa2f7"))) {
    error = QStringLiteral("noctalia CSS should beat the niri fallback");
    return false;
  }
  const ThemeColors viaNiri = resolveThemeColors(
      {}, QStringLiteral("focus-ring { active-color \"#565f89\" }\n"));
  if (viaNiri.accent != QColor(QStringLiteral("#565f89")) ||
      viaNiri.surface != defaults.surface ||
      viaNiri.onSurface != defaults.onSurface) {
    error = QStringLiteral("niri fallback should theme the accent only");
    return false;
  }
  const ThemeColors empty = resolveThemeColors({}, {});
  if (empty.accent != defaults.accent || empty.surface != defaults.surface ||
      empty.onSurface != defaults.onSurface) {
    error = QStringLiteral("empty inputs should keep defaults");
    return false;
  }
  return true;
}
