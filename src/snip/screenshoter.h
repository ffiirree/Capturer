#ifndef SCREEN_SHOTER_H
#define SCREEN_SHOTER_H

#include "canvas.h"
#include "circlecursor.h"
#include "command.h"
#include "config.h"
#include "hunter.h"

#include <QPixmap>
#include <QStandardPaths>
#include <QSystemTrayIcon>

// Layer 1: Selector
// Layer 2: Graphics View
// Layer 3: Parent QWidget

class Selector;
class QGraphicsScene;
class QGraphicsView;
class ImageEditMenu;
class Magnifier;

class ScreenShoter : public QWidget
{
    Q_OBJECT

public:
    explicit ScreenShoter(QWidget *parent = nullptr);

signals:
    void focusOnGraph(Graph);
    void pinSnipped(const QPixmap& image, const QPoint& pos);
    void SHOW_MESSAGE(const QString& title, const QString& msg,
                      QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information, int msecs = 10'000);

public slots:
    void start();
    void exit();

    void save();
    void copy();
    void pin();
    QPixmap snip();
    void save2clipboard(const QPixmap&, bool);

    void updateTheme() {}

    void moveMenu();

protected:
    bool eventFilter(QObject *, QEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    //void keyPressEvent(QKeyEvent *) override;
    //void keyReleaseEvent(QKeyEvent *) override;
    ////void paintEvent(QPaintEvent *) override;
    //void mouseDoubleClickEvent(QMouseEvent *) override;

private:
    void registerShortcuts();

    void moveMagnifier();

    Selector *selector_{};    // Layer 1
    QGraphicsScene *scene_{}; //
    QGraphicsView *view_{};   // Layer 2

    ImageEditMenu *menu_{};   // Edit menu
    Magnifier *magnifier_{};  // Magnifier

    QPoint move_begin_{ 0, 0 };
    QPoint resize_begin_{ 0, 0 };

    CircleCursor circle_cursor_{ 20 };
    //Canvas *canvas_{ nullptr };

    std::vector<hunter::prey_t> history_;
    size_t history_idx_{ 0 };

    QString save_path_{ QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) };
};

#endif // SCREEN_SHOTER_H
