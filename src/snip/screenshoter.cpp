#include "screenshoter.h"

#include "graphicsitem.h"
#include "graphicstextitem.h"
#include "imageeditmenu.h"
#include "magnifier.h"
#include "selector.h"

#include <QApplication>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QDesktopWidget>
#endif
#include "logging.h"

#include <QClipboard>
#include <QDateTime>
#include <QFileDialog>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QMimeData>
#include <QMouseEvent>
#include <QPalette>
#include <QScreen>
#include <QShortcut>

ScreenShoter::ScreenShoter(QWidget *parent)
    : QGraphicsView(parent)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                   Qt::BypassWindowManagerHint);

    // setAttribute(Qt::WA_TranslucentBackground);
    // setStyleSheet("background: transparent");

    setFrameStyle(QGraphicsView::NoFrame);
    setContentsMargins({ 0, 0, 0, 0 });
    setViewportMargins({ 0, 0, 0, 0 });

    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    selector_  = new Selector(this);
    menu_      = new ImageEditMenu(this);
    magnifier_ = new Magnifier(this);
    setScene(new QGraphicsScene(this));

    // connect(canvas_, &Canvas::changed, [this]() { update(); });
    // connect(canvas_, &Canvas::cursorChanged, [this]() { setCursor(canvas_->getCursorShape()); });

    // connect(selector_, &Selector::locked, canvas_, &Canvas::enable);
    // connect(selector_, &Selector::captured, canvas_, &Canvas::disable);

    // menu_->installEventFilter(this);
    // selector_->installEventFilter(this);

    connect(menu_, &ImageEditMenu::save, this, &ScreenShoter::save);
    connect(menu_, &ImageEditMenu::ok, this, &ScreenShoter::copy);
    connect(menu_, &ImageEditMenu::pin, this, &ScreenShoter::pin);
    connect(menu_, &ImageEditMenu::exit, this, &ScreenShoter::exit);

    connect(menu_, &ImageEditMenu::graphChanged, [this](graph_t graph) {
        if (graph != graph_t::none) {
            selector_->status(SelectorStatus::LOCKED);
        }
    });

    // show menu
    connect(selector_, &Selector::captured, [this]() {
        menu_->show();
        moveMenu();
    });

    // move menu
    connect(selector_, &Selector::moved, this, &ScreenShoter::moveMenu);
    connect(selector_, &Selector::resized, this, &ScreenShoter::moveMenu);
    connect(selector_, &Selector::statusChanged, [this](SelectorStatus status) {
        if ((status > SelectorStatus::READY && status < SelectorStatus::CAPTURED) ||
            status == SelectorStatus::RESIZING)
            magnifier_->show();
        else
            magnifier_->close();
    });

    // shortcuts
    registerShortcuts();
}

void ScreenShoter::start()
{
    if (isVisible()) return;

    auto virtual_geometry = probe::graphics::virtual_screen_geometry();
    auto captured_pixmap  = QGuiApplication::primaryScreen()->grabWindow(
        probe::graphics::virtual_screen().handle, virtual_geometry.x, virtual_geometry.y,
        static_cast<int>(virtual_geometry.width), static_cast<int>(virtual_geometry.height));

    // geometry
    setGeometry(QRect{ virtual_geometry });
    scene()->setSceneRect(geometry());
    selector_->setGeometry(geometry());

    auto item = scene()->addRect(10, 10, 1'000, 1'000);
    item->setFlag(QGraphicsItem::ItemIsFocusable);
    item->setFlag(QGraphicsItem::ItemIsMovable);
    item->setFlag(QGraphicsItem::ItemIsSelectable);

    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

    // background
    setBackgroundBrush(captured_pixmap);
    setCacheMode(QGraphicsView::CacheBackground);

    selector_->start(probe::graphics::window_filter_t::visible |
                     probe::graphics::window_filter_t::children);

    show();
    activateWindow(); //  Qt::BypassWindowManagerHint: no keyboard input unless call
                      //  QWidget::activateWindow()
}

void ScreenShoter::exit()
{
    magnifier_->hide();

    menu_->reset();
    menu_->hide();

    QWidget::close();
}

// bool ScreenShoter::eventFilter(QObject *obj, QEvent *event)
//{
//     if (obj == menu_) {
//         switch (event->type()) {
//         case QEvent::KeyPress: keyPressEvent(dynamic_cast<QKeyEvent *>(event)); return true;
//         case QEvent::KeyRelease:
//             keyReleaseEvent(dynamic_cast<QKeyEvent *>(event));
//             return true;
//             // default: return Selector::eventFilter(obj, event);
//         }
//     }
//     else {
//         // return Selector::eventFilter(obj, event);
//     }
// }

