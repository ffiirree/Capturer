#include "imagewindow.h"
#include <QKeyEvent>
#include <QPainter>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFileDialog>
#include <QShortcut>
#include <QMoveEvent>
#include <QMimeData>
#include "utils.h"
#include "logging.h"

ImageWindow::ImageWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);

    setAttribute(Qt::WA_TranslucentBackground);
    setAcceptDrops(true);

    effect_ = new QGraphicsDropShadowEffect(this);
    effect_->setBlurRadius(shadow_r_);
    effect_->setOffset(0, 0);
    effect_->setColor(QColor("#409eff"));
    setGraphicsEffect(effect_);

    menu_ = new ImageEditMenu(this, ImageEditMenu::ALL & ~ImageEditMenu::SAVE_GROUP);
    canvas_ = new Canvas(menu_, this);
    installEventFilter(canvas_);

    connect(menu_, &ImageEditMenu::save, this, &ImageWindow::saveAs);
    connect(canvas_, &Canvas::closed, [this]() {
        setMouseTracking(false);
        QWidget::repaint(QRect({ shadow_r_, shadow_r_ }, canvas_->pixmap().size()));
        pixmap_ = canvas_->pixmap().copy();
        scale_ = 1.0;
        canvas_->disable();
        editing_ = false;
    });
    connect(canvas_, &Canvas::changed, [this]() {
        QWidget::update(QRect({ shadow_r_, shadow_r_ }, canvas_->pixmap().size()));
    });

    registerShortcuts();
    initContextMenu();
}

void ImageWindow::image(const QPixmap& image)
{
    original_pixmap_ = image;
    pixmap_ = original_pixmap_.copy();
    canvas_->pixmap(pixmap_);

    scale_ = 1.0;

    if (image.hasAlpha()) {
        bg_ = Qt::transparent;
        effect_->setEnabled(!pixmap_.hasAlpha());
        shadow_r_ = pixmap_.hasAlpha() ? 0 : DEFAULT_SHADOW_R_;
    }
}

void ImageWindow::show(bool visible)
{
    switch (status_)
    {
    case WindowStatus::CREATED:
        status_ = WindowStatus::SHOWED;
        resize(getShadowSize(canvas_->pixmap().size()));
        move(original_pos_ - QPoint{ shadow_r_, shadow_r_ });
        QWidget::show();
        break;

    case WindowStatus::INVISIBLE:
        if (visible) {
            status_ = WindowStatus::SHOWED;
            QWidget::show();
        }
        break;

    case WindowStatus::HIDED:
        status_ = WindowStatus::SHOWED;
        QWidget::show();
        break;

    case WindowStatus::SHOWED:
    default: break;
    }
}

void ImageWindow::hide()
{
    if (status_ == WindowStatus::SHOWED) {
        status_ = WindowStatus::HIDED;
        setMouseTracking(false);
        editing_ = false;
        canvas_->disable();
        menu_->reset();
        menu_->hide();
        QWidget::hide();
    }
}

void ImageWindow::invisible()
{
    status_ = WindowStatus::INVISIBLE;
    setMouseTracking(false);
    editing_ = false;
    canvas_->disable();
    menu_->reset();
    menu_->hide();
    QWidget::hide();
}

