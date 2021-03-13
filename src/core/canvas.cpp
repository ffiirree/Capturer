#include "canvas.h"
#include <QMouseEvent>
#include <QPainter>
#include <QGraphicsItem>
#include "logging.h"

Canvas::Canvas(QWidget *parent)
    : QObject(parent)
{
    menu_ = new ImageEditMenu(parent);
    //connect(menu_, &ImageEditMenu::save, this, &Canvas::save);
    //connect(menu_, &ImageEditMenu::ok, this, &Canvas::copy);
    //connect(menu_, &ImageEditMenu::fix, this, &Canvas::pin);
    connect(menu_, &ImageEditMenu::exit, [this]() { clear(); emit close(); });

    connect(menu_, &ImageEditMenu::undo, this, &Canvas::undo);
    connect(menu_, &ImageEditMenu::redo, this, &Canvas::redo);
    connect(menu_, &ImageEditMenu::graphChanged, this, &Canvas::changeGraph);
    connect(this, &Canvas::focusOnGraph, menu_, &ImageEditMenu::paintGraph);
    //connect(this, &ScreenShoter::focusOnGraph, menu_, &ImageEditMenu::paintGraph);

    // attrs changed
    connect(menu_, &ImageEditMenu::styleChanged, [this](Graph graph) {
        if (focus_cmd_ && focus_cmd_->graph() == graph) {
            switch (graph) {
            case Graph::ERASER:
            case Graph::MOSAIC:
                //circle_cursor_.setWidth(menu_->pen(graph).width());
                //setCursor(QCursor(circle_cursor_.cursor()));
                break;

            case Graph::TEXT:
                focus_cmd_->font(menu_->font(Graph::TEXT));
            default:
                focus_cmd_->pen(menu_->pen(graph));
                focus_cmd_->setFill(menu_->fill(graph));
                break;
            }
        }
    });

    // repaint when stack changed
    connect(&commands_, &CommandStack::increased, [this]() { modified(PaintType::DRAW_FINISHED); });
    connect(&commands_, &CommandStack::decreased, [this]() { modified(PaintType::REPAINT_ALL); });
    // disable redo/undo
    connect(&commands_, &CommandStack::emptied, menu_, &ImageEditMenu::disableUndo);
    connect(&redo_stack_, &CommandStack::emptied, menu_, &ImageEditMenu::disableRedo);

    // reset menu
    connect(&commands_, &CommandStack::emptied, [this](bool empty) { if (empty) menu_->reset(); });
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

void Canvas::canvas(const QPixmap& canvas)
{ 
    canvas_ = canvas.copy(); 
    backup_ = canvas.copy();
    modified(PaintType::REPAINT_ALL); 
}

bool Canvas::editing()
{
    return (edit_status_ & GRAPH_MASK) != 0 || commands_.size();
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
        resize_begin_ = mouse_pos;

        HIDE_AND_COPY_CMD(hover_cmd_);

        edit_status_ |= GRAPH_RESIZING;
    }
    // rotate
    else if ((hover_position_ == Resizer::ROTATE_ANCHOR) && hover_cmd_ && hover_cmd_->visible()) {
        HIDE_AND_COPY_CMD(hover_cmd_);

        edit_status_ |= GRAPH_ROTATING;
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
        hover_cmd_->visible(true);
        focusOn(hover_cmd_);
        modified(PaintType::DRAW_MODIFYING);
        connect(hover_cmd_.get(), &PaintCommand::modified, this, &Canvas::modified);
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
            hover_cmd_->resize(hover_position_, mouse_pos - resize_begin_);
            resize_begin_ = mouse_pos;
            break;

        default:  break;
        }
    }

    if (event->buttons() == Qt::NoButton) {
        updateHoverPos(event->pos());
    }
}

QCursor Canvas::getCursorShape()
{
    if (edit_status_ & Graph::ERASER || edit_status_ & Graph::MOSAIC) {
        circle_cursor_.setWidth(menu_->pen(Graph(edit_status_ & GRAPH_MASK)).width());
        return QCursor(circle_cursor_.cursor());
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
    }
}

void Canvas::updateCanvas()
{
    if (!modified_) return;

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
    if (focus_cmd_ != nullptr && focus_cmd_->visible()) {
        if (modified_ == PaintType::DRAW_MODIFYING || modified_ == PaintType::REPAINT_ALL) {
            focus_cmd_->draw_modified(painter);
        }

        focus_cmd_->drawAnchors(painter);
    }

    modified_ = PaintType::UNMODIFIED;
}

QImage Canvas::mosaic(const QImage& _image)
{
    return _image.copy()
        .scaled(_image.width() / 9, _image.height() / 9, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
        .scaled(_image.width(), _image.height());
}

///////////////////////////////////////////////////// UNDO / REDO ////////////////////////////////////////////////////////////
void Canvas::undo()
{
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
    if (redo_stack_.empty()) return;

    auto ptr = redo_stack_.back();
    commands_.push(ptr);
    redo_stack_.pop();

    focusOn(commands_.back());

    if (ptr->previous() != nullptr) ptr->previous()->visible(false);
}


void Canvas::clear()
{
    commands_.clear();
    redo_stack_.clear();
    canvas_.fill(Qt::transparent);
    modified_ = PaintType::UNMODIFIED;
    emit changed();
}

void Canvas::copy()
{
    copied_cmd_ = focus_cmd_;
}

void Canvas::paste()
{
    if (copied_cmd_) {
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
    if (focus_cmd_) {
        HIDE_AND_COPY_CMD(focus_cmd_);
        commands_.push(focus_cmd_);
        redo_stack_.clear();
        focusOn(nullptr);
    }
}