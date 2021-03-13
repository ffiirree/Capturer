#include "imagewindow.h"
#include <QKeyEvent>
#include <QPainter>
#include <QApplication>
#include <QClipboard>
#include <QStandardPaths>
#include <QDateTime>
#include <QFileDialog>
#include <QShortcut>
#include <QMoveEvent>
#include <QMimeData>
#include "utils.h"
#include "logging.h"

ImageWindow::ImageWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Dialog);

    setAttribute(Qt::WA_TranslucentBackground);
    setAcceptDrops(true);

    effect_ = new QGraphicsDropShadowEffect(this);
    effect_->setBlurRadius(shadow_r_);
    effect_->setOffset(0, 0);
    effect_->setColor(QColor("#409eff"));
    setGraphicsEffect(effect_);

    canvas_ = new Canvas(this);
    installEventFilter(canvas_);
    menu_ = canvas_->menu_;
    connect(menu_, &ImageEditMenu::save, this, &ImageWindow::saveAs);
    connect(canvas_, &Canvas::closed, [this]() { 
        setMouseTracking(false); 
        QWidget::repaint(QRect({ shadow_r_, shadow_r_ }, canvas_->canvas().size()));
        pixmap_ = canvas_->canvas().copy();
        canvas_->disable();
        editing_ = false;
    });
    connect(canvas_, &Canvas::changed, [this]() { 
        QWidget::update(QRect({ shadow_r_, shadow_r_ }, canvas_->canvas().size())); 
    });

    registerShortcuts();
}

void ImageWindow::image(const QPixmap& image)
{ 
    original_pixmap_ = image;
    pixmap_ = original_pixmap_.copy();
    canvas_->canvas(pixmap_);

    scale_ = 1.0;

    if (image.hasAlpha()) bg_ = Qt::gray;
}

