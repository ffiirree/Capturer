#include "screenshoter.h"

#include "canvas/graphicsitems.h"
#include "logging.h"
#include "magnifier.h"
#include "menu/editing-menu.h"
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
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint /*| Qt::WindowStaysOnTopHint*/ |
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

        updateCursor();
    });

    connect(menu_, &ImageEditMenu::penChanged, [this](auto graph, auto pen) {
        if (scene()->focusItem()) {
            auto item = dynamic_cast<GraphicsItemInterface *>(scene()->focusItem());
            if (graph != item->graph() || item->pen() == pen) return;

            commands_.push(std::make_shared<PenChangedCommand>(item, item->pen(), pen));
            item->setPen(pen);
        }
    });

    connect(menu_, &ImageEditMenu::brushChanged, [this](auto graph, auto brush) {
        if (scene()->focusItem()) {
            auto item = dynamic_cast<GraphicsItemInterface *>(scene()->focusItem());
            if (graph != item->graph() || item->brush() == brush) return;

            commands_.push(std::make_shared<BrushChangedCommand>(item, item->brush(), brush));
            item->setBrush(brush);
        }
    });

    connect(menu_, &ImageEditMenu::fontChanged, [this](auto graph, auto font) {
        if (scene()->focusItem()) {
            auto item = dynamic_cast<GraphicsItemInterface *>(scene()->focusItem());
            if (graph != item->graph() || font == item->font()) return;

            commands_.push(std::make_shared<FontChangedCommand>(item, item->font(), font));
            item->setFont(menu_->font());
        }
    });

    connect(menu_, &ImageEditMenu::fillChanged, [this](auto graph, auto filled) {
        if (scene()->focusItem()) {
            auto item = dynamic_cast<GraphicsItemInterface *>(scene()->focusItem());
            if (graph != item->graph() || filled == item->filled()) return;

            commands_.push(std::make_shared<FillChangedCommand>(item, filled));
            item->fill(menu_->filled());
        }
    });

    connect(menu_, &ImageEditMenu::imageArrived, [this](auto pixmap) {
        QGraphicsItem *item = new GraphicsPixmapItem(pixmap, scene()->sceneRect().center());
        dynamic_cast<GraphicsItemInterface *>(item)->onchanged([this](auto cmd) { commands_.push(cmd); });

        scene()->addItem(item);
        item->setFocus();
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

    creating_item_ = {};

    counter_ = 0;

    commands_.clear();

    unsetCursor();

    QWidget::close();
}

QBrush ScreenShoter::mosaicBrush()
{
    auto pixmap = QPixmap::fromImage(backgroundBrush().textureImage());
    QPainter painter(&pixmap);

    scene()->render(&painter, pixmap.rect());

    return pixmap.scaled(pixmap.size() / 9, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
        .scaled(pixmap.size());
}

void ScreenShoter::updateCursor()
{
    if (!!(menu_->graph() & (graph_t::eraser | graph_t::mosaic))) {
        circle_cursor_.setWidth(menu_->pen().width());
        setCursor(QCursor(circle_cursor_.cursor()));
    }
    else {
        setCursor(Qt::CrossCursor);
    }
}

void ScreenShoter::mousePressEvent(QMouseEvent *event)
{
    creating_item_ = {};

    QGraphicsView::mousePressEvent(event);

    if (!event->isAccepted() && event->button() == Qt::LeftButton && menu_->graph() != graph_t::none) {
        auto pos = event->pos();

        editstatus_ = EditStatus::GRAPH_CREATING;

        QGraphicsItem *item = nullptr;
        QPen pen            = menu_->pen();

        switch (menu_->graph()) {
        case graph_t::text: item = new GraphicsTextItem(pos); break;
        case graph_t::rectangle: item = new GraphicsRectItem(pos, pos); break;
        case graph_t::ellipse: item = new GraphicsEllipseleItem(pos, pos); break;
        case graph_t::arrow: item = new GraphicsArrowItem(pos, pos); break;
        case graph_t::line: item = new GraphicsLineItem(pos, pos); break;
        case graph_t::counter: item = new GraphicsCounterleItem(pos, ++counter_); break;

        case graph_t::eraser:
            item = new GraphicsEraserItem(pos);
            pen.setBrush(QBrush(backgroundBrush().textureImage()));

            dynamic_cast<GraphicsEraserItem *>(item)->setPen(pen);
            break;

        case graph_t::mosaic:
            pen.setBrush(mosaicBrush());
            item = new GraphicsMosaicItem(pos);
            dynamic_cast<GraphicsMosaicItem *>(item)->setPen(pen);
            break;
        case graph_t::curve: item = new GraphicsCurveItem(pos); break;

        default: break;
        }

        creating_item_ = { dynamic_cast<GraphicsItemInterface *>(item), menu_->graph() };

        creating_item_.first->setPen(pen);
        creating_item_.first->setBrush(menu_->brush());
        creating_item_.first->setFont(menu_->font());
        creating_item_.first->fill(menu_->filled());

        creating_item_.first->onfocused([citem = creating_item_.first, this]() {
            scene()->clearSelection(); // clear selection of text items
            menu_->paintGraph(citem->graph());
            menu_->fill(citem->filled());
            menu_->setPen(citem->pen());
            menu_->setBrush(citem->brush());
            menu_->setFont(citem->font());
        });
        creating_item_.first->onchanged([this](auto cmd) { commands_.push(cmd); });

        scene()->addItem(item);
        item->setFocus();
    }
}

void ScreenShoter::mouseMoveEvent(QMouseEvent *event)
{
    auto pos = event->pos();

    if ((event->buttons() & Qt::LeftButton) && creating_item_.first &&
        editstatus_ == EditStatus::GRAPH_CREATING) {
        switch (creating_item_.second) {
        case graph_t::text: break;
        default: creating_item_.first->push(pos); break;
        }
    }

    // updateCursor();
    QGraphicsView::mouseMoveEvent(event);
}

void ScreenShoter::mouseReleaseEvent(QMouseEvent *event)
{
    if ((event->button() == Qt::LeftButton) && creating_item_.first &&
        editstatus_ == EditStatus::GRAPH_CREATING) {
        if (creating_item_.first->invalid() && creating_item_.second != graph_t::text) {
            scene()->removeItem(dynamic_cast<QGraphicsItem *>(creating_item_.first));
        }
        else {
            commands_.push(
                std::make_shared<CreatedCommand>(dynamic_cast<QGraphicsItem *>(creating_item_.first)));
        }
    }

    editstatus_    = EditStatus::READY;
    creating_item_ = {};

    QGraphicsView::mouseReleaseEvent(event);
}

void ScreenShoter::wheelEvent(QWheelEvent *event)
{
    if (!!(menu_->graph() & (graph_t::eraser | graph_t::mosaic))) {
        auto delta = event->angleDelta().y() / 120;
        auto width = std::clamp(menu_->pen().width() + delta, 5, 49);
        menu_->setPen(QPen(QBrush{}, width));
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
    QGraphicsView::mouseDoubleClickEvent(event);

    if (!event->isAccepted() && event->button() == Qt::LeftButton &&
        selector_->status() >= SelectorStatus::CAPTURED) {
        save2clipboard(snip().first, false);
        exit();
    }
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
    scene()->clearFocus();
    scene()->clearSelection();
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