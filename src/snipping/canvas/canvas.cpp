#include "canvas.h"

namespace canvas
{
    Canvas::Canvas(QObject *parent)
        : QGraphicsScene(parent)
    {}

    GraphicsItemWrapper *Canvas::focusItem() const
    {
        if (auto item = QGraphicsScene::focusItem(); item) return dynamic_cast<GraphicsItemWrapper *>(item);

        return nullptr;
    }

    GraphicsItemWrapper *Canvas::focusOrFirstSelectedItem() const
    {
        if (auto item = QGraphicsScene::focusItem(); item) return dynamic_cast<GraphicsItemWrapper *>(item);

        if (auto selected = selectedItems(); !selected.isEmpty()) {
            return dynamic_cast<GraphicsItemWrapper *>(selected.first());
        }

        return nullptr;
    }

    void Canvas::focusOutEvent(QFocusEvent *event)
    {
        clearFocus();
        clearSelection();
        QGraphicsScene::focusOutEvent(event);
    }

} // namespace canvas