void ImageWindow::update(Modified type)
{
    if (editing_) return;

    switch (type)
    {
    case Modified::ROTATED:
        pixmap_ = pixmap_.transformed(QTransform().rotate(1 * 90.0), Qt::SmoothTransformation);
        break;

    case Modified::ANTI_ROTATED:
        pixmap_ = pixmap_.transformed(QTransform().rotate(-1 * 90.0), Qt::SmoothTransformation);
        break;

    case Modified::FLIPPED_V:
        pixmap_ = QPixmap::fromImage(pixmap_.toImage().mirrored(false, true));
        break;

    case Modified::FLIPPED_H:
        pixmap_ = QPixmap::fromImage(pixmap_.toImage().mirrored(true, false));
        break;

    case Modified::GRAY:
        if (pixmap_.hasAlpha()) {
            auto img = pixmap_.toImage();
            for (int y = 0; y < img.height(); ++y) {
                QRgb* scanLine = (QRgb*)img.scanLine(y);
                for (int x = 0; x < img.width(); ++x) {
                    QRgb pixel = *scanLine;
                    uint ci = uint(qGray(pixel));
                    *scanLine = qRgba(ci, ci, ci, qAlpha(pixel));
                    ++scanLine;
                }
            }
            pixmap_ = QPixmap::fromImage(img);
        }
        else {
            pixmap_ = QPixmap::fromImage(pixmap_.toImage().convertToFormat(QImage::Format::Format_Grayscale8));
        }
        break;

    case Modified::RECOVERED:
        effect_->setEnabled(!pixmap_.hasAlpha());
        shadow_r_ = pixmap_.hasAlpha() ? 0 : DEFAULT_SHADOW_R_;

        scale_ = 1.0;
        opacity_ = 1.0;
        setWindowOpacity(opacity_);

        pixmap_ = original_pixmap_.copy();
        break;

    default: break;
    }

    canvas_->pixmap(scale_ == 1.0 ? pixmap_ : pixmap_.scaled(pixmap_.size() * scale_, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    auto geometry = getShadowGeometry(canvas_->pixmap().size());
#ifdef _WIN32
    if (type == Modified::SCALED) {
        // resize and repaint first
        resize(geometry.size());
        repaint();
        move(geometry.topLeft());
    }
    else {
        setGeometry(geometry);
    }
#elif __linux__
    setGeometry(geometry);
#endif
}

QRect ImageWindow::getShadowGeometry(QSize _size)
{
    auto size = getShadowSize(thumbnail_ ? QSize{ 125, 125 } : _size);

    QRect rect({ 0, 0 }, size);
    rect.moveCenter(geometry().center());
    return rect;
}

void ImageWindow::mousePressEvent(QMouseEvent* event)
{
    if (editing_) return;


    setCursor(Qt::SizeAllCursor);
    window_move_begin_pos_ = event->globalPos();
}

void ImageWindow::mouseMoveEvent(QMouseEvent* event)
{
    if (editing_) {
        setCursor(canvas_->getCursorShape());
    }
    else {
        move(event->globalPos() - window_move_begin_pos_ + pos());
        window_move_begin_pos_ = event->globalPos();
    }
}

void ImageWindow::mouseReleaseEvent(QMouseEvent*)
{
}

void ImageWindow::mouseDoubleClickEvent(QMouseEvent*)
{
    if (editing_) return;

    thumbnail_ = !thumbnail_;
    update(Modified::THUMBNAIL);
}

void ImageWindow::wheelEvent(QWheelEvent* event)
{
    if (editing_) return;

    auto delta = event->angleDelta().y();
    if (ctrl_) {
        opacity_ += (delta / 12000.0);
        opacity_ = std::clamp(opacity_, 0.01, 1.0);

        setWindowOpacity(opacity_);
    }
    else if (!thumbnail_) {
        scale_ = delta > 0 ? scale_ * 1.05 : scale_ / 1.05;
        scale_ = std::max(std::min(125.0 / std::min(pixmap_.size().width(), pixmap_.size().height()), 1.0), scale_);

        update(Modified::SCALED);
    }
}

void ImageWindow::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    // background for alpha
    if (bg_ != Qt::transparent && canvas_->pixmap().hasAlpha()) {
        painter.fillRect(shadow_r_, shadow_r_, width() - 2 * shadow_r_, height() - 2 * shadow_r_, bg_);
    }

    if (thumbnail_) {
        auto center = canvas_->pixmap().rect().center();
        painter.drawPixmap(QRect{ shadow_r_, shadow_r_, 125, 125 }, canvas_->pixmap(),
            { center.x() - THUMBNAIL_WIDTH_ / 2, center.y() - THUMBNAIL_WIDTH_ / 2, THUMBNAIL_WIDTH_, THUMBNAIL_WIDTH_ });
    }
    else {
        painter.drawPixmap(shadow_r_, shadow_r_, canvas_->pixmap());

        canvas_->drawModifying(&painter);
        canvas_->modified(PaintType::UNMODIFIED);
    }
}

void ImageWindow::paste()
{
    if (editing_ && QApplication::clipboard()->mimeData()->hasImage()) return;

    image(QApplication::clipboard()->pixmap());
    resize(getShadowSize(canvas_->pixmap().size()));
}

void ImageWindow::open()
{
    if (editing_) return;

    auto filename = QFileDialog::getOpenFileName(this,
        tr("Open Image"),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        "Image Files(*.png *.jpg *.jpeg *.bmp)");
    if (!filename.isEmpty()) {
        image(QPixmap(filename));
        resize(getShadowSize(original_pixmap_.size()));
    }
}

void ImageWindow::saveAs()
{
    if (editing_) return;

    QString default_filename = "Capturer_picture_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz") + ".png";

    QString filename = QFileDialog::getSaveFileName(
        this,
        tr("Save Image"),
        save_path_ + QDir::separator() + default_filename,
        "PNG(*.png);;JPEG(*.jpg *.jpeg);;BMP(*.bmp)"
    );

    if (!filename.isEmpty()) {
        QFileInfo fileinfo(filename);
        save_path_ = fileinfo.absoluteDir().path();

        canvas_->pixmap().save(filename);
    }
}

