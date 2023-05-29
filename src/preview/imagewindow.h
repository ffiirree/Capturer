#ifndef IMAGE_WINDOW_H
#define IMAGE_WINDOW_H

#include "menu/editing-menu.h"

#include <QGraphicsDropShadowEffect>
#include <QMenu>
#include <QStandardPaths>
#include <QWidget>

class ImageWindow : public QWidget
{
    Q_OBJECT

public:
    enum class Modified
    {
        ROTATED,
        ANTI_ROTATED,
        FLIPPED_V,
        FLIPPED_H,
        SCALED,
        THUMBNAIL,
        RECOVERED,
        SHADOW,
        GRAY,
        BACKGROUND,
        ALL
    };

    enum class WindowStatus
    {
        CREATED,
        INVISIBLE,
        HIDED,
        SHOWED
    };

    explicit ImageWindow(QWidget *parent = nullptr);

    ImageWindow(const QPixmap& image, const QPoint& pos, QWidget *parent = nullptr)
        : ImageWindow(parent)
    {
        this->image(image);
        original_pos_ = pos;

        show();
    }

    ~ImageWindow() override = default;

    QPixmap image() const { return original_pixmap_; }

    void image(const QPixmap& image);

public slots:
    void show(bool = true);
    void hide();
    void invisible();

    void paste();
    void open();
    void saveAs();
    void recover();
    void effectEnabled();

private:
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;

    void wheelEvent(QWheelEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;
    void paintEvent(QPaintEvent *) override;
    void contextMenuEvent(QContextMenuEvent *) override;
    void dropEvent(QDropEvent *) override;
    void dragEnterEvent(QDragEnterEvent *) override;

    void registerShortcuts();
    void initContextMenu();

    void update(Modified);
    QRect getShadowGeometry(QSize);

    QSize getShadowSize(QSize size) const { return size + QSize{ shadow_r_ * 2, shadow_r_ * 2 }; }

    WindowStatus status_{ WindowStatus::CREATED };

    Modified modified_status_{ Modified::ALL };

    QPoint window_move_begin_pos_{ 0, 0 };

    QPixmap original_pixmap_;
    QPixmap pixmap_;
    QPoint original_pos_{ 0, 0 };

    bool thumbnail_{ false };
    qreal scale_{ 1.0 };
    qreal opacity_{ 1.0 };
    Qt::GlobalColor bg_{ Qt::transparent };

    bool ctrl_{ false };

    static const int DEFAULT_SHADOW_R_{ 10 };
    static const int THUMBNAIL_WIDTH_{ 125 };
    int shadow_r_ = DEFAULT_SHADOW_R_;
    QGraphicsDropShadowEffect *effect_{ nullptr };

    QMenu *context_menu_{ nullptr };
    QAction *shadow_action_{ nullptr };
    QAction *zoom_action_{ nullptr };
    QAction *opacity_action_{ nullptr };

    QString save_path_{ QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) };
};

#endif // IMAGE_WINDOW_H
