#include "image-window.h"

#include "clipboard.h"
#include "config.h"
#include "logging.h"
#include "menu.h"

#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFileDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMimeData>
#include <QShortcut>
#include <QTextEdit>
#include <QVBoxLayout>

ImageWindow::ImageWindow(const std::shared_ptr<QMimeData>& data, QWidget *parent)
    : FramelessWindow(parent, Qt::Tool | Qt::WindowStaysOnTopHint)
{
    setWindowTitle("Image Window");

    setAcceptDrops(true);

    texture_ = new TextureWidget(this);
    setLayout(new QVBoxLayout());
    layout()->setSpacing(0);
    layout()->setContentsMargins({});
    layout()->addWidget(texture_);

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

    setWindowOpacity(opacity_);

    if (const auto pixmap = render(data_); pixmap) {
        pixmap_ = pixmap.value();
        texture_->present(pixmap_);

        if (mimedata->hasFormat(clipboard::MIME_TYPE_POINT)) {
            move(clipboard::deserialize<QPoint>(mimedata->data(clipboard::MIME_TYPE_POINT)));
        }
        else {
            const auto screen = QGuiApplication::screenAt(QCursor::pos())
                                    ? QGuiApplication::screenAt(QCursor::pos())
                                    : QGuiApplication::primaryScreen();
            move(screen->geometry().center() -
                 QPoint{ pixmap_.size().width() / 2, pixmap_.size().height() / 2 });
        }

        resize(pixmap_.size());
    }
}

void ImageWindow::present(const QPixmap& pixmap)
{
    pixmap_ = pixmap;

    texture_->present(pixmap_);

    auto _geometry = QRect{ {}, pixmap_.size() * scale_ };
    _geometry.moveCenter(geometry().center());
    setGeometry(_geometry);
}

void ImageWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    thumbnail_ = !thumbnail_;

    if (thumbnail_) {
        QRect rect({ 0, 0 }, THUMBNAIL_SIZE_);
        rect.moveCenter(event->globalPosition().toPoint());
        const auto geo = rect.intersected(geometry());

        texture_->present(grab(geo.translated((event->position() - event->globalPosition()).toPoint())));

        thumb_offset_ = geometry().center() - geo.center();
        setGeometry(geo);
    }
    else {
        auto geo = QRect({}, pixmap_.size().scaled(pixmap_.size() * scale_, Qt::KeepAspectRatio));
        texture_->present(pixmap_);

        geo.moveCenter(geometry().center() + thumb_offset_);
        setGeometry(geo);
    }
}

