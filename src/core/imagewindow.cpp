#include "imagewindow.h"
#include <QKeyEvent>
#include <QPainter>
#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QFileDialog>
#include <QShortcut>
#include <QMoveEvent>
#include <QMimeData>
#include "utils.h"
#include "logging.h"

#define APPLAY_PIXMAP(PIXMAP)       st(                                         \
                                        paint_pixmap_ = PIXMAP;                 \
                                        QRect rect({0 ,0}, window_size());      \
                                        rect.moveCenter(geometry().center());   \
                                        setGeometry(rect);                      \
                                        update(QRect{ { shadow_r_, shadow_r_ }, paint_pixmap_.size() }); \
                                    )

ImageWindow::ImageWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Dialog);

    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setAcceptDrops(true);

    effect_ = new QGraphicsDropShadowEffect(this);
    effect_->setBlurRadius(shadow_r_);
    effect_->setOffset(0, 0);
    effect_->setColor(QColor("#409eff"));
    setGraphicsEffect(effect_);

    registerShortcuts();

    connect(&edit_menu_, &ImageEditMenu::save, this, &ImageWindow::saveAs);
    connect(&edit_menu_, &ImageEditMenu::ok, [](){});
    connect(&edit_menu_, &ImageEditMenu::fix, [](){});
    connect(&edit_menu_, &ImageEditMenu::exit, [this](){ edit_menu_.hide(); editing_ = false; });

    connect(&edit_menu_, &ImageEditMenu::undo, [](){});
    connect(&edit_menu_, &ImageEditMenu::redo, [](){});
}

void ImageWindow::fix()
{
    if(status_ == 1) return;
    status_ = 1;

    resize(window_size());

    QWidget::show();
    move(original_pos_ - QPoint{ shadow_r_, shadow_r_ });
}

void ImageWindow::mousePressEvent(QMouseEvent *event)
{
    if(editing_) {

    } else {
        setCursor(Qt::SizeAllCursor);
        window_move_begin_pos_ = event->globalPos();
    }
}

