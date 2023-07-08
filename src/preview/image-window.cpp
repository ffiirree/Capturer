#include "image-window.h"

#include "clipboard.h"
#include "config.h"
#include "logging.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFileDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMimeData>
#include <QPainter>
#include <QShortcut>
#include <QTextEdit>
#include <QVBoxLayout>

ImageWindow::ImageWindow(const std::shared_ptr<QMimeData>& data, QWidget *parent)
    : FramelessWindow(parent)
{
    setWindowFlags((windowFlags() & ~Qt::Window) | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose);

    setWindowTitle("Image Window");

    setAcceptDrops(true);

    preview_ = new TextureWidget(this);
    setLayout(new QVBoxLayout());
    layout()->setSpacing(0);
    layout()->setContentsMargins({});
    layout()->addWidget(preview_);

    preview(data);

    registerShortcuts();
    initContextMenu();
}

void ImageWindow::preview(const std::shared_ptr<QMimeData>& mimedata)
{
    data_ = mimedata;
    data_->setData(clipboard::MIME_TYPE_STATUS, "P");

    scale_     = 1.0;
    opacity_   = 1.0;
    thumbnail_ = false;

    if (auto pixmap = render(data_); pixmap) {
        pixmap_ = pixmap.value();
        preview_->present(pixmap_);

        if (mimedata->hasFormat(clipboard::MIME_TYPE_POINT)) {
            move(clipboard::deserialize<QPoint>(mimedata->data(clipboard::MIME_TYPE_POINT)));
        }

        resize(pixmap_.size());
    }
}

void ImageWindow::present(const QPixmap& pixmap)
{
    pixmap_ = pixmap;

    preview_->present(pixmap_);

    auto _geometry = QRect{ {}, pixmap_.size() * scale_ };
    _geometry.moveCenter(geometry().center());
    setGeometry(_geometry);
}

void ImageWindow::mouseDoubleClickEvent(QMouseEvent *)
{
    thumbnail_ = !thumbnail_;

    QRect _geometry{};

    if (thumbnail_) {
        _geometry.setSize(THUMBNAIL_SIZE_);
        preview_->present(pixmap_.copy(_geometry));
    }
    else {
        _geometry.setSize(pixmap_.size().scaled(pixmap_.size() * scale_, Qt::KeepAspectRatio));
        preview_->present(pixmap_);
    }

    _geometry.moveCenter(geometry().center());
    setGeometry(_geometry);
}

void ImageWindow::wheelEvent(QWheelEvent *event)
{
    auto delta = event->angleDelta().y();
    if (ctrl_) {
        opacity_ += (delta / 12000.0);
        opacity_ = std::clamp(opacity_, 0.01, 1.0);

        setWindowOpacity(opacity_);
    }
    else if (!thumbnail_) {
        scale_ = delta > 0 ? scale_ * 1.05 : scale_ / 1.05;
        scale_ = std::max(std::min(125.0 / std::min(pixmap_.size().width(), pixmap_.size().height()), 1.0),
                          scale_);

        auto _geometry = QRect{ {}, pixmap_.size().scaled(pixmap_.size() * scale_, Qt::KeepAspectRatio) };
        _geometry.moveCenter(geometry().center());
        setGeometry(_geometry);
    }
}

static QPixmap grayscale(const QPixmap& pixmap)
{
    if (pixmap.hasAlpha()) {
        auto img = pixmap.toImage();
        for (int y = 0; y < img.height(); ++y) {
            QRgb *scanLine = (QRgb *)img.scanLine(y);
            for (int x = 0; x < img.width(); ++x) {
                QRgb pixel = *scanLine;
                auto ci    = qGray(pixel);
                *scanLine  = qRgba(ci, ci, ci, qAlpha(pixel));
                ++scanLine;
            }
        }
        return QPixmap::fromImage(img);
    }

    return QPixmap::fromImage(pixmap.toImage().convertToFormat(QImage::Format::Format_Grayscale8));
}

