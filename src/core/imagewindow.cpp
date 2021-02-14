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


    auto W = new QShortcut(QKeySequence("W"), this);
    auto move_up = new QShortcut(Qt::Key_Up, this);
    connect(W, &QShortcut::activated, [=]() { setGeometry(geometry().adjusted(0, -1, 0, 0)); });
    connect(move_up, &QShortcut::activated, [=]() { setGeometry(geometry().adjusted(0, -1, 0, 0)); });

    auto S = new QShortcut(QKeySequence("S"), this);
    auto move_down = new QShortcut(Qt::Key_Down, this);
    connect(S, &QShortcut::activated, [=]() { setGeometry(geometry().adjusted(0, 1, 0, 0)); });
    connect(move_down, &QShortcut::activated, [=]() { setGeometry(geometry().adjusted(0, 1, 0, 0)); });

    auto A = new QShortcut(QKeySequence("A"), this);
    auto move_left = new QShortcut(Qt::Key_Left, this);
    connect(A, &QShortcut::activated, [=]() { setGeometry(geometry().adjusted(-1, 0, 0, 0)); });
    connect(move_left, &QShortcut::activated, [=]() { setGeometry(geometry().adjusted(-1, 0, 0, 0)); });

    auto D = new QShortcut(QKeySequence("D"), this);
    auto move_right = new QShortcut(Qt::Key_Right, this);
    connect(D, &QShortcut::activated, [=]() { setGeometry(geometry().adjusted(1, 0, 0, 0)); });
    connect(move_right, &QShortcut::activated, [=]() { setGeometry(geometry().adjusted(1, 0, 0, 0)); });
}

void ImageWindow::fix()
{
    if(status_ == 1) return;

    status_ = 1;

    size_ = pixmap_.size();

    resize(size_ + QSize{ shadow_r_ * 2, shadow_r_ * 2 });

    update();
    QWidget::show();
    move(original_pos_ - QPoint{shadow_r_, shadow_r_});
}

void ImageWindow::mousePressEvent(QMouseEvent *event)
{
    if(editing_) {

    } else {
        // thumbnail_
        if(event->button() == Qt::LeftButton && event->type() == QEvent::MouseButtonDblClick) {
            thumbnail_ = !thumbnail_;
            QRect rect({0, 0}, (thumbnail_ ? QSize{125, 125} : size_ * scale_) + QSize{shadow_r_ * 2, shadow_r_ * 2});
            rect.moveCenter(geometry().center());
            setGeometry(rect);

            update();
        }

        setCursor(Qt::SizeAllCursor);
        begin_ = event->globalPos();
    }
}

void ImageWindow::mouseMoveEvent(QMouseEvent* event)
{
    if(editing_) {

    } else {
        move(event->globalPos() - begin_ + pos());
        begin_ = event->globalPos();
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

        QRect rect({0, 0}, (size_) * scale_ + QSize{shadow_r_ * 2, shadow_r_ * 2});
        rect.moveCenter(geometry().center());

        setGeometry(rect);
    }

    update();
}

void ImageWindow::paintEvent(QPaintEvent *)
{
    auto pixmap = pixmap_.scaled(size_ * scale_, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if(thumbnail_) {
        auto center = pixmap.rect().center();
        pixmap = pixmap.copy(center.x() - 62, center.y() - 62, 125, 125);
    }

    painter_.begin(this);
    painter_.drawPixmap(shadow_r_, shadow_r_, pixmap);
    painter_.end();
}

void ImageWindow::copy()
{
    QApplication::clipboard()->setPixmap(pixmap_);
}

void ImageWindow::paste()
{
    pixmap_ = QApplication::clipboard()->pixmap();
    size_ = pixmap_.size();
    resize(size_ + QSize{ shadow_r_ * 2, shadow_r_ * 2});
}

void ImageWindow::open()
{
    auto filename = QFileDialog::getOpenFileName(this,
                                                 tr("Open Image"),
                                                 QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
                                                 "Image Files(*.png *.jpg *.jpeg *.bmp)");
    if(!filename.isEmpty()) {
        pixmap_ = QPixmap(filename);
        size_ = pixmap_.size();
        resize(size_ + QSize{ shadow_r_ * 2, shadow_r_ * 2});
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
        pixmap_.save(filename);
    }
#elif __linux__
    auto filename = default_filepath + QDir::separator() + default_filename;

    pixmap_.save(filename);
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
    QRect rect({0, 0}, size_ + QSize{shadow_r_ * 2, shadow_r_ * 2});
    rect.moveCenter(geometry().center());
    setGeometry(rect);

    update();
}

void ImageWindow::effectEnabled()
{
    effect_enabled_ = !effect_enabled_;
    shadow_r_ = effect_enabled_ ? DEFAULT_SHADOW_R_ : 0;

    QRect rect({0, 0}, (size_) * scale_ + QSize{shadow_r_ * 2, shadow_r_ * 2});
    rect.moveCenter(geometry().center());

    setGeometry(rect);

    effect_->setEnabled(effect_enabled_);
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
    pixmap_.load(path);
    size_ = pixmap_.size();
    resize(size_ + QSize{ shadow_r_ * 2, shadow_r_ * 2 });
    repaint();

    event->acceptProposedAction();
}

void ImageWindow::dragEnterEvent(QDragEnterEvent *event)
{
    auto mimedata = event->mimeData();
    if(mimedata->hasUrls() && QString("jpg;png;jpeg;JPG;PNG;JPEG;bmp;BMP").contains(QFileInfo(mimedata->urls()[0].fileName()).suffix()))
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
    connect(new QShortcut(Qt::CTRL + Qt::Key_C, this), &QShortcut::activated, this, &ImageWindow::copy);
    connect(new QShortcut(Qt::CTRL + Qt::Key_V, this), &QShortcut::activated, this, &ImageWindow::paste);

    connect(new QShortcut(Qt::CTRL + Qt::Key_S, this), &QShortcut::activated, this, &ImageWindow::saveAs);
    connect(new QShortcut(Qt::CTRL + Qt::Key_O, this), &QShortcut::activated, this, &ImageWindow::open);
}
