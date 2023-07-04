#ifndef CAPTURER_IMAGE_WINDOW_H
#define CAPTURER_IMAGE_WINDOW_H

#include "framelesswindow.h"
#include "menu/editing-menu.h"
#include "texture-widget.h"

#include <any>
#include <optional>
#include "menu.h"
#include <QStandardPaths>

class ImageWindow : public FramelessWindow
{
    Q_OBJECT

public:
    explicit ImageWindow(QWidget *parent = nullptr);

    void preview(const std::shared_ptr<QMimeData>& data);

    std::optional<QPixmap> render(const std::shared_ptr<QMimeData>& mimedata);

    void present(const QPixmap& pixmap);

public slots:
    void paste();
    void open();
    void saveAs();
    void recover();

signals:
    void closed();

private:
    void mouseDoubleClickEvent(QMouseEvent *) override;

    void wheelEvent(QWheelEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;
    void contextMenuEvent(QContextMenuEvent *) override;
    void dropEvent(QDropEvent *) override;
    void dragEnterEvent(QDragEnterEvent *) override;

    void closeEvent(QCloseEvent *) override;

    void registerShortcuts();
    void initContextMenu();

    bool thumbnail_{ false };
    qreal scale_{ 1.0 };
    qreal opacity_{ 1.0 };

    bool ctrl_{ false };

    QSize THUMBNAIL_SIZE_{ 125, 125 };

    QMenu *context_menu_{ nullptr };
    QAction *zoom_action_{ nullptr };
    QAction *opacity_action_{ nullptr };

    QString save_path_{ QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) };

    // data
    std::shared_ptr<QMimeData> data_{};
    QPixmap pixmap_{};
    TextureWidget *preview_{};
};

#endif //! CAPTURER_IMAGE_WINDOW_H
