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
    ImageWindow(const QPixmap& image, const QPoint& pos, QWidget * parent = nullptr)
        : ImageWindow(parent)
    {
        pixmap_ = image;
        original_pos_ = pos;
    }
    virtual ~ImageWindow() override = default;

    bool operator==(const ImageWindow& r) const
    {
        return pixmap_.cacheKey() == r.pixmap_.cacheKey();
    }

    void show()
    {
        if(status_ == 0) {
            status_ = 1;
            QWidget::show();
        }
    }

    void fix();

    void registerShortcuts();

    QPixmap image() const { return pixmap_; }

public slots:
    void hide() { if(status_ == 1) { status_ = 0; QWidget::hide(); } }
    void close() { status_ = -1; QWidget::hide(); }
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

    int8_t status_ = -1; // -1: init or closed. 0: hided 1: show

    QPoint begin_{0, 0};

    qreal scale_ = 1.0;
    qreal opacity_ = 1.0;
    QPixmap pixmap_;
    QPoint original_pos_ = {0, 0};
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
