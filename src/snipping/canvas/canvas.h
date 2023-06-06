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

    protected:
        void focusOutEvent(QFocusEvent *) override;
    };
} // namespace canvas

#endif //! CAPTURER_CANVAS_H