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
        case Graph::NONE:   edit_status_ = NONE; if(undo_stack_.empty()) CAPTURED(); break;
        default:            edit_status_ = START_PAINTING | graph;      LOCKED();    break;
        }
        focus_ = nullptr; update();
    });

    // attrs changed
    connect(menu_, &ImageEditMenu::changed, [this](Graph graph){
        if(focus_ && focus_->graph() == graph) {
            switch (graph) {
            case ERASER:
            case MOSAIC: break;
            default:
                focus_->pen(menu_->pen(graph));
                focus_->setFill(menu_->fill(graph));
                break;
            }

            modified();
        }

        // change the cursor
        if(graph == ERASER || graph == MOSAIC) {
            circle_cursor_.setWidth(menu_->pen(graph).width());
            setCursor(QCursor(circle_cursor_.cursor()));
        }
    });

    // repaint when stack changed
    connect(&undo_stack_, SIGNAL(changed(size_t)), this, SLOT(modified()));
    connect(&undo_stack_, &CommandStack::emptied, [this](bool empty) {
        if(empty) CAPTURED(); else LOCKED();
    });

    // disable redo/undo
    connect(&undo_stack_, &CommandStack::emptied, menu_, &ImageEditMenu::disableUndo);
    connect(&redo_stack_, &CommandStack::emptied, menu_, &ImageEditMenu::disableRedo);

    // show menu
    connect(this, &ScreenShoter::captured, [this](){ menu_->show(); moveMenu(); });

    // reset menu
    connect(&undo_stack_, &CommandStack::emptied, [this](bool empty) { if(empty) menu_->reset(); });

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
    modified_ = true;
    paintOnCanvas();

    Selector::start();
}
void ScreenShoter::mousePressEvent(QMouseEvent *event)
{
    if(event->button() != Qt::LeftButton) return;

    auto mouse_pos = event->pos();
    if(status_ == LOCKED) {
        focus_ = nullptr;
        // Border => move
        if(cursor_graph_pos_ & Resizer::BORDER) {
            move_begin_ = mouse_pos;

            focus_ = command_;

            // Text
            if(command_->widget() != nullptr) {
                command_->widget()->show();
                command_->widget()->setFocus();
            }

            last_edit_status_ = edit_status_;
            edit_status_ = GRAPH_MOVING;
            modified();
        }
        // Anchor => Resize
        else if(cursor_graph_pos_ & Resizer::ANCHOR) {
            resize_begin_ = mouse_pos;

            focus_ = command_;

            last_edit_status_ = edit_status_;
            edit_status_ = GRAPH_RESIZING;
            modified();
        }
        // rotate
        else if(cursor_graph_pos_ == Resizer::ROTATE_ANCHOR) {
            focus_ = command_;

            last_edit_status_ = edit_status_;
            edit_status_ = GRAPH_ROTATING;
            modified();
        }
        // Not border and anchor => Paint
        else {
            auto graph = Graph(edit_status_ & GRAPH_MASK);
            auto pen = menu_->pen(graph);
            auto fill = menu_->fill(Graph::RECTANGLE);

            switch (graph) {
            case RECTANGLE:
            case CIRCLE:
            case ARROW:
            case LINE:
                command_ = focus_ = make_shared<PaintCommand>(graph, pen, fill, QVector<QPoint>{ event->pos(), event->pos() });
                DO(command_);
                break;

            case CURVES:
                command_ = focus_ = make_shared<PaintCommand>(graph, pen);
                command_->points({ event->pos() });
                DO(command_);
                break;

            case MOSAIC:
                pen.setBrush(QBrush(mosaic(canvas_.toImage())));
                command_ = focus_ = make_shared<PaintCommand>(graph, pen);
                command_->points({ event->pos() });
                DO(command_);
                break;

            case TEXT:
            {
                if(!text_edit_ || !text_edit_->toPlainText().isEmpty()) {
                    text_edit_ = new TextEdit(this);

                    text_edit_->setTextColor(pen.color());
                    text_edit_->setFont(menu_->font(Graph::TEXT));
                    text_edit_->setFocus();
                    text_edit_->show();

                    text_edit_->ensureCursorVisible();
                    connect(text_edit_, &TextEdit::textChanged, [this](){ modified(); });

                    auto command = make_shared<PaintCommand>(graph, pen);
                    command->widget(text_edit_);
                    DO(command);
                }
                text_edit_->move(event->pos());
                break;
            }

            case ERASER:
                pen.setBrush(QBrush(captured_screen_));
                command_ = focus_ = make_shared<PaintCommand>(graph, pen);
                command_->points({ event->pos() });

                DO(command_);
                break;

            default: LOG(INFO) << "Error type:" << graph; break;
            }
            edit_status_ = PAINTING | graph;
        }
    }

    Selector::mousePressEvent(event);
}

