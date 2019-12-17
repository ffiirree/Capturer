#include "screenshoter.h"
#include <cmath>
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QDesktopWidget>
#include <QFileDialog>
#include <QClipboard>
#include <QMouseEvent>
#include <QStandardPaths>
#include <QShortcut>
#include <QDateTime>
#include "circlecursor.h"
#include "logging.h"

ScreenShoter::ScreenShoter(QWidget *parent)
    : Selector(parent)
{
    menu_ = new ImageEditMenu(this);
    magnifier_ = new Magnifier(this);
    canvas_ = QPixmap(size());

    connect(menu_, &ImageEditMenu::save, this, &ScreenShoter::save);
    connect(menu_, &ImageEditMenu::copy, this, &ScreenShoter::copy);
    connect(menu_, &ImageEditMenu::fix,  this, &ScreenShoter::pin);
    connect(menu_, &ImageEditMenu::exit, this, &ScreenShoter::exit);

    connect(menu_, &ImageEditMenu::undo, this, &ScreenShoter::undo);
    connect(menu_, &ImageEditMenu::redo, this, &ScreenShoter::redo);

    connect(menu_, &ImageEditMenu::paint, [this](Graph graph) {
        switch (graph) {
        case Graph::NONE:   edit_status_ = EditStatus::NONE; if(commands_.empty()) CAPTURED(); break;
        default:            edit_status_ = EditStatus::READY | graph; LOCKED();break;
        }
        focus_cmd_ = nullptr; update();
    });

    // attrs changed
    connect(menu_, &ImageEditMenu::changed, [this](Graph graph){
        if(focus_cmd_ && focus_cmd_->graph() == graph) {
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
        if(graph == Graph::ERASER || graph == Graph::MOSAIC) {
            circle_cursor_.setWidth(menu_->pen(graph).width());
            setCursor(QCursor(circle_cursor_.cursor()));
        }

        if(focus_cmd_ && focus_cmd_->graph() == graph && graph == Graph::TEXT) {
            focus_cmd_->font(menu_->font(Graph::TEXT));
        }
    });

    // repaint when stack changed
    connect(&commands_, &CommandStack::increased, [this]() { focusOn(commands_.back()); modified(PaintType::DRAW_FINISHED); });
    connect(&commands_, &CommandStack::decreased, [this]() { modified(PaintType::REPAINT_ALL); });
    connect(&commands_, &CommandStack::emptied, [this](bool empty) {
        if(empty) CAPTURED(); else LOCKED();
    });

    // disable redo/undo
    connect(&commands_, &CommandStack::emptied, menu_, &ImageEditMenu::disableUndo);
    connect(&redo_stack_, &CommandStack::emptied, menu_, &ImageEditMenu::disableRedo);

    // show menu
    connect(this, &ScreenShoter::captured, [this](){ LOG(INFO) << "captured"; menu_->show(); moveMenu(); });

    // reset menu
    connect(&commands_, &CommandStack::emptied, [this](bool empty) { if(empty) menu_->reset(); });

    // move menu
    connect(this, &ScreenShoter::moved, this, &ScreenShoter::moveMenu);
    connect(this, &ScreenShoter::resized, this, &ScreenShoter::moveMenu);

    // shortcuts
    registerShortcuts();
}

void ScreenShoter::start()
{
    if(status_ != INITIAL) return;

    auto virtual_geometry = QGuiApplication::primaryScreen()->virtualGeometry();
    captured_screen_ = QGuiApplication::primaryScreen()->grabWindow(QApplication::desktop()->winId(),
                                                                    virtual_geometry.left(),
                                                                    virtual_geometry.top(),
                                                                    virtual_geometry.width(),
                                                                    virtual_geometry.height());
    modified(PaintType::REPAINT_ALL);

    Selector::start();
}

void ScreenShoter::focusOn(shared_ptr<PaintCommand> cmd)
{
    if(focus_cmd_ != nullptr && focus_cmd_ != cmd) {
        focus_cmd_->setFocus(false);
        if(!focus_cmd_->isValid()) {
            commands_.remove(focus_cmd_);
        }
    }
    focus_cmd_ = cmd;
    if(cmd) focus_cmd_->setFocus(true);
}

void ScreenShoter::mousePressEvent(QMouseEvent *event)
{
    if(event->button() != Qt::LeftButton)
        return;

    auto mouse_pos = event->pos();

    if(status_ == LOCKED) {
        focusOn(nullptr);
        // Border => move
        if(hover_position_ & Resizer::BORDER) {
            move_begin_ = mouse_pos;

            focusOn(hover_cmd_);

            edit_status_ |= EditStatus::GRAPH_MOVING;
        }
        // Anchor => resize
        else if(hover_position_ & Resizer::ANCHOR) {
            resize_begin_ = mouse_pos;

            focusOn(hover_cmd_);

            edit_status_ |= GRAPH_RESIZING;
        }
        // rotate
        else if(hover_position_ == Resizer::ROTATE_ANCHOR) {
            focusOn(hover_cmd_);

            edit_status_ |= GRAPH_ROTATING;
        }
        // Not border and anchor => Paint
        else {
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
                pen.setBrush(QBrush(mosaic(canvas_.toImage())));
                hover_cmd_ = make_shared<PaintCommand>(graph, pen, false, event->pos());
                break;

            case Graph::TEXT:
                hover_cmd_ = make_shared<PaintCommand>(graph, pen, false, event->pos());
                hover_cmd_->font(menu_->font(Graph::TEXT));
                break;

            case Graph::ERASER:
                pen.setBrush(QBrush(captured_screen_));
                hover_cmd_ = make_shared<PaintCommand>(graph, pen, false, event->pos());
                break;

            default: LOG(ERROR) << "Error type:" << graph; break;
            }
            if(hover_cmd_) {
                DO(hover_cmd_);
                modified(PaintType::DRAW_MODIFING);
                connect(hover_cmd_.get(), &PaintCommand::modified, this, &ScreenShoter::modified);
                edit_status_ |= GRAPH_PAINTING;
            }
        }
    }

    Selector::mousePressEvent(event);
}