void ImageWindow::show(bool visable)
{
    switch (status_)
    {
    case WindowStatus::CREATED:
        status_ = WindowStatus::SHOWED;
        resize(getShadowSize(canvas_->canvas().size()));
        move(original_pos_ - QPoint{ shadow_r_, shadow_r_ });
        QWidget::show();
        break;

    case WindowStatus::INVISABLE:
        if (visable) {
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

void ImageWindow::invisable()
{
    status_ = WindowStatus::INVISABLE;
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
        pixmap_ = pixmap_.transformed(QMatrix().rotate(1 * 90.0), Qt::FastTransformation);
        break;

    case Modified::ANTI_ROTATED:
        pixmap_ = pixmap_.transformed(QMatrix().rotate(-1 * 90.0), Qt::FastTransformation);
        break;

    case Modified::FLIPPED_V:
        pixmap_ = QPixmap::fromImage(pixmap_.toImage().mirrored(false, true));
        break;

    case Modified::FLIPPED_H:
        pixmap_ = QPixmap::fromImage(pixmap_.toImage().mirrored(true, false));
        break;

    case Modified::GRAY:
        pixmap_ = QPixmap::fromImage(pixmap_.toImage().convertToFormat(QImage::Format::Format_Grayscale8));
        break;

    case Modified::RECOVERED:
        effect_->setEnabled(true);

        shadow_r_ = DEFAULT_SHADOW_R_;
        scale_ = 1.0;
        opacity_ = 1.0;
        setWindowOpacity(opacity_);

        pixmap_ = original_pixmap_.copy();
        break;

    default: break;
    }

    canvas_->canvas(scale_ == 1.0 ? pixmap_ : pixmap_.scaled(pixmap_.size() * scale_, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    QWidget::update(QRect({ shadow_r_, shadow_r_ }, canvas_->canvas().size()));
    setGeometry(getShadowGeometry(canvas_->canvas().size()));
}

QRect ImageWindow::getShadowGeometry(QSize _size)
{
    auto size = getShadowSize(thumbnail_ ? QSize{ 125, 125 } : _size);

    QRect rect({ 0, 0 }, size);
    rect.moveCenter(geometry().center());
    return rect;
}

void ImageWindow::mousePressEvent(QMouseEvent *event)
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

void ImageWindow::mouseReleaseEvent(QMouseEvent* event)
{
    
}

void ImageWindow::mouseDoubleClickEvent(QMouseEvent* event)
{
    // thumbnail_
    thumbnail_ = !thumbnail_;
    update(Modified::THUMBNAIL);
}

void ImageWindow::wheelEvent(QWheelEvent *event)
{
    if (editing_) return;

    auto delta = (event->delta() / 12000.0);         // +/-1%

    if (ctrl_) {
        opacity_ += delta;
        if (opacity_ < 0.01) opacity_ = 0.01;
        if (opacity_ > 1.00) opacity_ = 1.00;

        setWindowOpacity(opacity_);
    }
    else if (!thumbnail_) {
        scale_ += delta;
        scale_ = scale_ < 0.01 ? 0.01 : scale_;

        update(Modified::SCALED);
    }

}

void ImageWindow::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    // background for alpha
    if (bg_ != Qt::transparent && canvas_->canvas().hasAlpha()) {
        painter.fillRect(shadow_r_, shadow_r_, width() - 2 * shadow_r_, height() - 2 * shadow_r_, bg_);
    }

    if (thumbnail_) {
        auto center = canvas_->canvas().rect().center();
        painter.drawPixmap(QRect{ shadow_r_, shadow_r_, 125, 125 }, canvas_->canvas(),
            { center.x() - THUMBNAIL_WIDTH_ / 2, center.y() - THUMBNAIL_WIDTH_ / 2, THUMBNAIL_WIDTH_, THUMBNAIL_WIDTH_ });
    }
    else {
        painter.drawPixmap(shadow_r_, shadow_r_, canvas_->canvas());

        canvas_->drawModifying(&painter);
        canvas_->modified(PaintType::UNMODIFIED);
    }
}

void ImageWindow::paste()
{
    if (editing_) return;

    image(QApplication::clipboard()->pixmap());
    resize(getShadowSize(canvas_->canvas().size()));
}

void ImageWindow::open()
{
    if (editing_) return;

    auto filename = QFileDialog::getOpenFileName(this,
                                                 tr("Open Image"),
                                                 QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
                                                 "Image Files(*.png *.jpg *.jpeg *.bmp)");
    if(!filename.isEmpty()) {
        image(QPixmap(filename));
        resize(getShadowSize(original_pixmap_.size()));
    }
}

void ImageWindow::saveAs()
{
    if (editing_) return;

    QString default_filepath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QString default_filename = "Capturer_picture_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz") + ".png";
#ifdef _WIN32
    auto filename = QFileDialog::getSaveFileName(this,
                                                 tr("Save Image"),
                                                 default_filepath + QDir::separator() + default_filename,
                                                 "PNG(*.png);;JPEG(*.jpg *.jpeg);;BMP(*.bmp)");

    if(!filename.isEmpty()) {
        canvas_->canvas().save(filename);
    }
#elif __linux__
    auto filename = default_filepath + QDir::separator() + default_filename;

    canvas_->canvas().save(filename);
#endif
}

void ImageWindow::recover()
{
    if(thumbnail_) return;

    update(Modified::RECOVERED);
}

void ImageWindow::effectEnabled()
{
    if (editing_) return;

    shadow_r_ = !effect_->isEnabled() ? DEFAULT_SHADOW_R_ : 0;
    
    update(Modified::SHADOW);
    effect_->setEnabled(!effect_->isEnabled());
}

void ImageWindow::contextMenuEvent(QContextMenuEvent * event)
{
    if (editing_) return;

    auto menu = new QMenu(this);

    menu->addAction(tr("Copy image") + "\tCtrl + C", [this]() { QApplication::clipboard()->setPixmap(image()); });
    menu->addAction(tr("Paste image") + "\tCtrl + V", this, &ImageWindow::paste);

    menu->addSeparator();

    menu->addAction(tr("Edit"), [this]() {
        if (thumbnail_) return;

        editing_ = true;
        canvas_->enable();
        setMouseTracking(true);
        canvas_->offset({ -shadow_r_, -shadow_r_ });
        canvas_->canvas(pixmap_);
        menu_->show();
        moveMenu();
    });

    menu->addSeparator();

    menu->addAction(tr("Open image...") + "\tCtrl + O", this, &ImageWindow::open);
    menu->addAction(tr("Save as...") + "\tCtrl + S", this, &ImageWindow::saveAs);

    menu->addSeparator();
    
    menu->addAction(tr("Grayscale") + "\tG", [this]() { update(Modified::GRAY); });
    menu->addAction(tr("Rotate 90") + "\tR", [this]() { update(Modified::ROTATED); });
    menu->addAction(tr("Rotate -90") + "\tCtrl + R", [this]() { update(Modified::ROTATED); });
    menu->addAction(tr("Flip H") + "\tH", [this]() { update(Modified::FLIPPED_H); });
    menu->addAction(tr("Flip V") + "\tV", [this]() { update(Modified::FLIPPED_V); });

    menu->addSeparator();

    auto sub_menu = new QMenu(tr("Background"), this);
    sub_menu->addAction(tr("White"), [this]() { bg_ = Qt::white; update(Modified::BACKGROUND); });
    sub_menu->addAction(tr("Gray"), [this]() { bg_ = Qt::gray; update(Modified::BACKGROUND); });
    sub_menu->addAction(tr("Black"), [this]() { bg_ = Qt::black; update(Modified::BACKGROUND); });
    sub_menu->addAction(tr("Transparent"), [this]() { bg_ = Qt::transparent; update(Modified::BACKGROUND); });
    menu->addMenu(sub_menu);
    menu->addAction((effect_->isEnabled() ? tr("Hide ") : tr("Show ")) + tr("Shadow"), this, &ImageWindow::effectEnabled);
    menu->addAction(tr("Zoom : ") + QString::number(static_cast<int>(scale_ * 100)) + "%");
    menu->addAction(tr("Opacity : ") + QString::number(static_cast<int>(opacity_ * 100)) + "%");
    menu->addAction(tr("Recover"), this, &ImageWindow::recover);

    menu->addSeparator();

    menu->addAction(tr("Close") + "\tEsc", this, &ImageWindow::invisable);

    menu->exec(event->globalPos());
}

void ImageWindow::dropEvent(QDropEvent *event)
{
    if (editing_) return;

    auto path = event->mimeData()->urls()[0].toLocalFile();
    LOG(INFO) << "Drop to ImageWindow : "<< path;

    scale_ = 1.0;
    image(QPixmap(path));
    resize(getShadowSize(canvas_->canvas().size()));

    event->acceptProposedAction();
}

void ImageWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (editing_) return;

    auto mimedata = event->mimeData();
    if(mimedata->hasUrls() && QString("jpg;png;jpeg;JPG;PNG;JPEG;bmp;BMP;ico;ICO;gif;GIF").contains(QFileInfo(mimedata->urls()[0].fileName()).suffix()))
        event->acceptProposedAction();
}

void ImageWindow::moveEvent(QMoveEvent*)
{
    moveMenu();
}

void ImageWindow::moveMenu()
{
    auto rect = geometry().adjusted(shadow_r_, shadow_r_, -shadow_r_ - canvas_->menu_->width(), -shadow_r_ + 5);
    canvas_->menu_->move(rect.bottomRight());
    canvas_->menu_->setSubMenuShowBelow();
}

void ImageWindow::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Control) {
        ctrl_ = true;
    }
}

