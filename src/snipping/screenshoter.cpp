#include "screenshoter.h"

#include "clipboard.h"
#include "logging.h"

#include <QApplication>
#include <QDateTime>
#include <QFileDialog>
#include <QMouseEvent>
#include <QScreen>
#include <QShortcut>

ScreenShoter::ScreenShoter(QWidget *parent)
    : QGraphicsView(parent)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::BypassWindowManagerHint);
#ifdef NDEBUG
    setWindowFlag(Qt::WindowStaysOnTopHint, true);
#endif

    setFrameStyle(QGraphicsView::NoFrame);
    setContentsMargins({});
    setViewportMargins({});

    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    selector_   = new Selector(this);
    menu_       = new EditingMenu(this);
    magnifier_  = new Magnifier(this);
    undo_stack_ = new QUndoStack(this);

    scene_ = new canvas::Canvas(this);
    setScene(scene_);

    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);

    connect(scene_, &canvas::Canvas::focusItemChanged, this, [this](auto item, auto, auto reason) {
        if (item != nullptr && reason == Qt::MouseFocusReason) {
            scene()->clearSelection(); // clear selection of text items

            const auto wrapper = dynamic_cast<GraphicsItemWrapper *>(item);
            menu_->paintGraph((wrapper->graph() == canvas::pixmap) ? canvas::none : wrapper->graph());

            menu_->fill(wrapper->filled());
            menu_->setPen(wrapper->pen());
            menu_->setFont(wrapper->font());
        }
    });

    // TODO:
    connect(menu_, &EditingMenu::scroll, [this]() {});

    connect(menu_, &EditingMenu::save, this, &ScreenShoter::save);
    connect(menu_, &EditingMenu::copy, this, &ScreenShoter::copy);
    connect(menu_, &EditingMenu::pin, this, &ScreenShoter::pin);
    connect(menu_, &EditingMenu::exit, this, &ScreenShoter::exit);

    connect(menu_, &EditingMenu::undo, undo_stack_, &QUndoStack::undo);
    connect(menu_, &EditingMenu::redo, undo_stack_, &QUndoStack::redo);

    connect(menu_, &EditingMenu::graphChanged, [this](auto graph) {
        if (graph != canvas::none) {
            selector_->status(SelectorStatus::LOCKED);
        }

        updateCursor(ResizerLocation::DEFAULT);
    });

    connect(menu_, &EditingMenu::penChanged, [this](auto graph, auto pen) {
        if (auto item = scene_->focusItem(); item && item->graph() == graph && item->pen() != pen) {
            undo_stack_->push(new PenChangedCommand(item, item->pen(), pen));
        }
    });

    connect(menu_, &EditingMenu::fontChanged, [this](auto graph, auto font) {
        if (auto item = scene_->focusOrFirstSelectedItem();
            item && item->graph() == graph && item->font() != font) {
            undo_stack_->push(new FontChangedCommand(item, item->font(), font));
        }
    });

    connect(menu_, &EditingMenu::fillChanged, [this](auto graph, auto filled) {
        if (auto item = scene_->focusItem(); item && item->graph() == graph && item->filled() != filled) {
            undo_stack_->push(new FillChangedCommand(item, filled));
        }
    });

    connect(menu_, &EditingMenu::imageArrived, [this](auto pixmap) {
        GraphicsItemWrapper *item = new GraphicsPixmapItem(pixmap, scene()->sceneRect().center());

        undo_stack_->push(new CreatedCommand(scene(), dynamic_cast<QGraphicsItem *>(item)));

        item->onhovered([this](auto rl) { updateCursor(rl); });
        item->onmoved([=, this](auto opos) {
            undo_stack_->push(new MoveCommand(dynamic_cast<QGraphicsItem *>(item), opos));
        });
        item->onrotated([=, this](auto angle) { undo_stack_->push(new RotateCommand(item, angle)); });
        item->onresized([=, this](auto g, auto l) { undo_stack_->push(new ResizeCommand(item, g, l)); });
    });

    // show menu
    connect(selector_, &Selector::captured, [this]() {
        menu_->show();
        moveMenu();
    });

    // move menu
    connect(selector_, &Selector::moved, this, &ScreenShoter::moveMenu);
    connect(selector_, &Selector::resized, this, &ScreenShoter::moveMenu);
    connect(selector_, &Selector::statusChanged, [this](auto status) {
        if ((status > SelectorStatus::READY && status < SelectorStatus::CAPTURED) ||
            status == SelectorStatus::RESIZING)
            magnifier_->show();
        else
            magnifier_->close();
    });

    selector_->installEventFilter(this);

    //
    connect(undo_stack_, &QUndoStack::canRedoChanged, menu_, &EditingMenu::canRedo);
    connect(undo_stack_, &QUndoStack::canUndoChanged, menu_, &EditingMenu::canUndo);

    // shortcuts
    registerShortcuts();
}

