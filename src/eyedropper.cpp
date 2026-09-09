/** @fileoverview Samples opaque source colors from the displayed crop. */
#include "eyedropper.hpp"

#include <algorithm>
#include <cmath>

QColor sampleSourceColor(const QImage &source, const QSizeF &previewSize,
                         const QRectF &selection, const QRectF &displayRect,
                         const QPointF &displayPoint) {
  if (source.isNull() || previewSize.width() <= 0 || previewSize.height() <= 0 ||
      selection.isEmpty() || displayRect.isEmpty() || source.width() <= 0 ||
      source.height() <= 0)
    return {};
  const QRectF crop =
      selection.normalized().intersected(QRectF(QPointF(), previewSize));
  if (crop.isEmpty())
    return {};

  const qreal u = std::clamp(
      (displayPoint.x() - displayRect.left()) / displayRect.width(), 0.0, 1.0);
  const qreal v = std::clamp(
      (displayPoint.y() - displayRect.top()) / displayRect.height(), 0.0, 1.0);
  const qreal previewX =
      crop.left() + u * std::max(0.0, crop.width() - 1.0 / previewSize.width());
  const qreal previewY =
      crop.top() + v * std::max(0.0, crop.height() - 1.0 / previewSize.height());

  const qreal scaleX = source.width() / previewSize.width();
  const qreal scaleY = source.height() / previewSize.height();
  const int cropLeft = std::clamp(
      static_cast<int>(std::floor(crop.left() * scaleX)), 0, source.width() - 1);
  const int cropTop = std::clamp(
      static_cast<int>(std::floor(crop.top() * scaleY)), 0, source.height() - 1);
  const int cropRight = std::clamp(
      static_cast<int>(std::ceil(crop.right() * scaleX)) - 1, cropLeft,
      source.width() - 1);
  const int cropBottom = std::clamp(
      static_cast<int>(std::ceil(crop.bottom() * scaleY)) - 1, cropTop,
      source.height() - 1);

  const int x = std::clamp(static_cast<int>(std::floor(previewX * scaleX)),
                           cropLeft, cropRight);
  const int y = std::clamp(static_cast<int>(std::floor(previewY * scaleY)),
                           cropTop, cropBottom);
  const QColor sampled = source.pixelColor(x, y);
  return QColor(sampled.red(), sampled.green(), sampled.blue());
}
