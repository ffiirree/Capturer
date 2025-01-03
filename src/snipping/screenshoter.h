#ifndef CAPTURER_SCREEN_SHOTER_H
#define CAPTURER_SCREEN_SHOTER_H

#include "canvas/canvas.h"
#include "canvas/command.h"
#include "hunter.h"
#include "magnifier.h"
#include "menu/editing-menu.h"
#include "resizer.h"
#include "selector.h"

#include <QGraphicsView>
#include <QPixmap>

class ScreenShoter final : public QGraphicsView
{
    Q_OBJECT

public:
    explicit ScreenShoter(QWidget *parent = nullptr);

signals:
    void pinData(const std::shared_ptr<QMimeData>&);

    void saved(const QString& path);

public slots:
    void start();
    void exit();

    void repeat();

    void                       save();
    void                       copy();
    void                       pin();
    std::pair<QPixmap, QPoint> snip();

    void setStyle(const SelectorStyle& style);

    void moveMenu();

    void updateCursor(ResizerLocation);

    bool screenshot(const probe::geometry_t&);
    void setBackground(const QPixmap&, const probe::geometry_t&);

    void createItem(const QPointF& pos);
    void finishItem();

#ifdef Q_OS_LINUX
    void DbusScreenshotArrived(uint, const QVariantMap&);
#endif

protected:
    bool eventFilter(QObject *, QEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    void wheelEvent(QWheelEvent *) override;

    void keyPressEvent(QKeyEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;

private:
    void                 registerShortcuts();
    QBrush               mosaicBrush();
    [[nodiscard]] size_t history_index();

    Selector       *selector_{}; // Layer 1
    canvas::Canvas *scene_{};

    GraphicsItemWrapper *creating_item_{};
    int                  counter_{ 0 };

    EditingMenu *menu_{};      // editing menu
    Magnifier   *magnifier_{}; // magnifier

    // history
    std::vector<hunter::prey_t> history_{};
    size_t                      history_idx_{ 0 };

    QUndoStack *undo_stack_{};
};

#endif //! CAPTURER_SCREEN_SHOTER_H