void ScreenShoter::mouseMoveEvent(QMouseEvent* event)
{
    auto mouse_pos = event->pos();

    if(event->buttons() & Qt::LeftButton) {
        if(status_ == LOCKED) {
            switch (edit_status_ & OPERATION_MASK) {
            case GRAPH_PAINTING:
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
        else if(status_ == MOVING) {
            redo_stack_.clear();
            commands_.clear();
        }
    }

    // setCursor
    if(edit_status_ & Graph::ERASER || edit_status_ & Graph::MOSAIC) {
        setCursor(QCursor(circle_cursor_.cursor()));
    }
    else if(event->buttons() == Qt::NoButton && status_ == LOCKED) {
        updateHoverPos(event->pos());
        setCursorByHoverPos(hover_position_);
    }

    moveMagnifier();
    Selector::mouseMoveEvent(event);
}

void ScreenShoter::mouseReleaseEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton && status_ == LOCKED) {
        if((edit_status_ & OPERATION_MASK) == GRAPH_PAINTING)
            emit modified(PaintType::DRAW_FINISHED);

        edit_status_ &= ~OPERATION_MASK;
    }

    Selector::mouseReleaseEvent(event);
}

void ScreenShoter::wheelEvent(QWheelEvent * event)
{
    auto delta = event->delta() / 120;
    if(edit_status_ & Graph::ERASER) {
        menu_->pen(Graph::ERASER, menu_->pen(Graph::ERASER).width() + delta);
    }
    else if(edit_status_ & Graph::MOSAIC) {
        menu_->pen(Graph::MOSAIC, menu_->pen(Graph::MOSAIC).width() + delta);
    }
}

