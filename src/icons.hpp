#pragma once

class QColor;
class QRectF;
class QString;
class QPainter;

void drawToolbarIcon(QPainter &painter, const QRectF &bounds,
                     const QString &action, const QString &label,
                     const QColor &color);
