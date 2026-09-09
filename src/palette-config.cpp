/** @fileoverview Loads editor preset colors from the user's INI config. */
#include "palette-config.hpp"

#include <QSettings>
#include <QStringList>

PaletteConfig defaultPaletteConfig() {
  return {{QColor(QStringLiteral("#ff375f")), QColor(QStringLiteral("#ff9f0a")),
           QColor(QStringLiteral("#ffd60a")), QColor(QStringLiteral("#30d158")),
           QColor(QStringLiteral("#0a84ff")), QColor(QStringLiteral("#bf5af2")),
           QColor(QStringLiteral("#000000")), QColor(QStringLiteral("#ffffff"))},
          QColor(QStringLiteral("#ff375f"))};
}

PaletteConfig loadPaletteConfig(const QString &filePath) {
  PaletteConfig config = defaultPaletteConfig();
  QSettings settings(filePath, QSettings::IniFormat);
  const QStringList palette =
      settings.value(QStringLiteral("colors/palette")).toStringList();
  for (int index = 0;
       index < palette.size() && index < static_cast<int>(config.palette.size());
       ++index) {
    const QColor color(palette.at(index).trimmed());
    if (color.isValid())
      config.palette.at(static_cast<std::size_t>(index)) = color;
  }
  const QColor custom(
      settings.value(QStringLiteral("colors/custom")).toString().trimmed());
  if (custom.isValid())
    config.custom = custom;
  return config;
}