void ScreenShoter::start()
{
    if (isVisible()) return;

    const auto virtual_geometry = probe::graphics::virtual_screen_geometry();

    // geometry
    setGeometry(QRect{ virtual_geometry });  // window geometry == virtual geometry, absolute coordinate
    scene()->setSceneRect(rect());           // relative coordinate, start at (0, 0)
    selector_->setGeometry(rect());          // relative coordinate, start at (0, 0)
    selector_->coordinate(virtual_geometry); // painter coordinate, absolute coordinate

    refresh(virtual_geometry);

    //
    selector_->start(probe::graphics::window_filter_t::visible |
                     probe::graphics::window_filter_t::children);

    selector_->show();

    show();

    // Qt::BypassWindowManagerHint: no keyboard input unless call QWidget::activateWindow()
    activateWindow();
}

void ScreenShoter::refresh(const probe::geometry_t& geometry)
{
    // background
    background_ = QGuiApplication::primaryScreen()->grabWindow(
        probe::graphics::virtual_screen().handle, geometry.x, geometry.y, static_cast<int>(geometry.width),
        static_cast<int>(geometry.height));

    setBackgroundBrush(background_);
    setCacheMode(QGraphicsView::CacheBackground);

    //
    magnifier_->setGrabPixmap(background_);
}

QBrush ScreenShoter::mosaicBrush()
{
    auto     pixmap = QPixmap::fromImage(backgroundBrush().textureImage());
    QPainter painter(&pixmap);

    scene()->render(&painter, pixmap.rect());

    return pixmap.scaled(pixmap.size() / 9, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
        .scaled(pixmap.size());
}

void ScreenShoter::updateCursor(ResizerLocation location)
{
    if (menu_->graph() & canvas::eraser) {
        setCursor(
            QCursor{ cursor::circle(menu_->pen().width(), { QColor{ 136, 136, 136 }, 3 }, Qt::NoBrush) });
    }
    else if (menu_->graph() & canvas::mosaic) {
        setCursor(QCursor{ cursor::circle(menu_->pen().width(), { QColor{ 136, 136, 136 }, 3 }) });
    }
    else {
        setCursor(getCursorByLocation(location, Qt::CrossCursor));
    }
}

bool ScreenShoter::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == selector_ && event->type() == QEvent::MouseButtonDblClick) {
        mouseDoubleClickEvent(dynamic_cast<QMouseEvent *>(event));
        return true;
    }
    return QGraphicsView::eventFilter(obj, event);
}

void ScreenShoter::mousePressEvent(QMouseEvent *event)
{
    const auto ffs_item = scene_->focusOrFirstSelectedItem();

    // focus or blur
    QGraphicsView::mousePressEvent(event);

    //
    if (event->isAccepted() || event->button() != Qt::LeftButton) return;

    // do not create text items if a text item has focus
    if ((creating_item_ && (creating_item_->graph() & canvas::text)) ||
        (ffs_item && (ffs_item->graph() & canvas::text))) {
        if (menu_->graph() & canvas::text) return;
    }

    createItem(event->pos());
}

void ScreenShoter::mouseMoveEvent(QMouseEvent *event)
{
    if (creating_item_) {
        creating_item_->push(event->pos());
    }

    QGraphicsView::mouseMoveEvent(event);
}

