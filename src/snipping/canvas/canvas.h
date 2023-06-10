#ifndef CAPTURER_CANVAS_H
#define CAPTURER_CANVAS_H

#include "graphicsitems.h"

#include <QGraphicsScene>

namespace canvas
{
    class Canvas : public QGraphicsScene
    {
    public:
        Canvas(QObject * = nullptr);

        GraphicsItemWrapper *focusItem() const;

        GraphicsItemWrapper *focusOrFirstSelectedItem() const;

        void add(GraphicsItemWrapper *item);

        void remove(GraphicsItemWrapper *item);

    public slots:
        void clear();
    };
} // namespace canvas

#endif //! CAPTURER_CANVAS_H