void ScreenShoter::mousePressEvent(QMouseEvent *event)
{
    auto pos = event->pos();

    QGraphicsView::mousePressEvent(event);

    if (event->button() == Qt::LeftButton && scene()->focusItem() == nullptr) {
        LOG(INFO) << "creating";

        QGraphicsItem *item{};
        if (menu_->graph() == graph_t::text) {
            item = new GraphicsTextItem("hello", pos);
        }
        else {
            editstatus_ = EditStatus::GRAPH_CREATING;

            item = new GraphicsItem(menu_->graph(), { pos, pos });
        }

        scene()->addItem(item);
        item->setFocus();
    }
}

void ScreenShoter::mouseMoveEvent(QMouseEvent *event)
{
    auto pos = event->pos();

    if ((event->buttons() & Qt::LeftButton) && editstatus_ == EditStatus::GRAPH_CREATING) {
        LOG(INFO) << "creating..";
        reinterpret_cast<GraphicsItem *>(scene()->focusItem())->pushVertex(pos);
    }

    QGraphicsView::mouseMoveEvent(event);
}

void ScreenShoter::mouseReleaseEvent(QMouseEvent *event)
{
    if ((event->button() == Qt::LeftButton) && editstatus_ == EditStatus::GRAPH_CREATING) {
        undo_.emplace_back(std::make_shared<CreateCommand>(scene()->focusItem()));
        editstatus_ = EditStatus::READY;
        LOG(INFO) << "created";
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void ScreenShoter::keyPressEvent(QKeyEvent *event)
{
    // hotkey 'Space': move the selector while editing.
    if (selector_->status() == SelectorStatus::LOCKED && event->key() == Qt::Key_Space &&
        !event->isAutoRepeat()) {
        selector_->status(SelectorStatus::CAPTURED);
    }

    QGraphicsView::keyPressEvent(event);
}

void ScreenShoter::keyReleaseEvent(QKeyEvent *event)
{
    // stop moving the selector while editing
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        selector_->status(SelectorStatus::LOCKED);
    }

    QGraphicsView::keyReleaseEvent(event);
}

void ScreenShoter::mouseDoubleClickEvent(QMouseEvent *event)
{
    //if (event->button() == Qt::LeftButton && selector_->status() >= SelectorStatus::CAPTURED) {
    //    save2clipboard(snip(), false);
    //    exit();
    //}

    QGraphicsView::mouseDoubleClickEvent(event);
}

void ScreenShoter::moveMenu()
{
    auto area  = selector_->selected();
    auto right = area.right() - menu_->width() + 1;
    if (right < geometry().left()) right = geometry().left();

    if (area.bottom() + (menu_->height() * 2 + 8 /*margin*/) < geometry().bottom()) {
        menu_->move(right, area.bottom() + 6);
        menu_->setSubMenuShowBelow();
    }
    else if (area.top() - (menu_->height() * 2 + 8) > 0) {
        menu_->move(right, area.top() - menu_->height() - 6);
        menu_->setSubMenuShowAbove();
    }
    else {
        menu_->move(right, area.bottom() - menu_->height() - 6);
        menu_->setSubMenuShowAbove();
    }
}

// void ScreenShoter::paintEvent(QPaintEvent *event)
//{
//     // 2. window
//     // QPainter painter_(this);
//     // painter_.drawPixmap(0, 0, canvas_->pixmap());
//
//     //// 3. modifying
//     // canvas_->drawModifying(&painter_);
//     // canvas_->modified(PaintType::UNMODIFIED);
//     // painter_.end();
//
//     // Selector::paintEvent(event);
// }

QPixmap ScreenShoter::snip()
{
    history_.push_back(selector_->prey());
    history_idx_ = history_.size() - 1;
    return {};
    // return canvas_->pixmap().copy(
    //     selector_->selected().translated(-QRect(probe::graphics::virtual_screen_geometry()).topLeft()));
}

void ScreenShoter::save2clipboard(const QPixmap& image, bool pinned)
{
    auto mimedata = new QMimeData();
    mimedata->setImageData(QVariant(image));
    mimedata->setData("application/x-snipped", QByteArray().append(pinned ? "pinned" : "copied"));
    mimedata->setImageData(QVariant(image));
    // Ownership of the data is transferred to the clipboard:
    // https://doc.qt.io/qt-5/qclipboard.html#setMimeData
    QApplication::clipboard()->setMimeData(mimedata);
}

void ScreenShoter::save()
{
    QString default_filename =
        "Capturer_" + QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss_zzz") + ".png";

#ifdef _WIN32
    QString filename = QFileDialog::getSaveFileName(this, tr("Save Image"),
                                                    save_path_ + QDir::separator() + default_filename,
                                                    "PNG(*.png);;JPEG(*.jpg *.jpeg);;BMP(*.bmp)");
#elif __linux__
    QString filename = save_path_ + QDir::separator() + default_filename;
#endif

    if (!filename.isEmpty()) {
        QFileInfo fileinfo(filename);
        save_path_ = fileinfo.absoluteDir().path();

        snip().save(filename);

        emit SHOW_MESSAGE("Capturer<PICTURE>", "Path: " + filename);
        exit();
    }
}

void ScreenShoter::copy()
{
    save2clipboard(snip(), false);

    exit();
}

void ScreenShoter::pin()
{
    auto snipped = snip();

    emit pinSnipped(snipped, { selector_->selected().topLeft() });

    save2clipboard(snipped, true);

    exit();
}

void ScreenShoter::updateTheme()
{
    const auto& style = Config::instance()["snip"]["selector"];
    selector_->setBorderStyle(QPen{
        style["border"]["color"].get<QColor>(),
        static_cast<qreal>(style["border"]["width"].get<int>()),
        style["border"]["style"].get<Qt::PenStyle>(),
    });

    selector_->setMaskStyle(style["mask"]["color"].get<QColor>());
}

void ScreenShoter::registerShortcuts()
{
    connect(new QShortcut(Qt::CTRL | Qt::Key_S, this), &QShortcut::activated, [this]() {
        if (any(selector_->status() & SelectorStatus::CAPTURED) ||
            any(selector_->status() & SelectorStatus::LOCKED)) {
            save();
        }
    });

    connect(new QShortcut(Qt::Key_P, this), &QShortcut::activated, [this]() {
        if (selector_->status() == SelectorStatus::CAPTURED ||
            selector_->status() == SelectorStatus::LOCKED) {
            pin();
        }
    });

    connect(new QShortcut(Qt::Key_Return, this), &QShortcut::activated, [this]() {
        copy();
        exit();
    });
    connect(new QShortcut(Qt::Key_Enter, this), &QShortcut::activated, [this]() {
        copy();
        exit();
    });

    connect(new QShortcut(Qt::Key_Escape, this), &QShortcut::activated, [this]() { exit(); });

    connect(new QShortcut(Qt::Key_Tab, this), &QShortcut::activated, [this]() {
        if (magnifier_->isVisible()) {
            magnifier_->toggleColorFormat();
        }
    });

    connect(new QShortcut(Qt::CTRL | Qt::Key_Z, this), &QShortcut::activated, [this]() {
        if (!undo_.empty()) {
            undo_.back()->undo();
            redo_.push_back(undo_.back());
            undo_.pop_back();
        }
    });
    connect(new QShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_Z, this), &QShortcut::activated, [this]() {
        if (!redo_.empty()) {
            redo_.back()->redo();
            undo_.push_back(redo_.back());
            redo_.pop_back();
        }
    });

    connect(new QShortcut(Qt::Key_PageUp, this), &QShortcut::activated, [=, this]() {
        if (history_idx_ < history_.size()) {
            selector_->select(history_[history_idx_]);
            selector_->status(SelectorStatus::CAPTURED);
            if (history_idx_ > 0) history_idx_--;
        }
    });

    connect(new QShortcut(Qt::Key_PageDown, this), &QShortcut::activated, [=, this]() {
        if (history_idx_ < history_.size()) {
            selector_->select(history_[history_idx_]);
            selector_->status(SelectorStatus::CAPTURED);
            if (history_idx_ < history_.size() - 1) history_idx_++;
        }
    });

    connect(new QShortcut(Qt::CTRL | Qt::Key_C, this), &QShortcut::activated, [=, this]() {
        if (selector_->status() < SelectorStatus::CAPTURED) {
            QApplication::clipboard()->setText(magnifier_->getColorStringValue());
        }
        else {
            // canvas_->copy();
        }
    });

    // connect(new QShortcut(Qt::CTRL | Qt::Key_V, this), &QShortcut::activated, canvas_, &Canvas::paste);

    // connect(new QShortcut(Qt::Key_Delete, this), &QShortcut::activated, canvas_, &Canvas::remove);
}