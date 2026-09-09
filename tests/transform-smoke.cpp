/** @fileoverview Tests Niri capture geometry and object cleanup. */
#include "transform-smoke.hpp"

#include "capture.hpp"
#include "surface-capture-smoke.hpp"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QVector>

#include <wayland-client-protocol.h>

namespace {
/** Writes an executable helper used to replace an external command. */
bool writeExecutable(const QString &path, const QByteArray &contents) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly) ||
      file.write(contents) != contents.size())
    return false;
  file.close();
  return QFile::setPermissions(path, QFileDevice::ReadOwner |
                                         QFileDevice::WriteOwner |
                                         QFileDevice::ExeOwner);
}

/** Creates a small image whose red channel stores easy-to-check values. */
QImage indexedImage(const QVector<QVector<int>> &rows) {
  QImage image(rows.constFirst().size(), rows.size(), QImage::Format_RGB32);
  for (int y = 0; y < rows.size(); ++y) {
    for (int x = 0; x < rows.at(y).size(); ++x)
      image.setPixelColor(x, y, QColor(rows.at(y).at(x), 0, 0));
  }
  return image;
}
} // namespace

bool runTransformSmoke(QString &error) {
  if (!runWaylandCleanupChecks()) {
    error =
        QStringLiteral("Wayland capture objects were not released in order");
    return false;
  }
  QTemporaryDir fakeCommands;
  if (!fakeCommands.isValid()) {
    error = QStringLiteral("Could not create transform-test directory");
    return false;
  }
  const QString fakeNiri =
      QDir(fakeCommands.path()).filePath(QStringLiteral("niri"));
  const QByteArray niriScript = QByteArrayLiteral(
      "#!/usr/bin/env bash\n"
      "set -euo pipefail\n"
      "if [[ \"${1:-}\" == \"msg\" ]]; then\n"
      "  shift\n"
      "  if [[ \"${1:-}\" == \"-j\" ]]; then shift; fi\n"
      "  if [[ \"${1:-}\" == \"focused-output\" ]]; then\n"
      "    printf '{\"name\":\"TEST-ROTATED\",\"modes\":[{\"width\":300,"
      "\"height\":200}],\"current_mode\":0,\"logical\":{\"x\":0,\"y\":0,"
      "\"width\":300,\"height\":200,\"scale\":1.0,\"transform\":\"%s\"}}' "
      "\"$NIRISNAP_TEST_TRANSFORM\"\n"
      "  elif [[ \"${1:-}\" == \"outputs\" ]]; then\n"
      "    printf '{\"TEST-ROTATED\":{\"name\":\"TEST-ROTATED\",\"modes\":[{"
      "\"width\":300,\"height\":200}],\"current_mode\":0,\"logical\":{\"x\":0,"
      "\"y\":0,\"width\":300,\"height\":200,\"scale\":1.0,\"transform\":\"%s\""
      "}}}' \"$NIRISNAP_TEST_TRANSFORM\" \"$NIRISNAP_TEST_TRANSFORM\"\n"
      "  else\n"
      "    printf '[]\\n'\n"
      "  fi\n"
      "else\n"
      "  printf '{}\\n'\n"
      "fi\n");
  if (!writeExecutable(fakeNiri, niriScript)) {
    error = QStringLiteral("Could not create transform-test commands");
    return false;
  }
  QImage rotatedSource(300, 200, QImage::Format_RGB32);
  rotatedSource.fill(QColor(QStringLiteral("#123456")));
  const QString capturePath =
      QDir(fakeCommands.path()).filePath(QStringLiteral("source.png"));
  if (!rotatedSource.save(capturePath, "PNG")) {
    error = QStringLiteral("Could not create transform-test capture");
    return false;
  }

  const QByteArray originalPath = qgetenv("PATH");
  qputenv("PATH", fakeCommands.path().toUtf8() + ':' + originalPath);
  qputenv("NIRISNAP_TEST_CAPTURE", capturePath.toUtf8());
  const auto restoreEnvironment = [&originalPath] {
    qputenv("PATH", originalPath);
    qunsetenv("NIRISNAP_TEST_CAPTURE");
    qunsetenv("NIRISNAP_TEST_TRANSFORM");
  };
  // Niri transform names that swap axes: 90/270 (+ flipped variants).
  for (const char *transform : {"90", "270", "Flipped-90", "Flipped-270"}) {
    qputenv("NIRISNAP_TEST_TRANSFORM", transform);
    CaptureData rotatedCapture;
    if (!captureFocusedMonitor(rotatedCapture, true, error) ||
        rotatedCapture.monitor.geometry.size() != QSize(200, 300) ||
        rotatedCapture.previewSize != QSize(200, 300) ||
        !rotatedCapture.windows.isEmpty()) {
      if (error.isEmpty())
        error = QStringLiteral("Quarter-turn monitor geometry was not swapped");
      restoreEnvironment();
      return false;
    }
  }
  for (const char *transform : {"Normal", "180", "Flipped", "Flipped-180"}) {
    qputenv("NIRISNAP_TEST_TRANSFORM", transform);
    CaptureData uprightCapture;
    if (!captureFocusedMonitor(uprightCapture, true, error) ||
        uprightCapture.monitor.geometry.size() != QSize(300, 200)) {
      if (error.isEmpty())
        error = QStringLiteral("Upright monitor geometry was incorrect");
      restoreEnvironment();
      return false;
    }
  }

  // Callers that never show the overlay skip window discovery entirely.
  // On Niri window discovery is always empty (no tiled coordinates).
  qputenv("NIRISNAP_TEST_TRANSFORM", QByteArrayLiteral("Normal"));
  CaptureData withoutWindows;
  if (!captureFocusedMonitor(withoutWindows, false, error) ||
      withoutWindows.source.size() != QSize(300, 200) ||
      withoutWindows.previewSize != QSize(300, 200) ||
      !withoutWindows.windows.isEmpty()) {
    if (error.isEmpty())
      error = QStringLiteral("Capture without window discovery was incorrect");
    restoreEnvironment();
    return false;
  }

  // Without a test capture, missing output-capture protocol must fail clearly.
  qunsetenv("NIRISNAP_TEST_CAPTURE");
  CaptureData missingProtocol;
  QString protocolError;
  const bool captured =
      captureFocusedMonitor(missingProtocol, false, protocolError);
  if (captured || protocolError.isEmpty()) {
    error = QStringLiteral("Missing output capture did not report an error");
    restoreEnvironment();
    return false;
  }

  restoreEnvironment();

  const QImage upright = indexedImage({{1, 2}, {3, 4}, {5, 6}});
  const QVector<QPair<std::uint32_t, QImage>> transformedImages{
      {WL_OUTPUT_TRANSFORM_NORMAL, upright},
      {WL_OUTPUT_TRANSFORM_90, indexedImage({{2, 4, 6}, {1, 3, 5}})},
      {WL_OUTPUT_TRANSFORM_180, indexedImage({{6, 5}, {4, 3}, {2, 1}})},
      {WL_OUTPUT_TRANSFORM_270, indexedImage({{5, 3, 1}, {6, 4, 2}})},
      {WL_OUTPUT_TRANSFORM_FLIPPED, indexedImage({{2, 1}, {4, 3}, {6, 5}})},
      {WL_OUTPUT_TRANSFORM_FLIPPED_90, indexedImage({{1, 3, 5}, {2, 4, 6}})},
      {WL_OUTPUT_TRANSFORM_FLIPPED_180, indexedImage({{5, 6}, {3, 4}, {1, 2}})},
      {WL_OUTPUT_TRANSFORM_FLIPPED_270, indexedImage({{6, 4, 2}, {5, 3, 1}})},
  };
  for (const auto &[transform, transformed] : transformedImages) {
    if (normalizeWaylandCapture(transformed, transform) != upright) {
      error = QStringLiteral("Captured Wayland buffer was not upright");
      return false;
    }
  }
  return true;
}