void ScreenShoter::mouseReleaseEvent(QMouseEvent *event)
{
    if (creating_item_) {
        // handle the creating text item on the second click
        if (!((creating_item_->graph() & canvas::text) && creating_item_ == scene_->focusItem())) {
            finishItem();
        }
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void ScreenShoter::createItem(const QPointF& pos)
{
    if (menu_->graph() == canvas::none) return;
    if (creating_item_) finishItem();

    switch (menu_->graph()) {
    case canvas::rectangle: creating_item_ = new GraphicsRectItem(pos, pos); break;
    case canvas::ellipse:   creating_item_ = new GraphicsEllipseleItem(pos, pos); break;
    case canvas::arrow:     creating_item_ = new GraphicsArrowItem(pos, pos); break;
    case canvas::line:      creating_item_ = new GraphicsLineItem(pos, pos); break;
    case canvas::curve:     creating_item_ = new GraphicsCurveItem(pos, sceneRect().size()); break;
    case canvas::counter:   creating_item_ = new GraphicsCounterleItem(pos, ++counter_); break;
    case canvas::text:      creating_item_ = new GraphicsTextItem(pos); break;
    case canvas::eraser:
        creating_item_ = new GraphicsEraserItem(pos, sceneRect().size(), backgroundBrush());
        break;
    case canvas::mosaic:
        creating_item_ = new GraphicsMosaicItem(pos, sceneRect().size(), mosaicBrush());
        break;
    default: return;
    }

    creating_item_->setPen(menu_->pen());
    creating_item_->setFont(menu_->font());
    creating_item_->fill(menu_->filled());

    creating_item_->onhovered([this](auto location) { updateCursor(location); });
    creating_item_->onmoved([item = creating_item_, this](auto opos) {
        undo_stack_->push(new MoveCommand(dynamic_cast<QGraphicsItem *>(item), opos));
    });
    creating_item_->onresized([item = creating_item_, this](auto g, auto l) {
        undo_stack_->push(new ResizeCommand(item, g, l));
        if (item->graph() & canvas::text) menu_->setFont(item->font());
    });

    creating_item_->onrotated(
        [item = creating_item_, this](auto angle) { undo_stack_->push(new RotateCommand(item, angle)); });

    scene_->add(creating_item_);
    dynamic_cast<QGraphicsItem *>(creating_item_)->setFocus();
}

void ScreenShoter::finishItem()
{
    if (!creating_item_) return;

    if (creating_item_->invalid()) {
        scene_->remove(creating_item_);
        delete creating_item_;
        creating_item_ = nullptr;
    }
    else {
        creating_item_->end();
        undo_stack_->push(new CreatedCommand(scene(), dynamic_cast<QGraphicsItem *>(creating_item_)));
        creating_item_ = nullptr;
    }
}

void ScreenShoter::wheelEvent(QWheelEvent *event)
{
    if (menu_->graph() != canvas::none) {
        const auto delta = event->angleDelta().y() / 120;
        auto       pen   = menu_->pen();

        if ((event->modifiers() & Qt::CTRL) && canvas::has_color(menu_->graph())) {
            auto color = pen.color();
            color.setAlpha(std::clamp<int>(color.alpha() + delta * 5, 0, 255));
            pen.setColor(color);

            menu_->setPen(pen, false);
            updateCursor(ResizerLocation::DEFAULT);
        }
        else if (canvas::has_width(menu_->graph())) {
            pen.setWidth(std::clamp(menu_->pen().width() + delta, 1, 71));

            menu_->setPen(pen, false);
            updateCursor(ResizerLocation::DEFAULT);
        }
    }
}

void ScreenShoter::keyPressEvent(QKeyEvent *event)
{
    // hotkey 'Space': move the selector while editing.
    if (selector_->status() == SelectorStatus::LOCKED && event->key() == Qt::Key_Space &&
        !event->isAutoRepeat()) {
        selector_->status(SelectorStatus::CAPTURED);
    }
    else if (event->key() == Qt::Key_Shift && !event->isAutoRepeat()) {
        selector_->showCrossHair(true);
    }

    QGraphicsView::keyPressEvent(event);
}

void ScreenShoter::keyReleaseEvent(QKeyEvent *event)
{
    // stop moving the selector while editing
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        selector_->status(SelectorStatus::LOCKED);
    }
    else if (event->key() == Qt::Key_Shift && !event->isAutoRepeat()) {
        selector_->showCrossHair(false);
    }

    QGraphicsView::keyReleaseEvent(event);
}

void ScreenShoter::mouseDoubleClickEvent(QMouseEvent *event)
{
    QGraphicsView::mouseDoubleClickEvent(event);

    if (!event->isAccepted() && event->button() == Qt::LeftButton &&
        selector_->status() >= SelectorStatus::CAPTURED) {
        copy();
    }
}

void ScreenShoter::moveMenu()
{
    const auto area  = selector_->selected();
    auto       right = area.right() - menu_->width() - 2;
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

    const auto rect = selector_->selected(); // absolute coordinate
    selector_->close();
    scene()->clearFocus();
    scene()->clearSelection();

    return { grab(rect.translated(-geometry().topLeft())), rect.topLeft() };
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
        if (!pixmap.save(filename)) {
            loge("failed to save the captured image.");
        }

        emit saved(filename);
        exit();
    }
}

