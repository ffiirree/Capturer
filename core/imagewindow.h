#ifndef CAPTURER_FIXEDWINDOW_H
#define CAPTURER_FIXEDWINDOW_H

#include <QWidget>
#include <QMenu>
#include <QPainter>
#include <QLabel>
#include <QGraphicsDropShadowEffect>
#include "imageeditmenu.h"

class ImageWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ImageWindow(QWidget *parent = nullptr);
    virtual ~ImageWindow() override = default;

    void fix(QPixmap image);

    void registerShortcuts();

public slots:
    void copy();
    void paste();
    void open();
    void saveAs();
    void recover();
    void effectEnabled();

private:
    virtual void mousePressEvent(QMouseEvent *) override;
    virtual void mouseMoveEvent(QMouseEvent *) override;
    virtual void wheelEvent(QWheelEvent *) override;
    virtual void keyPressEvent(QKeyEvent *) override;
    virtual void keyReleaseEvent(QKeyEvent *) override;
    virtual void paintEvent(QPaintEvent *) override;
    virtual void contextMenuEvent(QContextMenuEvent *) override;
    virtual void moveEvent(QMoveEvent *event) override;
    virtual void dropEvent(QDropEvent *event) override;
    virtual void dragEnterEvent(QDragEnterEvent *event) override;

    void moveMenu();

    QPoint begin_{0, 0};

    qreal scale_ = 1.0;
    qreal opacity_ = 1.0;
    QPixmap pixmap_;
    QSize size_{0, 0};
    QPoint center_{0, 0};
    QMenu * menu_;

    bool ctrl_ = false;

    QPainter painter_;
    bool editing_ = false;
    bool thumbnail_ = false;
    ImageEditMenu edit_menu_;
    const static int DEFAULT_SHADOW_R_{ 10 };
    int shadow_r_ = DEFAULT_SHADOW_R_;
    bool effect_enabled_ = true;
    QGraphicsDropShadowEffect *effect_;
};

#endif //! CAPTURER_FIXEDWINDOW_H
