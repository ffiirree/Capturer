#ifndef SCREEN_SHOTER_H
#define SCREEN_SHOTER_H

#include <QPixmap>
#include <QSystemTrayIcon>
#include "selector.h"
#include "imageeditmenu.h"
#include "magnifier.h"
#include "command.h"
#include "resizer.h"
#include "circlecursor.h"
#include "config.h"
#include "canvas.h"

class ScreenShoter : public Selector
{
    Q_OBJECT

public:
    explicit ScreenShoter(QWidget *parent = nullptr);

signals:
    void focusOnGraph(Graph);
    void FIX_IMAGE(const QPixmap& image, const QPoint& pos);
    void SHOW_MESSAGE(const QString &title, const QString &msg,
                      QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information, int msecs = 10000);

public slots:
    void start() override;
    void exit() override;

    void save();
    void copy();
    void pin();
    QPixmap snipped();

    void updateTheme()
    {
        Selector::updateTheme(Config::instance()["snip"]["selector"]);
    }

private slots:
    void moveMenu();

protected:
    bool eventFilter(QObject *, QEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;
    void paintEvent(QPaintEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;

private:
    void registerShortcuts();

    void moveMagnifier();

    QPixmap snippedImage();

    QPixmap captured_screen_;

    ImageEditMenu * menu_ = nullptr;
    Magnifier * magnifier_ = nullptr;

    QPoint move_begin_{0, 0};
    QPoint resize_begin_{0, 0};

    CircleCursor circle_cursor_{20};
    Canvas *canvas_ = nullptr;

    struct History{
        QPixmap image_;
        QRect rect_;
        CommandStack commands_;
    };

    vector<History> history_;
    size_t history_idx_ = 0;
};

#endif // SCREEN_SHOTER_H
