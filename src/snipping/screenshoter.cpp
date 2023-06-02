#include "screenshoter.h"

#include "logging.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFileDialog>
#include <QGraphicsScene>
#include <QMimeData>
#include <QMouseEvent>
#include <QPalette>
#include <QScreen>
#include <QShortcut>
#include <QWheelEvent>

ScreenShoter::ScreenShoter(QWidget *parent)
    : QGraphicsView(parent)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::BypassWindowManagerHint);
#ifdef NDEBUG
    setWindowFlag(Qt::WindowStaysOnTopHint, true);
#endif

    setFrameStyle(QGraphicsView::NoFrame);
    setContentsMargins({ 0, 0, 0, 0 });
    setViewportMargins({ 0, 0, 0, 0 });

    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    selector_  = new Selector(this);
    menu_      = new EditingMenu(this);
    magnifier_ = new Magnifier(this);
    setScene(new QGraphicsScene(this));

    menu_->installEventFilter(this);

    connect(menu_, &EditingMenu::save, this, &ScreenShoter::save);
    connect(menu_, &EditingMenu::copy, this, &ScreenShoter::copy);
    connect(menu_, &EditingMenu::pin, this, &ScreenShoter::pin);
    connect(menu_, &EditingMenu::exit, this, &ScreenShoter::exit);

    connect(menu_, &EditingMenu::undo, &commands_, &CommandStack::undo);
    connect(menu_, &EditingMenu::redo, &commands_, &CommandStack::redo);

    connect(menu_, &EditingMenu::graphChanged, [this](auto graph) {
        if (graph != canvas::none) {
            selector_->status(SelectorStatus::LOCKED);
        }

        updateCursor(ResizerLocation::DEFAULT);
    });

    connect(menu_, &EditingMenu::penChanged, [this](auto graph, auto pen) {
        if (scene()->focusItem()) {
            auto item = dynamic_cast<GraphicsItemInterface *>(scene()->focusItem());
            if (graph == item->graph()) item->setPen(pen);
        }
    });

    connect(menu_, &EditingMenu::brushChanged, [this](auto graph, auto brush) {
        if (scene()->focusItem()) {
            auto item = dynamic_cast<GraphicsItemInterface *>(scene()->focusItem());
            if (graph == item->graph()) item->setBrush(brush);
        }
    });

    connect(menu_, &EditingMenu::fontChanged, [this](auto graph, auto font) {
        if (scene()->focusItem()) {
            auto item = dynamic_cast<GraphicsItemInterface *>(scene()->focusItem());
            if (graph == item->graph()) item->setFont(font);
        }
    });

    connect(menu_, &EditingMenu::fillChanged, [this](auto graph, auto filled) {
        if (scene()->focusItem()) {
            auto item = dynamic_cast<GraphicsItemInterface *>(scene()->focusItem());
            if (graph == item->graph()) item->fill(filled);
        }
    });

    connect(menu_, &EditingMenu::imageArrived, [this](auto pixmap) {
        QGraphicsItem *item = new GraphicsPixmapItem(pixmap, scene()->sceneRect().center());

        scene()->addItem(item);
        item->setFocus();

        dynamic_cast<GraphicsItemInterface *>(item)->onchanged([this](auto cmd) { commands_.push(cmd); });
        dynamic_cast<GraphicsItemInterface *>(item)->onhovered([this](auto rl) { updateCursor(rl); });
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
    setGeometry(QRect{ virtual_geometry });  // window geometry == virtual geometry, absolute coordinate
    scene()->setSceneRect(rect());           // relative coordinate, start at (0, 0)
    selector_->setGeometry(rect());          // relative coordinate, start at (0, 0)
    selector_->coordinate(virtual_geometry); // painter coordinate, absolute coordinate

    // background
    setBackgroundBrush(captured_pixmap);
    setCacheMode(QGraphicsView::CacheBackground);

    selector_->start(probe::graphics::window_filter_t::visible |
                     probe::graphics::window_filter_t::children);

    selector_->show();

    show();

    // Qt::BypassWindowManagerHint: no keyboard input unless call QWidget::activateWindow()
    activateWindow();
}

void ScreenShoter::exit()
{
    magnifier_->close();

    menu_->close();

    selector_->close();

    copied_ = {};

    scene()->clear();
    setBackgroundBrush(Qt::NoBrush);

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

void ScreenShoter::updateCursor(ResizerLocation location)
{
    if (menu_->graph() & (canvas::eraser | canvas::mosaic)) {
        circle_cursor_.setWidth(menu_->pen().width());
        setCursor(QCursor(circle_cursor_.cursor()));
    }
    else {
        setCursor(getCursorByLocation(location, Qt::CrossCursor));
    }
}

void ScreenShoter::mousePressEvent(QMouseEvent *event)
{
    creating_item_ = {};

    QGraphicsView::mousePressEvent(event);

    if (!event->isAccepted() && event->button() == Qt::LeftButton && menu_->graph() != canvas::none) {
        auto pos = event->pos();

        editstatus_ = EditStatus::GRAPH_CREATING;

        QGraphicsItem *item = nullptr;
        QPen pen            = menu_->pen();

        switch (menu_->graph()) {
        case canvas::text: {
            item = new GraphicsTextItem(pos);

            auto iif = dynamic_cast<GraphicsItemInterface *>(item);
            iif->onresized([iif, this](auto) { menu_->setFont(iif->font()); });
            break;
        }
        case canvas::rectangle: item = new GraphicsRectItem(pos, pos); break;
        case canvas::ellipse: item = new GraphicsEllipseleItem(pos, pos); break;
        case canvas::arrow: item = new GraphicsArrowItem(pos, pos); break;
        case canvas::line: item = new GraphicsLineItem(pos, pos); break;
        case canvas::counter: item = new GraphicsCounterleItem(pos, ++counter_); break;

        case canvas::eraser:
            item = new GraphicsEraserItem(pos);
            pen.setBrush(QBrush(backgroundBrush().textureImage()));

            dynamic_cast<GraphicsEraserItem *>(item)->setPen(pen);
            break;

        case canvas::mosaic:
            pen.setBrush(mosaicBrush());
            item = new GraphicsMosaicItem(pos);
            dynamic_cast<GraphicsMosaicItem *>(item)->setPen(pen);
            break;
        case canvas::curve: item = new GraphicsCurveItem(pos); break;

        default: return;
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
        creating_item_.first->onhovered([this](ResizerLocation location) { updateCursor(location); });

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
        case canvas::text: break;
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
        if (creating_item_.first->invalid() && creating_item_.second != canvas::text) {
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
    if (menu_->graph() & (canvas::eraser | canvas::mosaic)) {
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
    auto right = area.right() - menu_->width() - 2;
    if (right < geometry().left()) right = geometry().left();

    if (area.bottom() + (menu_->height() * 2 + 6 /*margin*/) < geometry().bottom()) {
        menu_->move(right, area.bottom() + 6);
        menu_->setSubMenuShowAbove(false);
    }
    else if (area.top() - (menu_->height() * 2 + 6) > 0) {
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

    auto rect = selector_->selected(); // absolute coordinate
    selector_->close();
    scene()->clearFocus();
    scene()->clearSelection();

    return { QGraphicsView::grab(rect.translated(-geometry().topLeft())), rect.topLeft() };
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
        save_path_ = QFileInfo(filename).absolutePath();

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
            if (auto focused = scene()->focusItem(); focused) {
                copied_ = { focused, 1 };
            }
        }
    });

    connect(new QShortcut(Qt::CTRL | Qt::Key_V, this), &QShortcut::activated, [this]() {
        if (copied_.first) {
            if (auto copied = dynamic_cast<GraphicsItemInterface *>(copied_.first)->copy(); copied) {
                copied->moveBy(copied_.second * 10, copied_.second * (-10));
                copied_.second++;
                scene()->addItem(copied);
                commands_.push(std::make_shared<CreatedCommand>(copied));
                copied->setFocus();
            }
        }
    });
}