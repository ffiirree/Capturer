#include "canvas.h"
#include <QMouseEvent>
#include <QPainter>
#include <QGraphicsItem>
#include <QMimeData>
#include <QApplication>
#include <QClipboard>
#include "logging.h"

Canvas::Canvas(ImageEditMenu* menu, QWidget *parent)
    : QObject(parent)
{
    CHECK(menu);

    menu_ = menu;
    connect(menu_, &ImageEditMenu::fix, [this]() { focusOn(nullptr); emit closed(); });
    connect(menu_, &ImageEditMenu::ok, [this]() {
        // copy to clipboard
        QApplication::clipboard()->setPixmap(canvas_);

        focusOn(nullptr);
        emit closed(); 
    });
    connect(menu_, &ImageEditMenu::exit, [this]() { focusOn(nullptr); canvas_ = backup_.copy(); emit closed(); });

    connect(menu_, &ImageEditMenu::undo, this, &Canvas::undo);
    connect(menu_, &ImageEditMenu::redo, this, &Canvas::redo);
    connect(menu_, &ImageEditMenu::graphChanged, this, &Canvas::changeGraph);
    connect(this, &Canvas::focusOnGraph, menu_, &ImageEditMenu::paintGraph);

    // attrs changed
    connect(menu_, &ImageEditMenu::styleChanged, [this](Graph graph) {
        if (focus_cmd_ && focus_cmd_->graph() == graph) {
            focus_cmd_->font(menu_->font(graph));
            focus_cmd_->pen(menu_->pen(graph));
            focus_cmd_->setFill(menu_->fill(graph));
        }
    });

    // repaint when stack changed
    connect(&commands_, &CommandStack::increased, [this]() { modified(PaintType::DRAW_FINISHED); });
    connect(&commands_, &CommandStack::decreased, [this]() { modified(PaintType::REPAINT_ALL); });
    // disable redo/undo
    connect(&commands_, &CommandStack::emptied, menu_, &ImageEditMenu::disableUndo);
    connect(&redo_stack_, &CommandStack::emptied, menu_, &ImageEditMenu::disableRedo);
}

bool Canvas::eventFilter(QObject* object, QEvent* event)
{
    if (!enabled_) return false;

    switch (event->type())
    {
    case QEvent::MouseButtonPress:
        mousePressEvent(static_cast<QMouseEvent*>(event));
        return false;

    case QEvent::MouseButtonRelease:
        mouseReleaseEvent(static_cast<QMouseEvent*>(event));
        return false;

    case QEvent::HoverMove:
    case QEvent::MouseMove:
        mouseMoveEvent(static_cast<QMouseEvent*>(event));
        return false;

    case QEvent::KeyRelease:
        keyReleaseEvent(static_cast<QKeyEvent*>(event));
        return false;

    case QEvent::KeyPress:
        keyPressEvent(static_cast<QKeyEvent*>(event));
        return false;

    case QEvent::Wheel:
        wheelEvent(static_cast<QWheelEvent*>(event));
        return false;

    case QEvent::Paint:
        updateCanvas();
        return false;

    default:
        break;
    }

    return false;
}

void Canvas::changeGraph(Graph graph)
{
    switch (graph) {
    case Graph::NONE:
        edit_status_ = EditStatus::NONE;
        focusOn(nullptr);
        break;

    default:
        edit_status_ = (edit_status_ & (~GRAPH_MASK)) | graph;
        focusOn((focus_cmd_ && focus_cmd_->graph() == graph) ? focus_cmd_ : nullptr);
        break;
    }
}

void Canvas::pixmap(const QPixmap& canvas)
{ 
    canvas_ = canvas.copy(); 
    backup_ = canvas.copy();
    modified(PaintType::REPAINT_ALL); 
}

bool Canvas::editing()
{
    return (edit_status_ & GRAPH_MASK) != 0 || commands_.size() || redo_stack_.size();
} 

void Canvas::focusOn(shared_ptr<PaintCommand> cmd)
{
    if (focus_cmd_ == cmd) return;

    auto previous_focus_cmd = focus_cmd_;
    if (previous_focus_cmd) {
        previous_focus_cmd->setFocus(false);
        if (!previous_focus_cmd->isValid()) {
            commands_.remove(previous_focus_cmd); 
        }
    }

    // switch
    focus_cmd_ = cmd;

    if (focus_cmd_) {
        focus_cmd_->setFocus(true);
        menu_->style(focus_cmd_->graph(), focus_cmd_->pen(), focus_cmd_->isFill());
    }

    // menu
    if (!previous_focus_cmd || (focus_cmd_ && previous_focus_cmd->graph() != focus_cmd_->graph())) {
        emit focusOnGraph(focus_cmd_->graph());  // must after focus_cmd_ = cmd
    }
}