void ScreenShoter::mouseMoveEvent(QMouseEvent* event)
{
    auto mouse_pos = event->pos();

    if(event->buttons() & Qt::LeftButton) {
        if(status_ == LOCKED) {
            switch (edit_status_) {
            case PAINTING_RECTANGLE:
            case PAINTING_CIRCLE:
            case PAINTING_ARROW:
            case PAINTING_LINE:     command_->points()[1] = event->pos(); modified(); break;

            case PAINTING_MOSAIC:
            case ERASING:
            case PAINTING_CURVES:   command_->points().push_back(event->pos()); modified(); break;
            case PAINTING_TEXT: break;

            case GRAPH_MOVING:
            {
                move_end_ = mouse_pos;
                auto moved_d = move_end_ - move_begin_;
                switch (command_->graph()) {
                case  Graph::TEXT:
                    command_->widget()->move(command_->widget()->pos() + moved_d);
                    break;

                default:
                    command_->points()[0] += moved_d;
                    command_->points()[1] += moved_d;
                    break;
                }

                move_begin_ = move_end_;
                modified();
                break;
            }

            case GRAPH_RESIZING:
            {
                resize_end_ = mouse_pos;

                auto rx = resize_end_.x() - resize_begin_.x();
                auto ry = resize_end_.y() - resize_begin_.y();

                switch (cursor_graph_pos_) {
                case Resizer::X1Y1_ANCHOR:  command_->points()[0].rx() += rx; command_->points()[0].ry() += ry; break;
                case Resizer::X2Y1_ANCHOR:  command_->points()[1].rx() += rx; command_->points()[0].ry() += ry; break;
                case Resizer::X1Y2_ANCHOR:  command_->points()[0].rx() += rx; command_->points()[1].ry() += ry; break;
                case Resizer::X2Y2_ANCHOR:  command_->points()[1].rx() += rx; command_->points()[1].ry() += ry; break;
                case Resizer::X1_ANCHOR:    command_->points()[0].rx() += rx; break;
                case Resizer::X2_ANCHOR:    command_->points()[1].rx() += rx; break;
                case Resizer::Y1_ANCHOR:    command_->points()[0].ry() += ry; break;
                case Resizer::Y2_ANCHOR:    command_->points()[1].ry() += ry; break;
                default: break;
                }

                resize_begin_ = mouse_pos;
                modified();
                break;
            }

            default:  break;
            }
        }
        else if(status_ == MOVING) {
            redo_stack_.clear();
            undo_stack_.clear();
        }
    }

    // setCursor
    if(edit_status_ & ERASER || edit_status_ & MOSAIC) {
        setCursor(QCursor(circle_cursor_.cursor()));
    }
    else if(event->buttons() == Qt::NoButton && status_ == LOCKED) {
        command_ = getCursorPos(event->pos()); setCursorByPos(cursor_graph_pos_);
    }

    moveMagnifier();
    Selector::mouseMoveEvent(event);
}

void ScreenShoter::mouseReleaseEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton) {
        if(status_ == LOCKED) {
            switch (edit_status_) {
            case PAINTING_RECTANGLE:
            case PAINTING_CIRCLE:
            case PAINTING_ARROW:
            case PAINTING_LINE:
            case PAINTING_CURVES:
            case ERASING:
            case PAINTING_MOSAIC:
                if(command_->points().size() > 1 && command_->points()[0] == command_->points()[1]) {
                    undo_stack_.pop();
                    focus_ = nullptr;
                }

                edit_status_ = edit_status_ ^ END;
                break;

            case PAINTING_TEXT:
                edit_status_ = END_PAINTING_TEXT;
                break;

            case GRAPH_MOVING:
                edit_status_ = last_edit_status_;
                break;

            case GRAPH_RESIZING:
                edit_status_ = last_edit_status_;
                break;

            default: break;
            }
        }
    }

    Selector::mouseReleaseEvent(event);
}

