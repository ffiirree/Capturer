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
    connect(menu_, &ImageEditMenu::exit, [this]() { emit close(); });

    connect(menu_, &ImageEditMenu::undo, this, &Canvas::undo);
    connect(menu_, &ImageEditMenu::redo, this, &Canvas::redo);


    connect(menu_, &ImageEditMenu::paint, [this](Graph graph) {
        switch (graph) {
        case Graph::NONE:
            edit_status_ = EditStatus::NONE;
            //if (commands_.empty())
            //    CAPTURED();
            focusOn(nullptr);
            break;

        default:
            edit_status_ = static_cast<EditStatus>((edit_status_ & 0xffff0000) | graph);
            //LOCKED();
            focusOn((focus_cmd_ && focus_cmd_->graph() == graph) ? focus_cmd_ : nullptr);
            break;
        }
    });

    //connect(this, &ScreenShoter::focusOnGraph, menu_, &ImageEditMenu::paintGraph);

    // attrs changed
    connect(menu_, &ImageEditMenu::changed, [this](Graph graph) {
        if (focus_cmd_ && focus_cmd_->graph() == graph) {
            switch (graph) {
            case Graph::ERASER:
            case Graph::MOSAIC: break;
            default:
                focus_cmd_->pen(menu_->pen(graph));
                focus_cmd_->setFill(menu_->fill(graph));
                break;
            }
        }

        // change the cursor
        if (graph == Graph::ERASER || graph == Graph::MOSAIC) {
            circle_cursor_.setWidth(menu_->pen(graph).width());
            //setCursor(QCursor(circle_cursor_.cursor()));
        }

        if (focus_cmd_ && focus_cmd_->graph() == graph && graph == Graph::TEXT) {
            focus_cmd_->font(menu_->font(Graph::TEXT));
        }
    });

    // repaint when stack changed
    connect(&commands_, &CommandStack::increased, [this]() { modified(PaintType::DRAW_FINISHED); });
    connect(&commands_, &CommandStack::decreased, [this]() { modified(PaintType::REPAINT_ALL); });
    //connect(&commands_, &CommandStack::emptied, [this](bool empty) {
    //    if (empty) CAPTURED(); else LOCKED();
    //});

    // disable redo/undo
    connect(&commands_, &CommandStack::emptied, menu_, &ImageEditMenu::disableUndo);
    connect(&redo_stack_, &CommandStack::emptied, menu_, &ImageEditMenu::disableRedo);

    // reset menu
    connect(&commands_, &CommandStack::emptied, [this](bool empty) { if (empty) menu_->reset(); });

    // move menu
    //connect(this, &Canvas::moved, this, &ScreenShoter::moveMenu);
    //connect(this, &Canvas::resized, this, &ScreenShoter::moveMenu);

}

void Canvas::start()
{
    menu_->show();
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
        //emit focusOnGraph(focus_cmd_->graph());  // must after focus_cmd_ = cmd
    }
}

bool Canvas::eventFilter(QObject* object, QEvent* event)
{
    switch (event->type())
    {
    case QEvent::MouseButtonPress:
        mousePressEvent(static_cast<QMouseEvent*>(event));
        return true;
    case QEvent::MouseButtonRelease:
        mouseReleaseEvent(static_cast<QMouseEvent*>(event));
        return true;

    case QEvent::HoverMove:
    case QEvent::MouseMove:
        mouseMoveEvent(static_cast<QMouseEvent*>(event));
        return true;

    case QEvent::Paint:
        paintEvent(static_cast<QPaintEvent*>(event));
        return true;

    default:
        break;
    }

    return false;
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
    else if (edit_status_ != EditStatus::NONE) {
        auto graph = Graph(edit_status_ & GRAPH_MASK);
        auto pen = menu_->pen(graph);
        auto fill = menu_->fill(graph);

        switch (graph) {
        case Graph::RECTANGLE:
        case Graph::CIRCLE:
        case Graph::ARROW:
        case Graph::LINE:
        case Graph::CURVES:
            hover_cmd_ = make_shared<PaintCommand>(graph, pen, fill, event->pos());
            break;

        case Graph::MOSAIC:
            pen.setBrush(QBrush(mosaic(canvas_->toImage())));
            hover_cmd_ = make_shared<PaintCommand>(graph, pen, false, event->pos());
            break;

        //case Graph::TEXT:
        //    hover_cmd_ = make_shared<PaintCommand>(graph, pen, false, event->pos(), this);
        //    hover_cmd_->font(menu_->font(Graph::TEXT));
        //    break;

        case Graph::ERASER:
            pen.setBrush(QBrush(captured_screen_));
            hover_cmd_ = make_shared<PaintCommand>(graph, pen, false, event->pos());
            break;

        default: LOG(ERROR) << "Error type:" << graph; break;
        }
        edit_status_ |= GRAPH_CREATING;
    }

    if (hover_cmd_) {
        hover_cmd_->visible(true);
        focusOn(hover_cmd_);
        modified(PaintType::DRAW_MODIFYING);
        connect(hover_cmd_.get(), &PaintCommand::modified, this, &Canvas::modified);
    }

    emit update();
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

    // setCursor
    if (edit_status_ & Graph::ERASER || edit_status_ & Graph::MOSAIC) {
        circle_cursor_.setWidth(menu_->pen(Graph(edit_status_ & GRAPH_MASK)).width());
        device_->setCursor(QCursor(circle_cursor_.cursor()));
    }
    else if (event->buttons() == Qt::NoButton) {
        LOG(ERROR) << "MOVE";
        updateHoverPos(event->pos());
        setCursorByHoverPos(hover_position_);
    }
    emit update();
}

void Canvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton
        && edit_status_ != EditStatus::NONE) {

        if (!focus_cmd_) LOG(ERROR) << "Focused cmd is nullptr!";

        if ((!focus_cmd_->previous() && focus_cmd_->isValid())           // created
            || (focus_cmd_->previous() && focus_cmd_->adjusted())   // moved/resized..
            || ((edit_status_ & GRAPH_MASK) == Graph::TEXT)) {      // check text is valid when it loses focus
            commands_.push(focus_cmd_);
            redo_stack_.clear();
        }
        else {                                                          // Invalid, no modified. Back to the previous
            if (focus_cmd_->previous()) {
                focus_cmd_->previous()->visible(true);
            }
            // foucs on null or the previous cmd
            focusOn(focus_cmd_->previous());
        }

        edit_status_ &= ~OPERATION_MASK;
    }

    emit update();
}


void Canvas::updateHoverPos(const QPoint& pos)
{
    hover_position_ = Resizer::DEFAULT;
    hover_cmd_ = shared_ptr<PaintCommand>(nullptr);

    for (auto& command : commands_.commands()) {
        if (!command->visible()) continue;
        switch (command->graph()) {
        case Graph::RECTANGLE:
        {
            hover_position_ = command->resizer().absolutePos(pos);

            if (hover_position_ & Resizer::ADJUST_AREA) {
                hover_cmd_ = command;
                return;
            }
            break;
        }

        case Graph::CIRCLE:
        {
            auto resizer = command->resizer();

            QRegion r1(resizer.rect().adjusted(-2, -2, 2, 2), QRegion::Ellipse);
            QRegion r2(resizer.rect().adjusted(2, 2, -2, -2), QRegion::Ellipse);

            if (r1.contains(pos) && !r2.contains(pos)) {
                hover_position_ = Resizer::BORDER;
                hover_cmd_ = command;
                return;
            }
            else {
                hover_position_ = resizer.absolutePos(pos);
                if (hover_position_ & Resizer::ANCHOR) {
                    hover_cmd_ = command;
                    return;
                }
            }
            break;
        }

        case Graph::ARROW:
        case Graph::LINE:
        {
            auto x1y1_anchor = command->resizer().X1Y1Anchor();
            auto x2y2_anchor = command->resizer().X2Y2Anchor();
            QPolygon polygon;
            polygon.push_back(x1y1_anchor.topLeft());
            polygon.push_back(x1y1_anchor.bottomRight());
            polygon.push_back(x2y2_anchor.bottomRight());
            polygon.push_back(x2y2_anchor.topLeft());

            if (x1y1_anchor.contains(pos)) {
                hover_position_ = Resizer::X1Y1_ANCHOR;
                hover_cmd_ = command;
                return;
            }
            else if (x2y2_anchor.contains(pos)) {
                hover_position_ = Resizer::X2Y2_ANCHOR;
                hover_cmd_ = command;
                return;
            }
            else if (polygon.containsPoint(pos, Qt::OddEvenFill)) {
                hover_position_ = Resizer::BORDER;
                hover_cmd_ = command;
                return;
            }
            break;
        }

        case Graph::TEXT:
        {
            Resizer resizer(command->geometry().adjusted(-5, -5, 5, 5));
            resizer.enableRotate(true);
            hover_position_ = resizer.absolutePos(pos);
            switch (hover_position_) {
            case Resizer::INSIDE:
            case Resizer::X1_ANCHOR:
            case Resizer::Y2_ANCHOR:
            case Resizer::Y1_ANCHOR:
            case Resizer::X2_ANCHOR: hover_position_ = Resizer::BORDER; break;
            default: break;
            }

            if (hover_position_ & Resizer::ADJUST_AREA || hover_position_ == Resizer::ROTATE_ANCHOR) {
                hover_cmd_ = command;
                return;
            }
            break;
        }

        default: break;
        }
    }
}

