#include "screenshoter.h"

#include "graphicsitem.h"
#include "graphicstextitem.h"
#include "imageeditmenu.h"
#include "logging.h"
#include "magnifier.h"
#include "selector.h"

#include <QApplication>
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
#include <QWheelEvent>

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

    menu_->installEventFilter(this);

    connect(menu_, &ImageEditMenu::save, this, &ScreenShoter::save);
    connect(menu_, &ImageEditMenu::ok, this, &ScreenShoter::copy);
    connect(menu_, &ImageEditMenu::pin, this, &ScreenShoter::pin);
    connect(menu_, &ImageEditMenu::exit, this, &ScreenShoter::exit);

    connect(menu_, &ImageEditMenu::undo, &commands_, &CommandStack::undo);
    connect(menu_, &ImageEditMenu::redo, &commands_, &CommandStack::redo);

    connect(menu_, &ImageEditMenu::graphChanged, [this](graph_t graph) {
        if (graph != graph_t::none) {
            selector_->status(SelectorStatus::LOCKED);
        }
    });

    //connect(menu_, &ImageEditMenu::changed, [this]() {
    //    if (scene()->focusItem()) {}
    //});

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

    connect(&commands_, &CommandStack::emptied,
            [this](auto redo, auto v) { redo ? menu_->disableRedo(v) : menu_->disableUndo(v); });

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

    // background
    setBackgroundBrush(captured_pixmap);
    setCacheMode(QGraphicsView::CacheBackground);

    selector_->start(probe::graphics::window_filter_t::visible |
                     probe::graphics::window_filter_t::children);

    selector_->show();

    show();
    activateWindow(); //  Qt::BypassWindowManagerHint: no keyboard input unless call
                      //  QWidget::activateWindow()
}

void ScreenShoter::exit()
{
    magnifier_->hide();

    menu_->reset();
    menu_->hide();

    scene()->clear();

    hover_postion_ = Resizer::DEFAULT;
    created_item_  = {};

    commands_.clear();

    setCursor(Qt::CrossCursor);

    QWidget::close();
}

// bool ScreenShoter::eventFilter(QObject *obj, QEvent *event)
//{
//     if (obj == menu_) {
//         switch (event->type()) {
//         case QEvent::KeyPress:
//         case QEvent::KeyRelease: return eventFilter(this, event);
//         }
//     }
//
//     // return QGraphicsView::eventFilter(this, event);
//     return QGraphicsView::eventFilter(obj, event);
// }