void ScreenShoter::wheelEvent(QWheelEvent * event)
{
    auto delta = event->delta() / 120;
    if(edit_status_ & ERASER ) {
        menu_->pen(ERASER, menu_->pen(ERASER).width() + delta);
    }
    else if(edit_status_ & MOSAIC){
        menu_->pen(MOSAIC, menu_->pen(MOSAIC).width() + delta);
    }
}

double ScreenShoter::distance(const QPoint& p, const QPoint& p1, const QPoint&p2)
{
    auto d1s = std::pow(p1.x() - p.x(),  2) + std::pow(p1.y() - p.y(),  2);
    auto d2s = std::pow(p2.x() - p.x(),  2) + std::pow(p2.y() - p.y(),  2);
    auto d3s = std::pow(p1.x() - p2.x(), 2) + std::pow(p1.y() - p2.y(), 2);
    auto d3  = std::sqrt(d3s);

    // d1^2 - x^2 = d2^2 - (d3 - x)^2
    // x = (d1^2 - d2^2 + d3^2)/ (2 * d3)
    auto x = (d1s - d2s + d3s)/(2 * d3);
    // d = sqrt(d1^2 - x^2)
    return std::sqrt(d1s - std::pow(x, 2));
}

shared_ptr<PaintCommand> ScreenShoter::getCursorPos(const QPoint& pos)
{
    for(auto& command: undo_stack_.commands()) {
        switch (command->graph()) {
        case RECTANGLE:
        {
            Resizer resizer(command->points()[0], command->points()[1]);
            cursor_graph_pos_ = resizer.absolutePos(pos);

            if(cursor_graph_pos_ & Resizer::ADJUST_AREA) {
                return command;
            }
            break;
        }

        case CIRCLE:
        {
            Resizer resizer(command->points()[0], command->points()[1]);

            QRegion r1(resizer.rect().adjusted(-2, -2, 2, 2), QRegion::Ellipse);
            QRegion r2(resizer.rect().adjusted(2, 2, -2, -2), QRegion::Ellipse);

            if(r1.contains(pos) && !r2.contains(pos)) {
                cursor_graph_pos_ = Resizer::BORDER;
                return command;
            }
            else {
                cursor_graph_pos_ = resizer.absolutePos(pos);
                if(cursor_graph_pos_ & Resizer::ANCHOR)
                    return command;
            }
            break;
        }

        case ARROW:
        case LINE:
        {
            auto d = distance(command->points()[0], command->points()[1], pos);
            auto area = QRect(command->points()[0], command->points()[1]).adjusted(-2, -2, 2, 2);

            auto x1_anchor = QRect(command->points()[0], command->points()[0]).adjusted(-3, -3, 2, 2);
            auto x2_anchor = QRect(command->points()[1], command->points()[1]).adjusted(-3, -3, 2, 2);

            if(x1_anchor.contains(pos)) {
                cursor_graph_pos_ = Resizer::X1Y1_ANCHOR;
                return command;
            }
            else if(x2_anchor.contains(pos)) {
                cursor_graph_pos_ = Resizer::X2Y2_ANCHOR;
                return command;
            }
            else if(std::fabs(d) < 4 && area.contains(pos)) {
                cursor_graph_pos_ = Resizer::BORDER;
                return command;
            }
            break;
        }

        case TEXT:
        {
            Resizer resizer(command->widget()->geometry().adjusted(-5, -5, 5, 5));
            resizer.enableRotate(true);
            cursor_graph_pos_ = resizer.absolutePos(pos);
            switch (cursor_graph_pos_) {
            case Resizer::INSIDE:
            case Resizer::X1_ANCHOR:
            case Resizer::Y2_ANCHOR:
            case Resizer::Y1_ANCHOR:
            case Resizer::X2_ANCHOR: cursor_graph_pos_ = Resizer::BORDER; break;
            default: break;
            }

            if(cursor_graph_pos_ & Resizer::ADJUST_AREA || cursor_graph_pos_ == Resizer::ROTATE_ANCHOR) {
                return command;
            }
            break;
        }

        default: break;
        }
    }
    cursor_graph_pos_ = Resizer::DEFAULT;
    return shared_ptr<PaintCommand>(nullptr);
}