void ScreenShoter::copy()
{
    clipboard::push(snip().first);

    exit();
}

void ScreenShoter::pin()
{
    auto [pixmap, pos] = snip();

    emit pinData(clipboard::push(pixmap, pos));

    exit();
}

void ScreenShoter::exit()
{
    magnifier_->close();

    menu_->close();

    selector_->close();

    //! 1.
    undo_stack_->clear();

    //! 2.
    if (creating_item_ && !dynamic_cast<QGraphicsItem *>(creating_item_)->scene()) delete creating_item_;
    creating_item_ = {};

    //! 3.
    scene_->clear();

    counter_ = 0;

    QWidget::close();
}

void ScreenShoter::setStyle(const SelectorStyle& style)
{
    selector_->setBorderStyle(QPen{
        style.border_color,
        static_cast<qreal>(style.border_width),
        style.border_style,
    });

    selector_->setMaskStyle(style.mask_color);
}

void ScreenShoter::registerShortcuts()
{
    connect(new QShortcut(Qt::CTRL + Qt::Key_S, this), &QShortcut::activated, [this] {
        if (any(selector_->status() & SelectorStatus::CAPTURED) ||
            any(selector_->status() & SelectorStatus::LOCKED)) {
            save();
        }
    });

    connect(new QShortcut(Qt::Key_P, this), &QShortcut::activated, [this] {
        if (selector_->status() == SelectorStatus::CAPTURED ||
            selector_->status() == SelectorStatus::LOCKED) {
            pin();
        }
    });

    connect(new QShortcut(Qt::Key_Tab, this), &QShortcut::activated, [this] {
        if (magnifier_->isVisible()) {
            magnifier_->toggleFormat();
        }
    });

    connect(new QShortcut(Qt::Key_F5, this), &QShortcut::activated, [this] {
        const auto menuv = menu_->isVisible();
        const auto magnv = magnifier_->isVisible();
        if (menuv) menu_->hide();
        if (magnv) magnifier_->hide();

        hide();

        refresh(probe::graphics::virtual_screen_geometry());

        show();

        if (menuv) menu_->show();
        if (magnv) magnifier_->show();

        activateWindow();
    });

    // clang-format off
    connect(new QShortcut(Qt::Key_Return, this), &QShortcut::activated, [this] { copy(); exit(); });
    connect(new QShortcut(Qt::Key_Enter,  this), &QShortcut::activated, [this] { copy(); exit(); });
    connect(new QShortcut(Qt::Key_Escape, this), &QShortcut::activated, [this] { exit(); });

    connect(new QShortcut(Qt::CTRL + Qt::Key_Z,             this), &QShortcut::activated, undo_stack_, &QUndoStack::undo);
    connect(new QShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_Z, this), &QShortcut::activated, undo_stack_, &QUndoStack::redo);
    // clang-format on

    connect(new QShortcut(QKeySequence::Delete, this), &QShortcut::activated, [this] {
        if (!scene()->selectedItems().isEmpty() || scene()->focusItem()) {
            undo_stack_->push(new DeleteCommand(scene()));
        }
    });

    connect(new QShortcut(Qt::Key_PageUp, this), &QShortcut::activated, [=, this] {
        if (history_idx_ < history_.size()) {
            selector_->select(history_[history_idx_]);
            selector_->status(SelectorStatus::CAPTURED);
            moveMenu();
            if (history_idx_ > 0) history_idx_--;
        }
    });

    connect(new QShortcut(Qt::Key_PageDown, this), &QShortcut::activated, [=, this] {
        if (history_idx_ < history_.size()) {
            selector_->select(history_[history_idx_]);
            selector_->status(SelectorStatus::CAPTURED);
            moveMenu();
            if (history_idx_ < history_.size() - 1) history_idx_++;
        }
    });

    connect(new QShortcut(Qt::CTRL + Qt::Key_C, this), &QShortcut::activated, [=, this] {
        if (selector_->status() < SelectorStatus::CAPTURED && magnifier_->isVisible()) {
            clipboard::push(magnifier_->color(), magnifier_->colorname());
            exit();
        }
    });
}