void ImageWindow::mouseDoubleClickEvent(QMouseEvent* event)
{
    // thumbnail_
    thumbnail_ = !thumbnail_;
    if (thumbnail_) {
        auto center = original_pixmap_.rect().center();
        APPLAY_PIXMAP(original_pixmap_.copy(center.x() - 62, center.y() - 62, 125, 125));
    }
    else {
        APPLAY_PIXMAP(original_pixmap_.scaled(original_pixmap_.size() * scale_, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void ImageWindow::mouseMoveEvent(QMouseEvent* event)
{
    if(editing_) {

    } else {
        move(event->globalPos() - window_move_begin_pos_ + pos());
        window_move_begin_pos_ = event->globalPos();
    }
}

void ImageWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if(editing_) {

    } else {

    }
}

void ImageWindow::wheelEvent(QWheelEvent *event)
{
    if(editing_) return;

    auto delta = (event->delta()/12000.0);         // +/-1%

    if(ctrl_) {
        opacity_ += delta;
        if(opacity_ < 0.01) opacity_ = 0.01;
        if(opacity_ > 1.00) opacity_ = 1.00;

        setWindowOpacity(opacity_);
    }
    else if(!thumbnail_) {
        scale_ += delta;
        scale_ = scale_ < 0.01 ? 0.01 : scale_;

        APPLAY_PIXMAP(original_pixmap_.scaled(original_pixmap_.size() * scale_, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void ImageWindow::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.drawPixmap(shadow_r_, shadow_r_, paint_pixmap_);
}

void ImageWindow::copy()
{
    QApplication::clipboard()->setPixmap(image());
}

void ImageWindow::paste()
{
    image(QApplication::clipboard()->pixmap());
    resize(window_size());
}

void ImageWindow::open()
{
    auto filename = QFileDialog::getOpenFileName(this,
                                                 tr("Open Image"),
                                                 QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
                                                 "Image Files(*.png *.jpg *.jpeg *.bmp)");
    if(!filename.isEmpty()) {
        image(QPixmap(filename));
        resize(window_size());
    }
}

void ImageWindow::saveAs()
{
    QString default_filepath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QString default_filename = "Capturer_picture_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz") + ".png";
#ifdef _WIN32
    auto filename = QFileDialog::getSaveFileName(this,
                                                 tr("Save Image"),
                                                 default_filepath + QDir::separator() + default_filename,
                                                 "PNG(*.png);;JPEG(*.jpg *.jpeg);;BMP(*.bmp)");

    if(!filename.isEmpty()) {
        original_pixmap_.save(filename);
    }
#elif __linux__
    auto filename = default_filepath + QDir::separator() + default_filename;

    original_pixmap_.save(filename);
#endif
}

void ImageWindow::recover()
{
    if(thumbnail_) return;

    effect_enabled_ = true;
    effect_->setEnabled(effect_enabled_);

    opacity_ = 1.0;
    setWindowOpacity(opacity_);

    scale_ = 1.0;
    APPLAY_PIXMAP(original_pixmap_.copy());
}

void ImageWindow::effectEnabled()
{
    effect_enabled_ = !effect_enabled_;
    shadow_r_ = effect_enabled_ ? DEFAULT_SHADOW_R_ : 0;

    APPLAY_PIXMAP(paint_pixmap_);

    effect_->setEnabled(effect_enabled_);
}

void ImageWindow::rotate(bool anticlockwise)
{
    QMatrix matrix;
    matrix.rotate((anticlockwise ? -1 : 1) * 90);
    APPLAY_PIXMAP(paint_pixmap_.transformed(matrix, Qt::FastTransformation));
}

void ImageWindow::mirror(bool h, bool v)
{
    APPLAY_PIXMAP(QPixmap::fromImage(paint_pixmap_.toImage().mirrored(h, v)));
}

void ImageWindow::contextMenuEvent(QContextMenuEvent *)
{
    QMenu *menu = new QMenu(this);

    auto copy = new QAction(tr("Copy image"));
    menu->addAction(copy);
    connect(copy, &QAction::triggered, this, &ImageWindow::copy);

    auto paste = new QAction(tr("Paste image"));
    menu->addAction(paste);
    connect(paste, &QAction::triggered, this, &ImageWindow::paste);

    menu->addSeparator();

    auto edit = new QAction(tr("Edit"));
    menu->addAction(edit);
    connect(edit, &QAction::triggered, [this](){
        if(thumbnail_) return;

        editing_ = true;
        edit_menu_.show();
        moveMenu();
    });

    menu->addSeparator();

    auto open = new QAction(tr("Open image..."));
    menu->addAction(open);
    connect(open, &QAction::triggered, this, &ImageWindow::open);

    auto save = new QAction(tr("Save as..."));
    menu->addAction(save);
    connect(save, &QAction::triggered, this, &ImageWindow::saveAs);

    menu->addSeparator();
    
    auto rotate = new QAction(tr("Rotate 90"));
    menu->addAction(rotate);
    connect(rotate, &QAction::triggered, this, &ImageWindow::rotate);

    auto effect = new QAction((effect_enabled_ ? tr("Hide ") : tr("Show ")) + tr("Shadow"));
    menu->addAction(effect);
    connect(effect, &QAction::triggered, this, &ImageWindow::effectEnabled);

    auto zoom = new QAction(tr("Zoom : ") + QString::number(static_cast<int>(scale_ * 100)) + "%");
    menu->addAction(zoom);

    auto opacity = new QAction(tr("Opacity : ") + QString::number(static_cast<int>(opacity_ * 100)) + "%");
    menu->addAction(opacity);

    auto recover = new QAction(tr("Recover"));
    connect(recover, &QAction::triggered, this, &ImageWindow::recover);
    menu->addAction(recover);

    menu->addSeparator();

    auto close = new QAction(tr("Close"));
    menu->addAction(close);
    connect(close, SIGNAL(triggered(bool)), this, SLOT(close()));

    menu->exec(QCursor::pos());
}

void ImageWindow::moveEvent(QMoveEvent *event)
{
    Q_UNUSED(event);
    moveMenu();
}

void ImageWindow::dropEvent(QDropEvent *event)
{
    auto path = event->mimeData()->urls()[0].toLocalFile();
    LOG(INFO) << path;

    scale_ = 1.0;
    original_pixmap_.load(path);
    paint_pixmap_ = original_pixmap_.copy();
    resize(window_size());

    event->acceptProposedAction();
}

void ImageWindow::dragEnterEvent(QDragEnterEvent *event)
{
    auto mimedata = event->mimeData();
    if(mimedata->hasUrls() && QString("jpg;png;jpeg;JPG;PNG;JPEG;bmp;BMP;ico;ICO").contains(QFileInfo(mimedata->urls()[0].fileName()).suffix()))
        event->acceptProposedAction();
}

void ImageWindow::moveMenu()
{
    auto rect = geometry().adjusted(shadow_r_, shadow_r_, -shadow_r_ - edit_menu_.width(), -shadow_r_ + 5);
    edit_menu_.move(rect.bottomRight());
    edit_menu_.setSubMenuShowBelow();
}

void ImageWindow::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Escape) {
        close();
    }

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
    // copy & paste
    connect(new QShortcut(Qt::CTRL + Qt::Key_C, this), &QShortcut::activated, this, &ImageWindow::copy);
    connect(new QShortcut(Qt::CTRL + Qt::Key_V, this), &QShortcut::activated, this, &ImageWindow::paste);

    // save & open
    connect(new QShortcut(Qt::CTRL + Qt::Key_S, this), &QShortcut::activated, this, &ImageWindow::saveAs);
    connect(new QShortcut(Qt::CTRL + Qt::Key_O, this), &QShortcut::activated, this, &ImageWindow::open);

    // rotate
    connect(new QShortcut(Qt::Key_R, this), &QShortcut::activated, [this]() { rotate(); });
    connect(new QShortcut(Qt::CTRL + Qt::Key_R, this), &QShortcut::activated, [this]() { rotate(true); });

    // flip
    connect(new QShortcut(Qt::Key_V, this), &QShortcut::activated, [this]() { mirror(false, true); });
    connect(new QShortcut(Qt::Key_H, this), &QShortcut::activated, [this]() { mirror(true, false); });

    // move
    connect(new QShortcut(Qt::Key_W, this), &QShortcut::activated, [=]() { setGeometry(geometry().adjusted(0, -1, 0, 0)); });
    connect(new QShortcut(Qt::Key_Up, this), &QShortcut::activated, [=]() { setGeometry(geometry().adjusted(0, -1, 0, 0)); });

    connect(new QShortcut(Qt::Key_S, this), &QShortcut::activated, [=]() { setGeometry(geometry().adjusted(0, 1, 0, 0)); });
    connect(new QShortcut(Qt::Key_Down, this), &QShortcut::activated, [=]() { setGeometry(geometry().adjusted(0, 1, 0, 0)); });

    connect(new QShortcut(Qt::Key_A, this), &QShortcut::activated, [=]() { setGeometry(geometry().adjusted(-1, 0, 0, 0)); });
    connect(new QShortcut(Qt::Key_Left, this), &QShortcut::activated, [=]() { setGeometry(geometry().adjusted(-1, 0, 0, 0)); });

    connect(new QShortcut(Qt::Key_D, this), &QShortcut::activated, [=]() { setGeometry(geometry().adjusted(1, 0, 0, 0)); });
    connect(new QShortcut(Qt::Key_Right, this), &QShortcut::activated, [=]() { setGeometry(geometry().adjusted(1, 0, 0, 0)); });
}