void Canvas::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;

    auto mouse_pos = event->pos();

    // Border => move
    if ((hover_position_ & Resizer::BORDER) && hover_cmd_ && hover_cmd_->visible()) {
        move_begin_ = mouse_pos;

        HIDE_AND_COPY_CMD(hover_cmd_);

        edit_status_ |= EditStatus::GRAPH_MOVING;
    }
    // Anchor => resize
    else if ((hover_position_ & Resizer::ANCHOR) && hover_cmd_ && hover_cmd_->visible()) {
        HIDE_AND_COPY_CMD(hover_cmd_);

        edit_status_ |= GRAPH_RESIZING;
    }
    // rotate
    else if ((hover_position_ == Resizer::ROTATE_ANCHOR) && hover_cmd_ && hover_cmd_->visible()) {
        HIDE_AND_COPY_CMD(hover_cmd_);

        edit_status_ |= GRAPH_ROTATING;
    }
    // text
    else if ((hover_position_ == Resizer::INSIDE) && 
        hover_cmd_  && hover_cmd_->visible() && 
        hover_cmd_->graph() == TEXT) {
        // nothing
    }

    // Not borders and anchors => Create
    else if (edit_status_ & GRAPH_MASK) {
        auto graph = Graph(edit_status_ & GRAPH_MASK);
        auto pen = menu_->pen(graph);

        if (graph == Graph::MOSAIC) {
            pen.setBrush(QBrush(mosaic(canvas_.toImage())));
        }
        else if (graph == Graph::ERASER) {
            pen.setBrush(QBrush(backup_));
        }

        hover_cmd_ = make_shared<PaintCommand>(graph, pen, menu_->font(graph), menu_->fill(graph), event->pos());
        edit_status_ |= GRAPH_CREATING;
    }

    if (hover_cmd_) {
        focusOn(hover_cmd_);
        modified(PaintType::DRAW_MODIFYING);
        connect(hover_cmd_.get(), &PaintCommand::modified, this, &Canvas::modified, Qt::UniqueConnection);
        connect(hover_cmd_.get(), &PaintCommand::styleChanged, this, [this]() {if (hover_cmd_->graph() == TEXT) menu_->font(hover_cmd_->font()); }, Qt::UniqueConnection);
    }
}

void Canvas::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton) {
        auto mouse_pos = event->pos();

        switch (edit_status_ & OPERATION_MASK) {
        case GRAPH_CREATING:
            hover_cmd_->push_point(mouse_pos);
            break;

        case GRAPH_MOVING:
            hover_cmd_->move(mouse_pos - move_begin_);
            move_begin_ = mouse_pos;
            break;

        case GRAPH_RESIZING:
            hover_cmd_->resize(hover_position_, mouse_pos);
            break;

        default:  break;
        }
    }

    if (event->buttons() == Qt::NoButton
        && !(edit_status_ & Graph::ERASER) && !(edit_status_ & Graph::MOSAIC)) {
        updateHoverPos(event->pos());
    }
}

QCursor Canvas::getCursorShape()
{
    if (edit_status_ & Graph::ERASER || edit_status_ & Graph::MOSAIC) {
        circle_cursor_.setWidth(menu_->pen(Graph(edit_status_ & GRAPH_MASK)).width());
        return QCursor(circle_cursor_.cursor());
    }

    // TEXT
    if (hover_cmd_ && hover_cmd_->visible() && 
        hover_cmd_->graph() == TEXT && 
        hover_position_ == Resizer::INSIDE) {
        return Qt::IBeamCursor;
    }

    return PaintCommand::getCursorShapeByHoverPos(hover_position_);
}

void Canvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton
        && edit_status_ != EditStatus::NONE) {

        CHECK(focus_cmd_);

        if ((!focus_cmd_->previous() && focus_cmd_->isValid())      // created
            || (focus_cmd_->previous() && focus_cmd_->adjusted())   // moved/resized..
            || ((edit_status_ & GRAPH_MASK) == Graph::TEXT)) {      // check text is valid when it loses focus
            commands_.push(focus_cmd_);
            redo_stack_.clear();
        }
        else {                                                      // Invalid, no modified. Back to the previous
            if (focus_cmd_->previous()) {
                focus_cmd_->previous()->visible(true);
            }
            // foucs on null or the previous cmd
            focusOn(focus_cmd_->previous());
        }

        edit_status_ &= ~OPERATION_MASK;
    }
}

void Canvas::keyPressEvent(QKeyEvent* event)
{
}