void ImageWindow::paste()
{
    preview(std::shared_ptr<QMimeData>(clipboard::clone(QApplication::clipboard()->mimeData())));
}

void ImageWindow::open()
{
    auto filename = QFileDialog::getOpenFileName(
        this, tr("Open Image"), QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        "Image Files(*.png *.jpg *.jpeg *.bmp *.svg)");
    if (!filename.isEmpty()) {
        present(QPixmap(filename));
    }
}

void ImageWindow::saveAs()
{
    QString default_filename =
        "Capturer_" + QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss_zzz") + ".png";

    QString filename = QFileDialog::getSaveFileName(this, tr("Save Image"),
                                                    save_path_ + QDir::separator() + default_filename,
                                                    "PNG(*.png);;JPEG(*.jpg *.jpeg);;BMP(*.bmp)");

    if (!filename.isEmpty()) {
        QFileInfo fileinfo(filename);
        save_path_ = fileinfo.absoluteDir().path();

        pixmap_.save(filename);
    }
}

// clang-format off
void ImageWindow::initContextMenu()
{
    context_menu_ = new Menu(this);

    addAction(context_menu_->addAction(tr("Copy"),          [this]() { clipboard::push(data_); },   QKeySequence::Copy));
    addAction(context_menu_->addAction(tr("Paste"),         [this]() { paste(); },                  QKeySequence::Paste));

    context_menu_->addSeparator();

    addAction(context_menu_->addAction(tr("Open Image..."), this, &ImageWindow::open,   QKeySequence::Open));
    addAction(context_menu_->addAction(tr("Save As..."),    this, &ImageWindow::saveAs, QKeySequence::Save));

    context_menu_->addSeparator();

    addAction(context_menu_->addAction(tr("Grayscale"),     [this]() { present(grayscale(pixmap_));  },                                 QKeySequence(Qt::Key_G)));
    addAction(context_menu_->addAction(tr("Rotate +90"),    [this]() { present(pixmap_.transformed(QTransform().rotate(+1 * 90.0))); }, QKeySequence(Qt::Key_R)));
    addAction(context_menu_->addAction(tr("Rotate -90"),    [this]() { present(pixmap_.transformed(QTransform().rotate(-1 * 90.0))); }, QKeySequence(Qt::CTRL | Qt::Key_R)));
    addAction(context_menu_->addAction(tr("H Flip"),        [this]() { present(pixmap_.transformed(QTransform().scale(-1, +1))); },     QKeySequence(Qt::Key_H)));
    addAction(context_menu_->addAction(tr("V Flip"),        [this]() { present(pixmap_.transformed(QTransform().scale(+1, -1))); },     QKeySequence(Qt::Key_V)));

    context_menu_->addSeparator();

    //auto sub_menu = new Menu(tr("Background"), this);
    //context_menu_->addMenu(sub_menu);

    zoom_action_ = context_menu_->addAction(tr("Zoom : ") + QString::number(static_cast<int>(scale_ * 100)) + "%");
    opacity_action_ = context_menu_->addAction(tr("Opacity : ") + QString::number(static_cast<int>(opacity_ * 100)) + "%");
    context_menu_->addAction(tr("Recover"), [this]() { preview(data_); });

    context_menu_->addSeparator();

    context_menu_->addAction(tr("Close"), this, &ImageWindow::close, QKeySequence(Qt::Key_Escape));
}
// clang-format on

void ImageWindow::contextMenuEvent(QContextMenuEvent *event)
{
    zoom_action_->setText(tr("Zoom : ") + QString::number(static_cast<int>(scale_ * 100)) + "%");
    opacity_action_->setText(tr("Opacity : ") + QString::number(static_cast<int>(opacity_ * 100)) + "%");

    context_menu_->exec(event->globalPos());
}

void ImageWindow::dropEvent(QDropEvent *event)
{
    preview(std::shared_ptr<QMimeData>(clipboard::clone(event->mimeData())));

    event->acceptProposedAction();
}

void ImageWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (is_supported(event->mimeData())) {
        event->acceptProposedAction();
    }
}

void ImageWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Control) {
        ctrl_ = true;
    }
}

void ImageWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Control) {
        ctrl_ = false;
    }
}

void ImageWindow::closeEvent(QCloseEvent *event)
{
    data_->setData(clipboard::MIME_TYPE_STATUS, "N");
    return FramelessWindow::closeEvent(event);
}

void ImageWindow::registerShortcuts()
{
    // clang-format off
    connect(new QShortcut(Qt::Key_Escape, this),    &QShortcut::activated, [this]() { close(); });

    // move
    connect(new QShortcut(Qt::Key_W, this),         &QShortcut::activated, [this]() { setGeometry(geometry().translated(0, -1)); });
    connect(new QShortcut(Qt::Key_Up, this),        &QShortcut::activated, [this]() { setGeometry(geometry().translated(0, -1)); });

    connect(new QShortcut(Qt::Key_S, this),         &QShortcut::activated, [this]() { setGeometry(geometry().translated(0, +1)); });
    connect(new QShortcut(Qt::Key_Down, this),      &QShortcut::activated, [this]() { setGeometry(geometry().translated(0, +1)); });

    connect(new QShortcut(Qt::Key_A, this),         &QShortcut::activated, [this]() { setGeometry(geometry().translated(-1, 0)); });
    connect(new QShortcut(Qt::Key_Left, this),      &QShortcut::activated, [this]() { setGeometry(geometry().translated(-1, 0)); });

    connect(new QShortcut(Qt::Key_D, this),         &QShortcut::activated, [this]() { setGeometry(geometry().translated(+1, 0)); });
    connect(new QShortcut(Qt::Key_Right, this),     &QShortcut::activated, [this]() { setGeometry(geometry().translated(+1, 0)); });
    // clang-format on
}

bool ImageWindow::is_supported(const QMimeData *mimedata)
{
    return mimedata->hasImage() || mimedata->hasText();
}

std::optional<QPixmap> ImageWindow::render(const std::shared_ptr<QMimeData>& mimedata)
{
    // 1. image
    if (mimedata->hasImage()) {
        return mimedata->imageData().value<QPixmap>();
    }

    // 2. html
    if (mimedata->hasHtml()) {
        QTextEdit view;
        view.setFrameShape(QFrame::NoFrame);
        view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view.setLineWrapMode(QTextEdit::NoWrap);

        view.setHtml(mimedata->html());
        view.document()->setDocumentMargin(0);
        view.setFixedSize(view.document()->size().toSize());

        return view.grab();
    }

    // 3. urls
    if (mimedata->hasUrls() && mimedata->urls().size() == 1 &&
        QString("jpg;jpeg;png;bmp;ico;svg")
            .contains(QFileInfo(mimedata->urls()[0].fileName()).suffix(), Qt::CaseInsensitive)) {
        return QPixmap(mimedata->urls()[0].toLocalFile());
    }

    // TODO: text
    if (mimedata->hasUrls() &&
        QString("txt;json;html")
            .contains(QFileInfo(mimedata->urls()[0].fileName()).suffix(), Qt::CaseInsensitive)) {}

    // TODO: microsoft rich text format
    if (mimedata->hasFormat("text/rtf") || mimedata->hasFormat("text/richtext") ||
        mimedata->hasFormat("application/x-qt-windows-mime;value=\"Rich Text Format\"")) {}

    // 4. text
    if (mimedata->hasText()) {
        QLabel label(mimedata->text());
        label.setWordWrap(false);
        label.setMargin(10);
        label.setStyleSheet(Config::theme() == "light" ? "background-color:white; color: black;"
                                                       : "background-color:black; color: white;");
        return label.grab();
    }

    // 5. color
    if (mimedata->hasColor()) {}

    LOG(WARNING) << "unsupported";
    return std::nullopt;
}