/** @fileoverview Tests the cut engine: band removal, replay, coordinate
 *  shifting. */
#include "cut-smoke.hpp"

#include "cut.hpp"

#include <QImage>
#include <QVector>

namespace {
/** Creates a small image whose red channel stores easy-to-check values. */
QImage indexedImage(const QVector<QVector<int>> &rows) {
  QImage image(rows.constFirst().size(), rows.size(), QImage::Format_RGB32);
  for (int y = 0; y < rows.size(); ++y) {
    for (int x = 0; x < rows.at(y).size(); ++x)
      image.setPixelColor(x, y, QColor(rows.at(y).at(x), 0, 0));
  }
  return image;
}

int redAt(const QImage &image, int x, int y) {
  return image.pixelColor(x, y).red();
}
} // namespace

bool runCutSmoke(QString &error) {
  // 4x4 image, rows valued 10,20,30,40.
  const QImage source = indexedImage(
      {{10, 10, 10, 10}, {20, 20, 20, 20}, {30, 30, 30, 30}, {40, 40, 40, 40}});

  // Horizontal band removes rows [1,3): rows 20 and 30 vanish, 40 moves up.
  const QImage rows = removeBand(source, Qt::Horizontal, 1, 3);
  if (rows.size() != QSize(4, 2) || redAt(rows, 0, 0) != 10 ||
      redAt(rows, 0, 1) != 40) {
    error = QStringLiteral("horizontal removeBand produced wrong image");
    return false;
  }

  // Vertical band removes columns [0,2) from a column-indexed image.
  const QImage cols = removeBand(
      indexedImage({{1, 2, 3, 4}, {1, 2, 3, 4}}), Qt::Vertical, 0, 2);
  if (cols.size() != QSize(2, 2) || redAt(cols, 0, 0) != 3 ||
      redAt(cols, 1, 0) != 4) {
    error = QStringLiteral("vertical removeBand produced wrong image");
    return false;
  }

  // Out-of-range bounds clamp; empty and full-extent bands are no-ops.
  if (removeBand(source, Qt::Horizontal, -5, 1).height() != 3 ||
      removeBand(source, Qt::Horizontal, 2, 2) != source ||
      removeBand(source, Qt::Horizontal, 0, 99) != source) {
    error = QStringLiteral("removeBand bounds handling wrong");
    return false;
  }

  // Replay: two sequential cuts, each in the coordinates of its moment.
  // Cut A removes row 1 (value 20); cut B then removes row 1 of the
  // remaining 10,30,40 image (value 30) -> 10,40.
  const QVector<CutOp> cuts{{Qt::Horizontal, 1, 2, 1, 2},
                            {Qt::Horizontal, 1, 2, 1, 2}};
  const QImage replayed = composeCuts(source, cuts);
  if (replayed.size() != QSize(4, 2) || redAt(replayed, 0, 0) != 10 ||
      redAt(replayed, 0, 1) != 40) {
    error = QStringLiteral("composeCuts replay wrong");
    return false;
  }

  if (composedLogicalSize(QSize(100, 80), cuts) != QSize(100, 78)) {
    error = QStringLiteral("composedLogicalSize wrong");
    return false;
  }

  // A full-extent band is a no-op in removeBand(); composedLogicalSize must
  // agree and leave the size unchanged rather than shrinking it (which would
  // desync the preview size from the actual, unchanged pixels).
  const QVector<CutOp> fullExtentCut{{Qt::Horizontal, 0, 80, 0, 80}};
  if (composedLogicalSize(QSize(100, 80), fullExtentCut) != QSize(100, 80)) {
    error = QStringLiteral(
        "composedLogicalSize shrank on a full-extent band");
    return false;
  }

  // Coordinate shifting: before band unchanged, inside clamps to seam,
  // after shifts back by the band size.
  if (shiftForCut(3.0, 5.0, 10.0) != 3.0 ||
      shiftForCut(7.0, 5.0, 10.0) != 5.0 ||
      shiftForCut(12.0, 5.0, 10.0) != 7.0) {
    error = QStringLiteral("shiftForCut wrong");
    return false;
  }

  // Vertical band removal must preserve alpha: create ARGB32 image with
  // semi-transparent pixel, remove a vertical band, verify alpha is unchanged.
  QImage alphaSource(4, 2, QImage::Format_ARGB32);
  alphaSource.fill(QColor(0, 0, 0, 255)); // Opaque black
  alphaSource.setPixelColor(2, 0, QColor(255, 0, 0, 128)); // Semi-transparent red
  const QImage alphaCut = removeBand(alphaSource, Qt::Vertical, 0, 2);
  if (alphaCut.size() != QSize(2, 2) ||
      alphaCut.pixelColor(0, 0) != QColor(255, 0, 0, 128) ||
      alphaCut.pixelColor(0, 1) != QColor(0, 0, 0, 255)) {
    error = QStringLiteral("vertical removeBand alpha preservation failed");
    return false;
  }

  return true;
}
