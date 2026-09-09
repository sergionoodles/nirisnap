/** @fileoverview Maps displayed editor coordinates to source pixels. */
#pragma once

#include <QColor>
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QSizeF>

[[nodiscard]] QColor sampleSourceColor(const QImage &source,
                                       const QSizeF &previewSize,
                                       const QRectF &selection,
                                       const QRectF &displayRect,
                                       const QPointF &displayPoint);