void ImageWindow::recover()
{
    if (thumbnail_) return;

    update(Modified::RECOVERED);
}

void ImageWindow::effectEnabled()
{
    if (editing_) return;

    shadow_r_ = !effect_->isEnabled() ? DEFAULT_SHADOW_R_ : 0;

    update(Modified::SHADOW);
    effect_->setEnabled(!effect_->isEnabled());
}

void ImageWindow::initContextMenu()
{
    context_menu_ = new QMenu(this);
    context_menu_->setWindowFlag(Qt::FramelessWindowHint);
    context_menu_->setWindowFlag(Qt::NoDropShadowWindowHint);
    context_menu_->setAttribute(Qt::WA_TranslucentBackground);

    addAction(context_menu_->addAction(tr("Copy image"), [this]() { editing_ ? canvas_->copy() : QApplication::clipboard()->setPixmap(image()); }, QKeySequence(Qt::CTRL | Qt::Key_C)));
    addAction(context_menu_->addAction(tr("Paste image"), [this](){ editing_ ? canvas_->paste() : paste(); }, QKeySequence(Qt::CTRL | Qt::Key_V)));

    context_menu_->addSeparator();

    context_menu_->addAction(tr("Edit"), [this]() {
        if (thumbnail_) return;

        editing_ = true;
        canvas_->reset();
        canvas_->enable();
        setMouseTracking(true);
        canvas_->offset({ -shadow_r_, -shadow_r_ });
        canvas_->globalOffset(geometry().topLeft());
        canvas_->pixmap(pixmap_.scaled(pixmap_.size() * scale_, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        menu_->show();
        moveMenu();
    });

    context_menu_->addSeparator();

    addAction(context_menu_->addAction(tr("Open image..."), this, &ImageWindow::open, QKeySequence(Qt::CTRL | Qt::Key_O)));
    addAction(context_menu_->addAction(tr("Save as..."), this, &ImageWindow::saveAs, QKeySequence(Qt::CTRL | Qt::Key_S)));

    context_menu_->addSeparator();

    addAction(context_menu_->addAction(tr("Grayscale"), [this]() { update(Modified::GRAY); }, QKeySequence(Qt::Key_G)));
    addAction(context_menu_->addAction(tr("Rotate 90"), [this]() { update(Modified::ROTATED); }, QKeySequence(Qt::Key_R)));
    addAction(context_menu_->addAction(tr("Rotate -90"), [this]() { update(Modified::ROTATED); }, QKeySequence(Qt::CTRL | Qt::Key_R)));
    addAction(context_menu_->addAction(tr("Flip H"), [this]() { update(Modified::FLIPPED_H); }, QKeySequence(Qt::Key_H)));
    addAction(context_menu_->addAction(tr("Flip V"), [this]() { update(Modified::FLIPPED_V); }, QKeySequence(Qt::Key_V)));

    context_menu_->addSeparator();

    auto sub_menu = new QMenu(tr("Background"), this);
    sub_menu->setWindowFlag(Qt::FramelessWindowHint);
    sub_menu->setWindowFlag(Qt::NoDropShadowWindowHint);
    sub_menu->setAttribute(Qt::WA_TranslucentBackground);
    sub_menu->addAction(tr("White"), [this]() { bg_ = Qt::white; update(Modified::BACKGROUND); });
    sub_menu->addAction(tr("Gray"), [this]() { bg_ = Qt::gray; update(Modified::BACKGROUND); });
    sub_menu->addAction(tr("Black"), [this]() { bg_ = Qt::black; update(Modified::BACKGROUND); });
    sub_menu->addAction(tr("Transparent"), [this]() { bg_ = Qt::transparent; update(Modified::BACKGROUND); });
    context_menu_->addMenu(sub_menu);
    shadow_action_ = context_menu_->addAction((effect_->isEnabled() ? tr("Hide ") : tr("Show ")) + tr("Shadow"), this, &ImageWindow::effectEnabled);
    zoom_action_ = context_menu_->addAction(tr("Zoom : ") + QString::number(static_cast<int>(scale_ * 100)) + "%");
    opacity_action_ = context_menu_->addAction(tr("Opacity : ") + QString::number(static_cast<int>(opacity_ * 100)) + "%");
    context_menu_->addAction(tr("Recover"), this, &ImageWindow::recover);

    context_menu_->addSeparator();

    context_menu_->addAction(tr("Close"), this, &ImageWindow::invisible, QKeySequence(Qt::Key_Escape));
}

void ImageWindow::contextMenuEvent(QContextMenuEvent* event)
{
    if (editing_) return;

    shadow_action_->setText((effect_->isEnabled() ? tr("Hide ") : tr("Show ")) + tr("Shadow"));
    zoom_action_->setText(tr("Zoom : ") + QString::number(static_cast<int>(scale_ * 100)) + "%");
    opacity_action_->setText(tr("Opacity : ") + QString::number(static_cast<int>(opacity_ * 100)) + "%");

    context_menu_->exec(event->globalPos());
}

void ImageWindow::dropEvent(QDropEvent* event)
{
    if (editing_) return;

    auto path = event->mimeData()->urls()[0].toLocalFile();

    scale_ = 1.0;
    image(QPixmap(path));
    resize(getShadowSize(canvas_->pixmap().size()));

    event->acceptProposedAction();
}

void ImageWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (editing_) return;

    auto mimedata = event->mimeData();
    if (mimedata->hasUrls() && QString("jpg;png;jpeg;JPG;PNG;JPEG;bmp;BMP;ico;ICO;gif;GIF").contains(QFileInfo(mimedata->urls()[0].fileName()).suffix()))
        event->acceptProposedAction();
}

void ImageWindow::moveEvent(QMoveEvent*)
{
    moveMenu();
}

void ImageWindow::moveMenu()
{
    auto rect = geometry().adjusted(shadow_r_, shadow_r_, -shadow_r_ - menu_->width(), -shadow_r_ + 5);
    menu_->move(rect.bottomRight());
    menu_->setSubMenuShowBelow();
}

void ImageWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Control) {
        ctrl_ = true;
    }
}