void ScreenShoter::updateHoverPos(const QPoint& pos)
{
    hover_position_ = Resizer::DEFAULT;
    hover_cmd_ = shared_ptr<PaintCommand>(nullptr);

    for(auto& command: commands_.commands()) {
        switch (command->graph()) {
        case Graph::RECTANGLE:
        {
            hover_position_ = command->resizer().absolutePos(pos);

            if(hover_position_ & Resizer::ADJUST_AREA) {
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

            if(r1.contains(pos) && !r2.contains(pos)) {
                hover_position_ = Resizer::BORDER;
                hover_cmd_ = command;
                return;
            }
            else {
                hover_position_ = resizer.absolutePos(pos);
                if(hover_position_ & Resizer::ANCHOR) {
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


            if(x1y1_anchor.contains(pos)) {
                hover_position_ = Resizer::X1Y1_ANCHOR;
                hover_cmd_ = command;
                return;
            }
            else if(x2y2_anchor.contains(pos)) {
                hover_position_ = Resizer::X2Y2_ANCHOR;
                hover_cmd_ = command;
                return;
            }
            else if(polygon.containsPoint(pos, Qt::OddEvenFill)) {
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

            if(hover_position_ & Resizer::ADJUST_AREA || hover_position_ == Resizer::ROTATE_ANCHOR) {
                hover_cmd_ = command;
                return;
            }
            break;
        }

        default: break;
        }
    }
}

void ScreenShoter::setCursorByHoverPos(Resizer::PointPosition pos, const QCursor & default_cursor)
{
    switch (pos) {
    case Resizer::X1_ANCHOR:    setCursor(Qt::SizeHorCursor); break;
    case Resizer::X2_ANCHOR:    setCursor(Qt::SizeHorCursor); break;
    case Resizer::Y1_ANCHOR:    setCursor(Qt::SizeVerCursor); break;
    case Resizer::Y2_ANCHOR:    setCursor(Qt::SizeVerCursor); break;
    case Resizer::X1Y1_ANCHOR:  setCursor(Qt::SizeFDiagCursor); break;
    case Resizer::X1Y2_ANCHOR:  setCursor(Qt::SizeBDiagCursor); break;
    case Resizer::X2Y1_ANCHOR:  setCursor(Qt::SizeBDiagCursor); break;
    case Resizer::X2Y2_ANCHOR:  setCursor(Qt::SizeFDiagCursor); break;
    case Resizer::BORDER:
    case Resizer::X1_BORDER:
    case Resizer::X2_BORDER:
    case Resizer::Y1_BORDER:
    case Resizer::Y2_BORDER:    setCursor(Qt::SizeAllCursor); break;
    case Resizer::ROTATE_ANCHOR: setCursor(QPixmap(":/icon/res/rotate").scaled(20, 20)); break;

    default: setCursor(default_cursor); break;
    }
}

void ScreenShoter::keyPressEvent(QKeyEvent *event)
{
    if((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && status_ >= CAPTURED) {
        copy();
        exit();
    }

    if(event->key() == Qt::Key_Escape) {
        exit();
    }

    Selector::keyPressEvent(event);
}

void ScreenShoter::moveMenu()
{
    auto area = selected();
    auto right = area.right() - menu_->width() + 1;
    if(right < 0) right = 0;

    if(area.bottom() + (menu_->height() * 2 + 5/*margin*/) < rect().height()) {
        menu_->move(right, area.bottom() + 3);
        menu_->setSubMenuShowBelow();
    }
    else if(area.top() - (menu_->height() * 2 + 5) > 0) {
        menu_->move(right, area.top() - menu_->height() - 3);
        menu_->setSubMenuShowAbove();
    }
    else {
        menu_->move(right, area.bottom() - menu_->height() - 3);
        menu_->setSubMenuShowAbove();
    }
}

void ScreenShoter::moveMagnifier()
{
    if(status_ < CAPTURED || status_ == RESIZING) {
        magnifier_->pixmap(captured_screen_.copy(magnifier_->mrect()));
        magnifier_->show();
        magnifier_->move(QCursor::pos().x() + 10, QCursor::pos().y() + 10);
    } else {
        magnifier_->hide();
    }
}

void ScreenShoter::exit()
{
    commands_.clear();

    menu_->reset();
    menu_->hide();
    text_edit_ = nullptr;
    focus_cmd_ = nullptr;

    Selector::exit();
}

QImage ScreenShoter::mosaic(const QImage& _image)
{
    auto image = _image.copy();

    for(auto h = 3; h < image.height(); h += 7) {
        for(auto w = 3; w < image.width(); w += 7) {

            //
            for(auto i = 0; i < 7 && h + i - 3 < image.height(); ++i) {
                for(auto j = 0; j < 7 && w + j - 3 < image.width(); ++j) {
                    image.setPixel(w + j - 3, h + i - 3, image.pixel(w, h));
                }
            }
        }
    }

    return image;
}

void ScreenShoter::updateCanvas()
{
    painter_.begin(&canvas_);
    switch (modified_) {
    case PaintType::DRAW_FINISHED:
        if(focus_cmd_ == nullptr) {
            LOG(ERROR) << "nullptr";
            return;
        }

        focus_cmd_->repaint(&painter_);
        break;

    case PaintType::DRAW_MODIFIED:
        if(focus_cmd_ == nullptr) {
            LOG(ERROR) << "nullptr";
            return;
        }

        focus_cmd_->draw_modified(&painter_);
        break;

    case PaintType::REPAINT_ALL:
        painter_.drawPixmap(0, 0, captured_screen_);
        for(auto& command: commands_.commands()) {
            command->repaint(&painter_);
        }
        break;

    default: break;
    }
    painter_.end();
}

void ScreenShoter::paintEvent(QPaintEvent *event)
{
    // 1. canvas
    updateCanvas();

    // 2. window
    painter_.begin(this);
    painter_.drawPixmap(0, 0, canvas_);

    if(modified_ == PaintType::DRAW_MODIFING) {
        if(focus_cmd_) {
            focus_cmd_->draw_modified(&painter_);
        }
    }

    //
    if(focus_cmd_ != nullptr) {
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
            painter_.setPen(QPen(Qt::black, 1, Qt::SolidLine));
            painter_.setRenderHint(QPainter::Antialiasing, true);
            painter_.setBrush(Qt::white);
            painter_.drawEllipse(resizer.rotateAnchor());
            painter_.restore();

            painter_.setPen(QPen(Qt::black, 1, Qt::DashLine));
            // box border
            painter_.setPen(QPen(Qt::black, 1, Qt::DashLine));
            painter_.drawRect(resizer.rect());

            // anchors
            painter_.setPen(QPen(Qt::black, 1, Qt::SolidLine));
            painter_.drawRect(resizer.X1Y1Anchor());
            painter_.drawRect(resizer.X2Y1Anchor());
            painter_.drawRect(resizer.X1Y2Anchor());
            painter_.drawRect(resizer.X2Y2Anchor());
            break;
        }

        default: break;
        }
        painter_.restore();
    }
    painter_.end();

    Selector::paintEvent(event);
}

void ScreenShoter::snipped()
{
    auto image = snippedImage();
    QApplication::clipboard()->setPixmap(image);

    emit SNIPPED(image, selected().topLeft());

    history_.push_back({captured_screen_, selected(), commands_});
    history_idx_ = history_.size() - 1;
}

QPixmap ScreenShoter::snippedImage()
{
    return canvas_.copy(selected());
}

void ScreenShoter::save()
{
    QString default_filepath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QString default_filename = "Capturer_picture_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz") + ".png";
#ifdef _WIN32
    auto filename = QFileDialog::getSaveFileName(this,
                                                 tr("Save Image"),
                                                 default_filepath + QDir::separator() + default_filename,
                                                 "PNG(*.png);;JPEG(*.jpg *.jpeg);;BMP(*.bmp)");

    if(!filename.isEmpty()) {
        snippedImage().save(filename);
        emit SHOW_MESSAGE("Capturer<PICTURE>", "Path: " + filename);

        snipped();

        exit();
    }
#elif __linux__
    auto filename = default_filepath + QDir::separator() + default_filename;

    snippedImage().save(filename);
    emit SHOW_MESSAGE("Capturer<PICTURE>", "Path: " + filename);

    snipped();

    exit();
#endif
}

void ScreenShoter::copy()
{
    snipped();

    exit();
}

void ScreenShoter::pin()
{
    snipped();
    emit FIX_IMAGE(snippedImage(), { selected().topLeft() });

    exit();
}

void ScreenShoter::registerShortcuts()
{
    connect(new QShortcut(Qt::CTRL + Qt::Key_S, this), &QShortcut::activated, [this](){
        if(status_ == CAPTURED) {
            save();
        }
    });

    connect(new QShortcut(Qt::Key_P, this), &QShortcut::activated, [this](){
        if(status_ == CAPTURED) {
            pin();
        }
    });

    connect(new QShortcut(Qt::CTRL + Qt::Key_Z, this), &QShortcut::activated, this, &ScreenShoter::undo);
    connect(new QShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_Z, this), &QShortcut::activated, this, &ScreenShoter::redo);

    auto setCurrent = [this](const History& history) {
        captured_screen_ = history.image_;
        box_.reset(history.rect_);
        commands_ = history.commands_;
        redo_stack_.clear();
        modified(PaintType::REPAINT_ALL);

        CAPTURED();
        LOCKED();
    };

    connect(new QShortcut(Qt::Key_PageUp, this), &QShortcut::activated, [=](){
        if(history_idx_ < history_.size()) {
            setCurrent(history_[history_idx_]);
            if(history_idx_ > 0) history_idx_--;
        }
    });

    connect(new QShortcut(Qt::Key_PageDown, this), &QShortcut::activated, [=](){
        if(history_idx_ < history_.size()) {
            setCurrent(history_[history_idx_]);
            if(history_idx_ < history_.size() - 1) history_idx_++;
        }
    });

    connect(new QShortcut(Qt::CTRL + Qt::Key_C, this), &QShortcut::activated, [=](){
        if(status_ < CAPTURED) {
            LOG(INFO) << "CTRL + C";
            QApplication::clipboard()->setText(magnifier_->getColorStringValue());
        }
    });
}

///////////////////////////////////////////////////// UNDO / REDO ////////////////////////////////////////////////////////////
void ScreenShoter::undo()
{
    if(commands_.empty()) return;

    auto temp = commands_.back();
    if(focus_cmd_ == temp)
        focusOn(nullptr);

    redo_stack_.push(temp);
    commands_.pop();
}

void ScreenShoter::redo()
{
    if(redo_stack_.empty()) return;

    commands_.push(redo_stack_.back());
    redo_stack_.pop();
}
