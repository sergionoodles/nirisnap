/** @fileoverview Provides pure pinned-window layout helpers. */
#pragma once

#include <QPoint>
#include <QRect>
#include <QSize>

[[nodiscard]] QPoint pinPositionFromGlobalPointer(const QPoint &globalPointer,
                                                  const QPoint &screenOrigin,
                                                  const QPoint &pressOffset);
[[nodiscard]] QPoint pinSlotPosition(const QSize &screenSize,
                                     const QSize &pinSize,
                                     const QSize &slotSize, int index, int gap,
                                     int margin);
[[nodiscard]] QRect clampPinGeometry(const QRect &pin, const QRect &bounds);