QBrush ScreenShoter::mosaicBrush()
{
    auto pixmap = QPixmap::fromImage(backgroundBrush().textureImage());
    QPainter painter(&pixmap);

    scene()->render(&painter, pixmap.rect());

    return pixmap.scaled(pixmap.size() / 9, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
        .scaled(pixmap.size());
}

void ScreenShoter::mousePressEvent(QMouseEvent *event)
{
    created_item_ = {};

    QGraphicsView::mousePressEvent(event);

    if (event->button() == Qt::LeftButton && !(hover_postion_ & Resizer::ADJUST_AREA) &&
        !(hover_postion_ & Resizer::EDITAREA)) {
        auto pos = event->pos();

        if (menu_->graph() == graph_t::text) {
            auto item = new GraphicsTextItem("", pos);
            item->setFont(menu_->font());
            item->setDefaultTextColor(menu_->pen().color());
            scene()->addItem(item);
            item->setFocus();
            created_item_   = { item, graph_t::text };
            item->onhovered = [this](auto p) {
                hover_postion_ = p;
                updateCursor();
            };
            commands_.push(std::make_shared<CreatedCommand>(item));
        }
        else {
            editstatus_ = EditStatus::GRAPH_CREATING;

            QPen pen = menu_->pen();

            if (menu_->graph() == graph_t::mosaic) {
                auto background = backgroundBrush().textureImage();
                pen.setBrush(mosaicBrush());
            }
            else if (menu_->graph() == graph_t::eraser) {
                pen.setBrush(QBrush(backgroundBrush().textureImage()));
            }

            auto item = new GraphicsItem(menu_->graph(), { pos, pos });
            item->setPen(pen);
            if (menu_->fill()) item->setBrush(menu_->brush());
            scene()->addItem(item);
            item->setFocus();
            item->onhovered = [this](auto p) {
                hover_postion_ = p;
                updateCursor();
            };
            item->ondeleted = [item, this]() { commands_.push(std::make_shared<DeleteCommand>(item)); };
            created_item_   = { item, menu_->graph() };
        }
    }
}

void ScreenShoter::updateCursor()
{
    if (menu_->graph() == graph_t::eraser || menu_->graph() == graph_t::mosaic) {
        circle_cursor_.setWidth(menu_->pen().width());
        setCursor(QCursor(circle_cursor_.cursor()));
    }
    else {
        setCursor(getCursorByLocation(hover_postion_, Qt::CrossCursor));
    }
}

void ScreenShoter::mouseMoveEvent(QMouseEvent *event)
{
    auto pos = event->pos();

    if ((event->buttons() & Qt::LeftButton) && editstatus_ == EditStatus::GRAPH_CREATING) {
        if (scene()->focusItem() == created_item_.first) {
            reinterpret_cast<GraphicsItem *>(scene()->focusItem())->pushVertex(pos);
        }
    }

    QGraphicsView::mouseMoveEvent(event);
}

void ScreenShoter::mouseReleaseEvent(QMouseEvent *event)
{
    if ((event->button() == Qt::LeftButton) && editstatus_ == EditStatus::GRAPH_CREATING) {
        if (created_item_.first && scene()->focusItem() == created_item_.first &&
            created_item_.second != graph_t::none) {
            if (created_item_.second != graph_t::text) {
                auto item = reinterpret_cast<GraphicsItem *>(scene()->focusItem());
                if (!item->invalid()) {
                    commands_.push(std::make_shared<CreatedCommand>(scene()->focusItem()));
                }
                else {
                    scene()->removeItem(item);
                }
            }
        }
        editstatus_ = EditStatus::READY;
    }
    QGraphicsView::mouseReleaseEvent(event);
    created_item_ = {};
}

void ScreenShoter::wheelEvent(QWheelEvent *event)
{
    if (menu_->graph() == graph_t::eraser || menu_->graph() == graph_t::mosaic) {
        auto delta = event->angleDelta().y() / 120;
        auto width = std::min(menu_->pen().width() + delta, 49);
        menu_->pen(QPen(QBrush{}, width));
        circle_cursor_.setWidth(width);
        setCursor(QCursor(circle_cursor_.cursor()));
    }
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
    if (event->button() == Qt::LeftButton && selector_->status() >= SelectorStatus::CAPTURED &&
        hover_postion_ != Resizer::EDITAREA) {
        save2clipboard(snip().first, false);
        exit();
    }

    QGraphicsView::mouseDoubleClickEvent(event);
}

void ScreenShoter::moveMenu()
{
    auto area  = selector_->selected();
    auto right = area.right() - menu_->width() + 1;
    if (right < geometry().left()) right = geometry().left();

    if (area.bottom() + (menu_->height() * 2 + 8 /*margin*/) < geometry().bottom()) {
        menu_->move(right, area.bottom() + 6);
        menu_->setSubMenuShowAbove(false);
    }
    else if (area.top() - (menu_->height() * 2 + 8) > 0) {
        menu_->move(right, area.top() - menu_->height() - 6);
        menu_->setSubMenuShowAbove(true);
    }
    else {
        menu_->move(right, area.bottom() - menu_->height() - 6);
        menu_->setSubMenuShowAbove(true);
    }
}

std::pair<QPixmap, QPoint> ScreenShoter::snip()
{
    history_.push_back(selector_->prey());
    history_idx_ = history_.size() - 1;

    auto rect = selector_->selected();
    selector_->close();
    return { QGraphicsView::grab(rect), rect.topLeft() };
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

        auto [pixmap, _] = snip();
        pixmap.save(filename);

        emit SHOW_MESSAGE("Capturer<PICTURE>", "Path: " + filename);
        exit();
    }
}

void ScreenShoter::copy()
{
    save2clipboard(snip().first, false);

    exit();
}

void ScreenShoter::pin()
{
    auto [pixmap, pos] = snip();

    emit pinSnipped(pixmap, pos);

    save2clipboard(pixmap, true);

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

    connect(new QShortcut(Qt::Key_Tab, this), &QShortcut::activated, [this]() {
        if (magnifier_->isVisible()) {
            magnifier_->toggleColorFormat();
        }
    });

    // clang-format off
    connect(new QShortcut(Qt::Key_Return, this), &QShortcut::activated, [this]() { copy(); exit(); });
    connect(new QShortcut(Qt::Key_Enter,  this), &QShortcut::activated, [this]() { copy(); exit(); });
    connect(new QShortcut(Qt::Key_Escape, this), &QShortcut::activated, [this]() { exit(); });

    connect(new QShortcut(Qt::CTRL | Qt::Key_Z, this),              &QShortcut::activated, &commands_, &CommandStack::undo);
    connect(new QShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_Z, this),  &QShortcut::activated, &commands_, &CommandStack::redo);
    // clang-format on

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
            // TODO: copy
        }
    });
}