void ImageWindow::keyReleaseEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Control) {
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
            invisable();
        }
    });

    // copy & paste
    connect(new QShortcut(Qt::CTRL + Qt::Key_C, this), &QShortcut::activated, [this]() { 
        editing_? canvas_->copy() : QApplication::clipboard()->setPixmap(image()); 
    });
    connect(new QShortcut(Qt::CTRL + Qt::Key_V, this), &QShortcut::activated, [this] {
        editing_ ? canvas_->paste() : paste();
    });

    // save & open
    connect(new QShortcut(Qt::CTRL + Qt::Key_S, this), &QShortcut::activated, this, &ImageWindow::saveAs);
    connect(new QShortcut(Qt::CTRL + Qt::Key_O, this), &QShortcut::activated, this, &ImageWindow::open);

    // gray
    connect(new QShortcut(Qt::Key_G, this), &QShortcut::activated, [this]() { update(Modified::GRAY); });

    // rotate
    connect(new QShortcut(Qt::Key_R, this), &QShortcut::activated, [this]() { update(Modified::ROTATED); });
    connect(new QShortcut(Qt::CTRL + Qt::Key_R, this), &QShortcut::activated, [this]() { update(Modified::ANTI_ROTATED); });

    // flip
    connect(new QShortcut(Qt::Key_V, this), &QShortcut::activated, [this]() { update(Modified::FLIPPED_V); });
    connect(new QShortcut(Qt::Key_H, this), &QShortcut::activated, [this]() { update(Modified::FLIPPED_H); });

    // move
    connect(new QShortcut(Qt::Key_W, this), &QShortcut::activated, [this]() { if (!editing_) setGeometry(geometry().adjusted(0, -1, 0, 0)); });
    connect(new QShortcut(Qt::Key_Up, this), &QShortcut::activated, [this]() { if (!editing_) setGeometry(geometry().adjusted(0, -1, 0, 0)); });

    connect(new QShortcut(Qt::Key_S, this), &QShortcut::activated, [this]() { if (!editing_) setGeometry(geometry().adjusted(0, 1, 0, 0)); });
    connect(new QShortcut(Qt::Key_Down, this), &QShortcut::activated, [this]() { if (!editing_) setGeometry(geometry().adjusted(0, 1, 0, 0)); });

    connect(new QShortcut(Qt::Key_A, this), &QShortcut::activated, [this]() { if (!editing_) setGeometry(geometry().adjusted(-1, 0, 0, 0)); });
    connect(new QShortcut(Qt::Key_Left, this), &QShortcut::activated, [this]() { if (!editing_) setGeometry(geometry().adjusted(-1, 0, 0, 0)); });

    connect(new QShortcut(Qt::Key_D, this), &QShortcut::activated, [this]() { if (!editing_) setGeometry(geometry().adjusted(1, 0, 0, 0)); });
    connect(new QShortcut(Qt::Key_Right, this), &QShortcut::activated, [this]() { if (!editing_) setGeometry(geometry().adjusted(1, 0, 0, 0)); });

    connect(new QShortcut(Qt::CTRL + Qt::Key_Z, this), &QShortcut::activated, canvas_, &Canvas::undo);
    connect(new QShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_Z, this), &QShortcut::activated, canvas_, &Canvas::redo);
    connect(new QShortcut(Qt::Key_Delete, this), &QShortcut::activated, canvas_, &Canvas::remove);
}