void ImageWindow::wheelEvent(QWheelEvent *event)
{
    const auto delta = event->angleDelta().y();
    if (event->modifiers() & Qt::CTRL) {
        opacity_ += (delta / 12000.0);
        opacity_  = std::clamp(opacity_, 0.01, 1.0);

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
            auto scanLine = reinterpret_cast<QRgb *>(img.scanLine(y));
            for (int x = 0; x < img.width(); ++x) {
                const QRgb pixel = *scanLine;
                const auto ci    = qGray(pixel);
                *scanLine        = qRgba(ci, ci, ci, qAlpha(pixel));
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
    const auto filename = QFileDialog::getOpenFileName(this, tr("Open Image"), config::snip::path,
                                                       "Image Files(*.png *.jpg *.jpeg *.bmp *.svg)");
    if (!filename.isEmpty()) {
        present(QPixmap(filename));
    }
}

void ImageWindow::saveAs()
{
    const QString default_filename =
        "Capturer_" + QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss_zzz") + ".png";

    const auto filename = QFileDialog::getSaveFileName(
        this, tr("Save Image"), config::snip::path + QDir::separator() + default_filename,
        "PNG(*.png);;JPEG(*.jpg *.jpeg);;BMP(*.bmp)");

    if (!filename.isEmpty()) {
        const QFileInfo fileinfo(filename);
        config::snip::path = fileinfo.absoluteDir().path();

        pixmap_.save(filename);
    }
}

// clang-format off
void ImageWindow::initContextMenu()
{
    context_menu_ = new Menu(this);

    addAction(context_menu_->addAction(tr("Copy"),  QKeySequence::Copy,     [this] { clipboard::push(data_); }));
    addAction(context_menu_->addAction(tr("Paste"), QKeySequence::Paste,    [this] { paste(); }));

    context_menu_->addSeparator();

    addAction(context_menu_->addAction(tr("Open Image..."), QKeySequence::Open, this, &ImageWindow::open));
    addAction(context_menu_->addAction(tr("Save As..."),    QKeySequence::Save, this, &ImageWindow::saveAs));

    context_menu_->addSeparator();

    addAction(context_menu_->addAction(tr("Grayscale"),     QKeySequence(Qt::Key_G),            [this] { present(grayscale(pixmap_)); }));
    addAction(context_menu_->addAction(tr("Rotate +90"),    QKeySequence(Qt::Key_R),            [this] { present(pixmap_.transformed(QTransform().rotate(+1 * 90.0))); }));
    addAction(context_menu_->addAction(tr("Rotate -90"),    QKeySequence(Qt::CTRL | Qt::Key_R), [this] { present(pixmap_.transformed(QTransform().rotate(-1 * 90.0))); }));
    addAction(context_menu_->addAction(tr("H Flip"),        QKeySequence(Qt::Key_H),            [this] { present(pixmap_.transformed(QTransform().scale(-1, +1))); }));
    addAction(context_menu_->addAction(tr("V Flip"),        QKeySequence(Qt::Key_V),            [this] { present(pixmap_.transformed(QTransform().scale(+1, -1))); }));

    context_menu_->addSeparator();

    const auto sub_menu = new Menu(tr("Background"), this);

    const auto group = new QActionGroup(this);
    group->addAction(sub_menu->addAction(tr("White"),        [this] { setStyleSheet("ImageWindow{ background: white; }"); }))->setCheckable(true);
    group->addAction(sub_menu->addAction(tr("Gray"),         [this] { setStyleSheet("ImageWindow{ background: gray; }"); }))->setCheckable(true);
    group->addAction(sub_menu->addAction(tr("Black"),        [this] { setStyleSheet("ImageWindow{ background: black; }"); }))->setCheckable(true);
    group->actions()[config::definite_theme() == "light" ? 0 : 2]->trigger();
    context_menu_->addMenu(sub_menu);

    zoom_action_    = context_menu_->addAction(tr("Zoom : ")    + QString::number(static_cast<int>(scale_ * 100)) + "%");
    opacity_action_ = context_menu_->addAction(tr("Opacity : ") + QString::number(static_cast<int>(opacity_ * 100)) + "%");
    context_menu_->addAction(tr("Transparent Input"),       [this] { toggleTransparentInput(); setWindowOpacity(opacity_ == 1.0 ? 0.99 : opacity_); });

    context_menu_->addSeparator();

    context_menu_->addAction(tr("Recover"), [this] { preview(data_); });
    context_menu_->addAction(tr("Close"), QKeySequence(Qt::Key_Escape), this, &ImageWindow::close);
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

void ImageWindow::closeEvent(QCloseEvent *event)
{
    data_->setData(clipboard::MIME_TYPE_STATUS, "N");
    return FramelessWindow::closeEvent(event);
}

void ImageWindow::registerShortcuts()
{
    // clang-format off
    connect(new QShortcut(Qt::Key_Escape, this),    &QShortcut::activated, [this] { close(); });

    // move
    connect(new QShortcut(Qt::Key_W, this),         &QShortcut::activated, [this] { setGeometry(geometry().translated(0, -1)); });
    connect(new QShortcut(Qt::Key_Up, this),        &QShortcut::activated, [this] { setGeometry(geometry().translated(0, -1)); });

    connect(new QShortcut(Qt::Key_S, this),         &QShortcut::activated, [this] { setGeometry(geometry().translated(0, +1)); });
    connect(new QShortcut(Qt::Key_Down, this),      &QShortcut::activated, [this] { setGeometry(geometry().translated(0, +1)); });

    connect(new QShortcut(Qt::Key_A, this),         &QShortcut::activated, [this] { setGeometry(geometry().translated(-1, 0)); });
    connect(new QShortcut(Qt::Key_Left, this),      &QShortcut::activated, [this] { setGeometry(geometry().translated(-1, 0)); });

    connect(new QShortcut(Qt::Key_D, this),         &QShortcut::activated, [this] { setGeometry(geometry().translated(+1, 0)); });
    connect(new QShortcut(Qt::Key_Right, this),     &QShortcut::activated, [this] { setGeometry(geometry().translated(+1, 0)); });
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
    if (mimedata->hasUrls() && mimedata->urls().size() == 1) {
        const auto path = QFileInfo(mimedata->urls()[0].fileName());
        if (mimedata->urls()[0].isLocalFile() && QFileInfo(mimedata->urls()[0].toLocalFile()).isFile() &&
            QString("jpg;jpeg;png;bmp;ico;svg;webp")
                .split(';')
                .contains(path.suffix(), Qt::CaseInsensitive)) {
            return QPixmap(mimedata->urls()[0].toLocalFile());
        }
    }

    // TODO: text
    if (mimedata->hasUrls() &&
        QString("txt;json;html")
            .split(';')
            .contains(QFileInfo(mimedata->urls()[0].fileName()).suffix(), Qt::CaseInsensitive)) {}

    // TODO: microsoft rich text format
    if (mimedata->hasFormat("text/rtf") || mimedata->hasFormat("text/richtext") ||
        mimedata->hasFormat("application/x-qt-windows-mime;value=\"Rich Text Format\"")) {}

    // 4. text
    if (mimedata->hasText()) {
        auto text = mimedata->text();

        // remove the last '\n'
        if (mimedata->hasUrls() && mimedata->urls().size() > 1 && text.back() == '\n')
            text = text.mid(0, text.size() - 1);

        QLabel label(text);
        label.setWordWrap(false);
        label.setMargin(10);
        label.setStyleSheet(config::definite_theme() == "light" ? "background-color:white; color: black;"
                                                                : "background-color:black; color: white;");
        return label.grab();
    }

    // 5. color
    if (mimedata->hasColor()) {}

    logw("unsupported");
    return std::nullopt;
}