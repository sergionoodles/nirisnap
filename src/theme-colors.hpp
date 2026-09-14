/** @fileoverview Overlay chrome theme colors.

Reads the desktop theme from plain files — never from QStyle, QPalette, or a
platform-theme plugin, which stay bypassed (see docs/dependencies.md):

1. Noctalia's generated GTK CSS (`~/.config/gtk-4.0/noctalia.css`), which
   carries semantic roles (accent, surface, text, destructive) and is
   regenerated on every theme switch.
2. Fallback: the Niri `focus-ring { active-color ... }` from the
   Noctalia-generated fragment (`~/.config/niri/noctalia.kdl`) and the local
   override (`~/.config/niri/cfg/layout.kdl`, last wins so user overrides
   apply). Only the accent comes from here; surface/text stay default.

When neither yields colors, the built-in dark defaults (the previous
hardcoded chrome) apply. The process is short-lived, so colors are resolved
once per launch and cached; there is no live reload. */
#pragma once

#include <QColor>
#include <QString>

/// Accent + surface roles used by overlay chrome (tab strip, toolbar,
// pills, selection rings). Annotation preset colors are intentionally
// separate (see palette-config.hpp) and never themed.
struct ThemeColors {
  QColor accent;
  QColor onAccent;
  QColor surface;
  QColor onSurface;
  QColor destructive;
};

/// The previous hardcoded chrome, used when no theme file is readable.
[[nodiscard]] ThemeColors defaultThemeColors();

/// Parse `@define-color <name> <value>;` entries from Noctalia GTK CSS.
/// Returns true when at least the accent role was found; `out` starts from
/// `defaultThemeColors()` so partial files keep defaults for missing roles.
[[nodiscard]] bool parseNoctaliaGtkCss(const QString &content, ThemeColors &out);

/// Extract the last `focus-ring { ... active-color "..." }` from KDL text
/// (later definitions win, mirroring include-override order). Returns an
/// invalid color when absent.
[[nodiscard]] QColor parseNiriFocusRingAccent(const QString &content);

/// Resolve paths honoring XDG_CONFIG_HOME (falling back to ~/.config).
[[nodiscard]] QString themeGtkCssPath();
[[nodiscard]] QString themeNiriFragmentPath();
[[nodiscard]] QString themeNiriLayoutPath();

/// Load from the real filesystem: Noctalia CSS first, Niri accent fallback,
/// defaults last. Pure-content overload below exists for tests.
[[nodiscard]] ThemeColors loadThemeColors();
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
[[nodiscard]] ThemeColors resolveThemeColors(const QString &gtkCssContent,
                                             const QString &niriKdlContent);

/// Process-wide cached result of loadThemeColors(). Safe to call from paint
/// code; file I/O happens once on first call.
[[nodiscard]] const ThemeColors &currentThemeColors();

/// Opaque `color` with its alpha replaced (theme roles are opaque; chrome
/// applies its own translucency on top).
[[nodiscard]] inline QColor withAlpha(const QColor &color, int alpha) {
  QColor out(color);
  out.setAlpha(alpha);
  return out;
}