void Canvas::keyReleaseEvent(QKeyEvent* event)
{
    // regularize
    if (event->key() == Qt::Key_Shift 
        && focus_cmd_
        && !focus_cmd_->regularized()
        && !(edit_status_ & OPERATION_MASK)) {

        HIDE_AND_COPY_CMD(focus_cmd_);
        focus_cmd_->regularize();
        focusOn(focus_cmd_);
        connect(focus_cmd_.get(), &PaintCommand::modified, this, &Canvas::modified);

        commands_.push(focus_cmd_);
        redo_stack_.clear();
    }
}
void Canvas::wheelEvent(QWheelEvent* event)
{
    if ((edit_status_ & Graph::ERASER) || (edit_status_ & Graph::MOSAIC)) {
        auto delta = event->delta() / 120;
        auto graph = static_cast<Graph>(edit_status_ & GRAPH_MASK);
        auto pen = menu_->pen(graph);

        pen.setWidth(std::min(pen.width() + delta, 49));
        menu_->pen(graph, pen);
    }
}

void Canvas::updateHoverPos(const QPoint& pos)
{
    hover_position_ = Resizer::DEFAULT;
    hover_cmd_ = nullptr;

    for (auto& command : commands_.commands()) {
        if (!command->visible()) continue;

        hover_position_ = command->hover(pos);
        if (hover_position_ & Resizer::ADJUST_AREA) {
            hover_cmd_ = command;
            break;
        }
        else if (command->graph() == TEXT && hover_position_ == Resizer::INSIDE) {
            hover_cmd_ = command;
            break;
        }
    }
}

void Canvas::updateCanvas()
{
    if (!enabled_ || modified_ == PaintType::UNMODIFIED) return;

    QPainter painter;
    
    painter.begin(&canvas_);

    switch (modified_) {
    case PaintType::DRAW_FINISHED:
        CHECK(focus_cmd_);
        painter.translate(offset_);
        focus_cmd_->repaint(&painter);
        break;

    case PaintType::DRAW_MODIFIED:
        CHECK(focus_cmd_);
        painter.translate(offset_);
        focus_cmd_->draw_modified(&painter);
        break;

    case PaintType::REPAINT_ALL:
        painter.drawPixmap(0, 0, backup_);
        painter.translate(offset_);
        for (auto& command : commands_.commands()) {
            command->repaint(&painter);
        }
        break;

    default: break;
    }
}

void Canvas::drawModifying(QPainter * painter)
{
    if (enabled_ && focus_cmd_ != nullptr && focus_cmd_->visible()) {
        if (modified_ == PaintType::DRAW_MODIFYING || modified_ == PaintType::REPAINT_ALL) {
            focus_cmd_->draw_modified(painter);
        }

        focus_cmd_->drawAnchors(painter);
    }
}

QImage Canvas::mosaic(const QImage& _image)
{
    return _image.copy()
        .scaled(_image.width() / 9, _image.height() / 9, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
        .scaled(_image.width(), _image.height());
}

void Canvas::clear()
{
    disable();
    modified_ = PaintType::UNMODIFIED;
    focusOn(nullptr);
    commands_.clear();
    redo_stack_.clear();
}

void Canvas::reset()
{
    clear();
    canvas_.fill(Qt::transparent);
    modified_ = PaintType::REPAINT_ALL;
}

void Canvas::copy()
{
    if(enabled_)
        copied_cmd_ = focus_cmd_;
}

void Canvas::paste()
{
    if (enabled_ && copied_cmd_) {
        hover_cmd_ = make_shared<PaintCommand>(*copied_cmd_);
        connect(hover_cmd_.get(), &PaintCommand::modified, this, &Canvas::modified);
        hover_cmd_->move({ 16, 16 });

        focusOn(hover_cmd_);
        commands_.push(hover_cmd_);
        redo_stack_.clear();
    }
}

void Canvas::remove()
{
    if (enabled_ && focus_cmd_) {
        HIDE_AND_COPY_CMD(focus_cmd_);
        commands_.push(focus_cmd_);
        redo_stack_.clear();
        focusOn(nullptr);
    }
}

///////////////////////////////////////////////////// UNDO / REDO ////////////////////////////////////////////////////////////
void Canvas::undo()
{
    if (!enabled_) return;

    while (!commands_.empty()) {
        auto ptr = commands_.back();

        // text cmd may be invalid
        if (ptr->isValid()) {
            redo_stack_.push(ptr);
            commands_.pop();

            focusOn(commands_.empty() ? nullptr : commands_.back());

            if (ptr->previous() != nullptr) ptr->previous()->visible(true);

            break;
        }
        commands_.pop();
    }
}

void Canvas::redo()
{
    if (!enabled_ || redo_stack_.empty()) return;

    auto ptr = redo_stack_.back();
    commands_.push(ptr);
    redo_stack_.pop();

    focusOn(commands_.back());

    if (ptr->previous() != nullptr) ptr->previous()->visible(false);
}
