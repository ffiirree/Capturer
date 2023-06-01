#ifndef SCREEN_SHOTER_H
#define SCREEN_SHOTER_H

#include "canvas/command.h"
#include "canvas/graphicsitems.h"
#include "circlecursor.h"
#include "config.h"
#include "hunter.h"
#include "resizer.h"

#include <QGraphicsView>
#include <QPixmap>
#include <QStandardPaths>
#include <QSystemTrayIcon>

class Selector;
class EditingMenu;
class Magnifier;

class ScreenShoter : public QGraphicsView
{
    Q_OBJECT

    enum EditStatus : uint32_t
    {
        NONE           = 0x0000'0000,
        READY          = 0x0001'0000,
        GRAPH_CREATING = 0x0010'0000,
        GRAPH_MOVING   = 0x0020'0000,
        GRAPH_RESIZING = 0x0040'0000,
        GRAPH_ROTATING = 0x0080'0000,

        READY_MASK     = 0x000f'0000,
        OPERATION_MASK = 0xfff0'0000,
        GRAPH_MASK     = 0x0000'ffff,

        ENABLE_BITMASK_OPERATORS()
    };

public:
    explicit ScreenShoter(QWidget *parent = nullptr);

signals:
    void focusOnGraph(canvas::graphics_t);
    void pinSnipped(const QPixmap& image, const QPoint& pos);
    void SHOW_MESSAGE(const QString& title, const QString& msg,
                      QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information, int msecs = 10'000);

public slots:
    void start();
    void exit();

    void save();
    void copy();
    void pin();
    std::pair<QPixmap, QPoint> snip();
    void save2clipboard(const QPixmap&, bool);

    void updateTheme();

    void moveMenu();

    void updateCursor(ResizerLocation);

protected:
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    void wheelEvent(QWheelEvent *) override;

    void keyPressEvent(QKeyEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;

private:
    void registerShortcuts();
    QBrush mosaicBrush();

    Selector *selector_{}; // Layer 1

    uint32_t editstatus_{};

    std::pair<GraphicsItemInterface *, canvas::graphics_t> creating_item_;
    int counter_{ 0 };

    EditingMenu *menu_{};    // editing menu
    Magnifier *magnifier_{}; // magnifier

    QPoint move_begin_{ 0, 0 };
    QPoint resize_begin_{ 0, 0 };

    CircleCursor circle_cursor_{ 20 };

    // history
    std::vector<hunter::prey_t> history_;
    size_t history_idx_{ 0 };

    QString save_path_{ QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) };

    //
    std::pair<QGraphicsItem *, int> copied_{}; // item, paste times

    CommandStack commands_{};
};

#endif // SCREEN_SHOTER_H