void ScreenShoter::setCursorByPos(Resizer::PointPosition pos, const QCursor & default_cursor)
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
    menu_->height() * 2 + area.bottomRight().y() > rect().height()
        ? menu_->move(area.topRight().x() - menu_->width() + 1, area.topRight().y() - 1) // topright
        : menu_->move(area.bottomRight().x() - menu_->width() + 1, area.bottomRight().y() + 3); // bottomright
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
    undo_stack_.clear();

    menu_->hide();
    text_edit_ = nullptr;
    focus_ = nullptr;

    Selector::exit();
}

QImage ScreenShoter::mosaic(const QImage& _image)
{
    auto image = _image.copy();

    for(auto h = 4; h < image.height(); h += 9) {
        for(auto w = 4; w < image.width(); w += 9) {

            //
            for(auto i = 0; i < 9 && h + i - 4 < image.height(); ++i) {
                for(auto j = 0; j < 9 && w + j - 4 < image.width(); ++j) {
                    image.setPixel(w + j - 4, h + i - 4, image.pixel(w, h));
                }
            }
        }
    }

    return image;
}

void ScreenShoter::paintOnCanvas()
{
    if(!modified_) return;

    painter_.begin(&canvas_);
    painter_.drawPixmap(0, 0, captured_screen_);

    if(status_ == LOCKED) {
        for(auto& command: undo_stack_.commands()) {
            command->execute(&painter_);
        }
    }

    painter_.end();
}

void ScreenShoter::paintEvent(QPaintEvent *event)
{
    paintOnCanvas();

    painter_.begin(this);
    painter_.drawPixmap(0, 0, canvas_);

    ////
    if(focus_ != nullptr) {
        painter_.save();
        switch (focus_->graph()) {
        case RECTANGLE:
        case ELLIPSE:
            drawSelector(&painter_, Resizer(focus_->points()[0], focus_->points()[1]));
            break;

        case LINE:
            painter_.setPen(QPen(Qt::black, 1, Qt::DashLine));
            painter_.setRenderHint(QPainter::Antialiasing, true);
            painter_.drawLine(focus_->points()[0], focus_->points()[1]);

            painter_.setPen(QPen(Qt::black, 1, Qt::SolidLine));
            painter_.drawRect(QRect(focus_->points()[0] - QPoint(3, 3), focus_->points()[0] + QPoint(2, 2)));
            painter_.drawRect(QRect(focus_->points()[1] - QPoint(3, 3), focus_->points()[1] + QPoint(2, 2)));
            break;

        case ARROW:
            painter_.setPen(QPen(Qt::black, 1, Qt::DashLine));
            painter_.setRenderHint(QPainter::Antialiasing, true);
            painter_.drawLine(focus_->points()[0], focus_->points()[1]);

            painter_.setPen(QPen(Qt::black, 1, Qt::SolidLine));
            painter_.drawRect(QRect(focus_->points()[0] - QPoint(2, 2), focus_->points()[0] + QPoint(2, 2)));
            painter_.drawRect(QRect(focus_->points()[1] - QPoint(2, 2), focus_->points()[1] + QPoint(2, 2)));
            break;

        case TEXT:
        {
            Resizer resizer(focus_->widget()->geometry().adjusted(-5, -5, 5, 5));

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

    history_.push_back({captured_screen_, selected(), undo_stack_});
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

    snip_done();

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
        undo_stack_ = history.commands_;
        redo_stack_.clear();
        modified();

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
}

///////////////////////////////////////////////////// UNDO / REDO ////////////////////////////////////////////////////////////
void ScreenShoter::undo()
{
    if(undo_stack_.empty()) return;

    auto temp = undo_stack_.back();
    if(focus_ == temp)
        focus_ = nullptr;

    redo_stack_.push(temp);
    undo_stack_.pop();
}

void ScreenShoter::redo()
{
    if(redo_stack_.empty()) return;

    undo_stack_.push(redo_stack_.back());
    redo_stack_.pop();
}
