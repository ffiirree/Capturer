#include "imagewindow.h"
#include <QKeyEvent>
#include <QMainWindow>
#include <QPainter>
#include <QGraphicsDropShadowEffect>
#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QShortcut>
#include "utils.h"

ImageWindow::ImageWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Dialog);
    setCursor(Qt::SizeAllCursor);

    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);

    auto effect = new QGraphicsDropShadowEffect(this);
    effect->setBlurRadius(10);
    effect->setOffset(0);
    effect->setColor(QColor(0, 125, 255));
    setGraphicsEffect(effect);

    regSCs();
}

void ImageWindow::fix(QPixmap image)
{
    pixmap_ = image;
    size_ = pixmap_.size();

    resize(size_ + QSize{10, 10});

    update();
    show();
}

void ImageWindow::mousePressEvent(QMouseEvent *event)
{
    begin_ = event->pos();
}

void ImageWindow::mouseMoveEvent(QMouseEvent* event)
{
    move(event->pos() - begin_ + pos());
}

void ImageWindow::wheelEvent(QWheelEvent *event)
{
    auto delta = (event->delta()/12000.0);         // +/-1%

    if(ctrl_) {
        opacity_ += delta;
        if(opacity_ < 0.01) opacity_ = 0.01;

        setWindowOpacity(opacity_);
    }
    else {
        scale_ += delta;
        scale_ = scale_ < 0.01 ? 0.01 : scale_;

        QRect rect({0, 0}, (size_) * scale_ + QSize{10, 10});
        rect.moveCenter(geometry().center());

        setGeometry(rect);
    }

    update();
}

void ImageWindow::paintEvent(QPaintEvent *)
{
    QPainter p{this};
    auto pixmap = pixmap_.scaled(size_ * scale_, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    p.drawPixmap(5, 5, pixmap);
    p.end();
}

void ImageWindow::copy()
{
    QApplication::clipboard()->setPixmap(pixmap_);
}
void ImageWindow::paste()
{
    pixmap_ = QApplication::clipboard()->pixmap();
    size_ = pixmap_.size();
    resize(size_ + QSize{10, 10});
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
        resize(size_ + QSize{10, 10});
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

void ImageWindow::contextMenuEvent(QContextMenuEvent *)
{
    QMenu *menu = new QMenu(this);

    auto copy = new QAction("复制");
    menu->addAction(copy);
    connect(copy, &QAction::triggered, this, &ImageWindow::copy);

    auto paste = new QAction("粘贴");
    menu->addAction(paste);
    connect(paste, &QAction::triggered, this, &ImageWindow::paste);

    menu->addSeparator();

//    auto edit = new QAction("编辑");
//    menu->addAction(edit);
//    connect(edit, &QAction::triggered, [this](){

//    });

    auto open = new QAction("打开...");
    menu->addAction(open);
    connect(open, &QAction::triggered, this, &ImageWindow::open);

    auto save = new QAction("另存为...");
    menu->addAction(save);
    connect(save, &QAction::triggered, this, &ImageWindow::saveAs);

    menu->addSeparator();

    auto zoom = new QAction("缩放比例: " + QString::number(static_cast<int>(scale_ * 100)) + "%");
    menu->addAction(zoom);

    auto opacity = new QAction("不透明度: " + QString::number(static_cast<int>(opacity_ * 100)) + "%");
    menu->addAction(opacity);

    menu->addSeparator();

    auto close = new QAction("关闭");
    menu->addAction(close);
    connect(close,SIGNAL(triggered(bool)),this,SLOT(close()));

    menu->exec(QCursor::pos());
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

void ImageWindow::regSCs()
{
    connect(new QShortcut(Qt::CTRL + Qt::Key_C, this), &QShortcut::activated, this, &ImageWindow::copy);
    connect(new QShortcut(Qt::CTRL + Qt::Key_V, this), &QShortcut::activated, this, &ImageWindow::paste);

    connect(new QShortcut(Qt::CTRL + Qt::Key_S, this), &QShortcut::activated, this, &ImageWindow::saveAs);
    connect(new QShortcut(Qt::CTRL + Qt::Key_O, this), &QShortcut::activated, this, &ImageWindow::open);
}