void Canvas::setCursorByHoverPos(Resizer::PointPosition pos, const QCursor& default_cursor)
{
    switch (pos) {
    case Resizer::X1_ANCHOR:    device_->setCursor(Qt::SizeHorCursor); break;
    case Resizer::X2_ANCHOR:    device_->setCursor(Qt::SizeHorCursor); break;
    case Resizer::Y1_ANCHOR:    device_->setCursor(Qt::SizeVerCursor); break;
    case Resizer::Y2_ANCHOR:    device_->setCursor(Qt::SizeVerCursor); break;
    case Resizer::X1Y1_ANCHOR:  device_->setCursor(Qt::SizeFDiagCursor); break;
    case Resizer::X1Y2_ANCHOR:  device_->setCursor(Qt::SizeBDiagCursor); break;
    case Resizer::X2Y1_ANCHOR:  device_->setCursor(Qt::SizeBDiagCursor); break;
    case Resizer::X2Y2_ANCHOR:  device_->setCursor(Qt::SizeFDiagCursor); break;
    case Resizer::BORDER:
    case Resizer::X1_BORDER:
    case Resizer::X2_BORDER:
    case Resizer::Y1_BORDER:
    case Resizer::Y2_BORDER:    device_->setCursor(Qt::SizeAllCursor); break;
    case Resizer::ROTATE_ANCHOR: device_->setCursor(QPixmap(":/icon/res/rotate").scaled(22, 22, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)); break;

    default: device_->setCursor(default_cursor); break;
    }
}

void Canvas::updateCanvas()
{
    if (!modified_) return;

    QPainter painter;
    painter.begin(canvas_);

    switch (modified_) {
    case PaintType::DRAW_FINISHED:
        CHECK(focus_cmd_);

        focus_cmd_->repaint(&painter);
        break;

    case PaintType::DRAW_MODIFIED:
        CHECK(focus_cmd_);

        focus_cmd_->draw_modified(&painter);
        break;

    case PaintType::REPAINT_ALL:
        painter.drawPixmap(0, 0, captured_screen_);
        for (auto& command : commands_.commands()) {
            command->repaint(&painter);
        }
        break;

    default: break;
    }
}

void Canvas::paintEvent(QPaintEvent* event)
{
    // 1. canvas
    updateCanvas();

    QPainter painter_;
    // 2. window
    painter_.begin(device_);
    painter_.drawPixmap(0, 0, *canvas_);

    if (modified_ == PaintType::DRAW_MODIFYING || modified_ == PaintType::REPAINT_ALL) {
        if (focus_cmd_) {
            focus_cmd_->draw_modified(&painter_);
        }
    }

    //
    if (focus_cmd_ != nullptr && focus_cmd_->visible()) {
        painter_.save();
        switch (focus_cmd_->graph()) {
        case Graph::RECTANGLE:
        case Graph::ELLIPSE:
            drawSelector(&painter_, focus_cmd_->resizer());
            break;

        case Graph::LINE:
        case Graph::ARROW:
            painter_.setPen(QPen(Qt::black, 1, Qt::DashLine));
            painter_.setRenderHint(QPainter::Antialiasing, true);
            painter_.drawLine(focus_cmd_->resizer().point1(), focus_cmd_->resizer().point2());

            painter_.setPen(QPen(Qt::black, 1, Qt::SolidLine));
            painter_.drawRect(focus_cmd_->resizer().X1Y1Anchor());
            painter_.drawRect(focus_cmd_->resizer().X2Y2Anchor());
            break;

        case Graph::TEXT:
        {
            Resizer resizer(focus_cmd_->geometry().adjusted(-5, -5, 5, 5));

            painter_.save();
            painter_.setPen(QPen(QColor("#333"), 2, Qt::SolidLine));
            painter_.setRenderHint(QPainter::Antialiasing, true);
            painter_.setBrush(Qt::white);
            painter_.drawEllipse(resizer.rotateAnchor());
            painter_.restore();

            // box border
            painter_.setPen(QPen(Qt::black, 1, Qt::DashLine));
            painter_.drawRect(resizer.rect());

            // anchors
            painter_.setPen(QPen(Qt::black, 1, Qt::SolidLine));
            painter_.drawRects(resizer.cornerAnchors());
            break;
        }

        default: break;
        }
        painter_.restore();
    }
    painter_.end();
    modified_ = PaintType::UNMODIFIED;
}

QImage Canvas::mosaic(const QImage& _image)
{
    return _image.copy()
        .scaled(_image.width() / 9, _image.height() / 9, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
        .scaled(_image.width(), _image.height());
}


void Canvas::drawSelector(QPainter* painter, const Resizer& resizer)
{
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setBrush(Qt::NoBrush);

    // box border
    painter->setPen(QPen(Qt::black, 1, Qt::DashLine));
    painter->drawRect(resizer.rect());

    // anchors
    painter->setPen(QPen(Qt::black, 1, Qt::SolidLine));
    painter->drawRects(resizer.anchors());
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
