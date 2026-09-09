/** @fileoverview Band cut-out engine: removes a horizontal or vertical band
 *  from an image and collapses the gap (Snagit-style "cut out", ported from
 *  omapic). */
#include "cut.hpp"

#include <QPainter>
#include <algorithm>
#include <cstring>

QImage removeBand(const QImage &source, Qt::Orientation orientation,
                  int start, int end) {
  if (source.isNull())
    return source;
  if (orientation == Qt::Horizontal) {
    const int height = source.height();
    start = std::clamp(start, 0, height);
    end = std::clamp(end, start, height);
    const int band = end - start;
    if (band <= 0 || band >= height)
      return source;
    QImage out(source.width(), height - band, source.format());
    out.setDevicePixelRatio(source.devicePixelRatio());
    for (int y = 0; y < start; ++y)
      std::memcpy(out.scanLine(y), source.constScanLine(y),
                  source.bytesPerLine());
    for (int y = end; y < height; ++y)
      std::memcpy(out.scanLine(y - band), source.constScanLine(y),
                  source.bytesPerLine());
    return out;
  }
  const int width = source.width();
  start = std::clamp(start, 0, width);
  end = std::clamp(end, start, width);
  const int band = end - start;
  if (band <= 0 || band >= width)
    return source;
  QImage out(width - band, source.height(), source.format());
  out.setDevicePixelRatio(source.devicePixelRatio());
  QPainter painter(&out);
  painter.setCompositionMode(QPainter::CompositionMode_Source);
  painter.drawImage(QPoint(0, 0), source, QRect(0, 0, start, source.height()));
  painter.drawImage(QPoint(start, 0), source,
                    QRect(end, 0, width - end, source.height()));
  return out;
}

QImage composeCuts(const QImage &pristine, const QVector<CutOp> &cuts) {
  QImage image = pristine;
  for (const CutOp &cut : cuts)
    image = removeBand(image, cut.orientation, cut.sourceStart, cut.sourceEnd);
  return image;
}

QSize composedLogicalSize(QSize pristineLogical, const QVector<CutOp> &cuts) {
  for (const CutOp &cut : cuts) {
    const int band = cut.logicalEnd - cut.logicalStart;
    if (cut.orientation == Qt::Horizontal) {
      const int extent = pristineLogical.height();
      // Mirror removeBand()'s no-op guard so this can never disagree with
      // what composeCuts() actually produces: an empty or full-extent band
      // leaves the image unchanged.
      if (band <= 0 || band >= extent)
        continue;
      pristineLogical.setHeight(std::max(1, extent - band));
    } else {
      const int extent = pristineLogical.width();
      if (band <= 0 || band >= extent)
        continue;
      pristineLogical.setWidth(std::max(1, extent - band));
    }
  }
  return pristineLogical;
}

qreal shiftForCut(qreal value, qreal start, qreal end) {
  if (value <= start)
    return value;
  if (value >= end)
    return value - (end - start);
  return start;
}