void ImageWindow::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Control) {
        ctrl_ = false;
    }
}

void ImageWindow::registerShortcuts()
{
    // close 
    connect(new QShortcut(Qt::Key_Escape, this), &QShortcut::activated, [this]() {
        if (editing_) {
            setMouseTracking(false);
            editing_ = false;
            canvas_->disable();
            menu_->reset();
            menu_->hide();
        }
        else {
            invisible();
        }
    });

    // move
    connect(new QShortcut(Qt::Key_W, this), &QShortcut::activated, [this]() { if (!editing_) setGeometry(geometry().adjusted(0, -1, 0, 0)); });
    connect(new QShortcut(Qt::Key_Up, this), &QShortcut::activated, [this]() { if (!editing_) setGeometry(geometry().adjusted(0, -1, 0, 0)); });

    connect(new QShortcut(Qt::Key_S, this), &QShortcut::activated, [this]() { if (!editing_) setGeometry(geometry().adjusted(0, 1, 0, 0)); });
    connect(new QShortcut(Qt::Key_Down, this), &QShortcut::activated, [this]() { if (!editing_) setGeometry(geometry().adjusted(0, 1, 0, 0)); });

    connect(new QShortcut(Qt::Key_A, this), &QShortcut::activated, [this]() { if (!editing_) setGeometry(geometry().adjusted(-1, 0, 0, 0)); });
    connect(new QShortcut(Qt::Key_Left, this), &QShortcut::activated, [this]() { if (!editing_) setGeometry(geometry().adjusted(-1, 0, 0, 0)); });

    connect(new QShortcut(Qt::Key_D, this), &QShortcut::activated, [this]() { if (!editing_) setGeometry(geometry().adjusted(1, 0, 0, 0)); });
    connect(new QShortcut(Qt::Key_Right, this), &QShortcut::activated, [this]() { if (!editing_) setGeometry(geometry().adjusted(1, 0, 0, 0)); });

    connect(new QShortcut(Qt::CTRL | Qt::Key_Z, this), &QShortcut::activated, canvas_, &Canvas::undo);
    connect(new QShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_Z, this), &QShortcut::activated, canvas_, &Canvas::redo);
    connect(new QShortcut(Qt::Key_Delete, this), &QShortcut::activated, canvas_, &Canvas::remove);
}
