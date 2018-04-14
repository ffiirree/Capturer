#include "screencapturer.h"
#include <QApplication>
#include <QScreen>
#include <QDesktopWidget>
#include <QFileDialog>
#include <QClipboard>
#include <QMouseEvent>
//#include <QDebug>

ScreenCapturer::ScreenCapturer(QWidget *parent)
    : Selector(parent)
{
    // menu
    menu_ = new MainMenu(this);
    menu_->hide();

    connect(menu_, &MainMenu::SAVE_IMAGE, this, &ScreenCapturer::save_image);
    connect(menu_, &MainMenu::COPY_TO_CLIPBOARD, this, &ScreenCapturer::copy2clipboard);
    connect(menu_, &MainMenu::FIX_IMAGE, this, &ScreenCapturer::fix_image);
    connect(menu_, &MainMenu::EXIT_CAPTURE, this, &ScreenCapturer::exit_capture);


    connect(menu_, &MainMenu::START_PAINT_RECTANGLE, [=]() { status_ = LOCKED;  edit_status_ = START_PAINTING_RECTANGLE; gmenu_->show(); });
    connect(menu_, &MainMenu::END_PAINT_RECTANGLE, [=]() { edit_status_ = NONE; gmenu_->hide(); });

    connect(menu_, &MainMenu::START_PAINT_CIRCLE, [=]() { status_ = LOCKED;  edit_status_ = START_PAINTING_CIRCLE; gmenu_->show(); });
    connect(menu_, &MainMenu::END_PAINT_CIRCLE, [=]() { edit_status_ = NONE; gmenu_->hide(); });

    connect(menu_, &MainMenu::START_PAINT_ARROW, [=]() { status_ = LOCKED;  edit_status_ = START_PAINTING_ARROW; gmenu_->show(); });
    connect(menu_, &MainMenu::END_PAINT_ARROW, [=]() { edit_status_ = NONE; gmenu_->hide(); });

    connect(menu_, &MainMenu::START_PAINT_LINE, [=]() { status_ = LOCKED;  edit_status_ = START_PAINTING_LINE; gmenu_->show(); });
    connect(menu_, &MainMenu::END_PAINT_LINE, [=]() { edit_status_ = NONE; gmenu_->hide(); });

    connect(menu_, &MainMenu::START_PAINT_CURVES, [=]() { status_ = LOCKED;  edit_status_ = START_PAINTING_CURVES; gmenu_->show(); });
    connect(menu_, &MainMenu::END_PAINT_CURVES, [=]() { edit_status_ = NONE; gmenu_->hide(); });

    connect(menu_, &MainMenu::START_PAINT_TEXT, [=]() { status_ = LOCKED;  edit_status_ = START_PAINTING_TEXT; gmenu_->show(); });
    connect(menu_, &MainMenu::END_PAINT_TEXT, [=]() { edit_status_ = NONE; text_edit_->hide(); gmenu_->hide(); });

    // graph menu
    gmenu_ = new GraphMenu(this);
    gmenu_->hide();

    connect(gmenu_, &GraphMenu::SET_WIDTH_01, [=](){ pen_width_ = 1; fill_ = false; });
    connect(gmenu_, &GraphMenu::SET_WIDTH_02, [=](){ pen_width_ = 2; fill_ = false; });
    connect(gmenu_, &GraphMenu::SET_WIDTH_03, [=](){ pen_width_ = 4; fill_ = false; });

    connect(gmenu_, &GraphMenu::SET_FILL, [=](){ fill_ = true; });
    connect(gmenu_, &GraphMenu::SET_UNFILL, [=](){ fill_ = false; });

    connect(gmenu_, &GraphMenu::SET_COLOR, [=](const QColor& color) { color_ = color; });

    // move menu
    auto menu_move_factor = [&]() {
        captured_screen_.height() - rb().y() <= menu_->height()
            ? menu_->move(rb().x() - menu_->width() + 1, rb().y() - menu_->height() - 1)
            : menu_->move(rb().x() - menu_->width() + 1, rb().y() + 3);

        gmenu_->move(rb().x() - menu_->width() + 1, rb().y() + menu_->height() + 5);
    };

    connect(this, &ScreenCapturer::moved, menu_move_factor);
    connect(this, &ScreenCapturer::resized, menu_move_factor);

    text_edit_ = new QTextEdit(this);
    text_edit_->hide();
    text_edit_->setObjectName("text_area");
}

void ScreenCapturer::start()
{
    captured_screen_ = QGuiApplication::primaryScreen()->grabWindow(QApplication::desktop()->winId());

    Selector::start();

    menu_->hide();
    gmenu_->hide();
}

void ScreenCapturer::mousePressEvent(QMouseEvent *event)
{
    if(status_ == LOCKED) {
        switch (edit_status_) {
        case START_PAINTING_RECTANGLE:
        case END_PAINTING_RECTANGLE: rectangle_end_ = rectangle_begin_ = event->pos(); edit_status_ = PAINTING_RECTANGLE; break;
        case START_PAINTING_CIRCLE:
        case END_PAINTING_CIRCLE: circle_end_ = circle_begin_ = event->pos(); edit_status_ = PAINTING_CIRCLE; break;
        case START_PAINTING_ARROW:
        case END_PAINTING_ARROW: arrow_end_ = arrow_begin_ = event->pos(); edit_status_ = PAINTING_ARROW; break;
        case START_PAINTING_LINE:
        case END_PAINTING_LINE: line_end_ = line_begin_ = event->pos(); edit_status_ = PAINTING_LINE; break;
        case START_PAINTING_CURVES:
        case END_PAINTING_CURVES: curves_end_ = curves_begin_ = event->pos(); edit_status_ = PAINTING_CURVES; break;
        case START_PAINTING_TEXT:
        case END_PAINTING_TEXT:
            edit_status_ = PAINTING_TEXT;
            text_pos_ = event->pos();
            text_edit_->setAttribute(Qt::WA_TranslucentBackground, true);
            text_edit_->setPlainText("");
            text_edit_->show();
            text_edit_->move(text_pos_);
            break;
        default:
            break;
        }
    }

    Selector::mousePressEvent(event);
}

void ScreenCapturer::mouseMoveEvent(QMouseEvent* event)
{
    if(status_ > CAPTURED) {
        menu_->show();

        captured_screen_.height() - rb().y() <= menu_->height()
            ? menu_->move(rb().x() - menu_->width() + 1, rb().y() - menu_->height() - 1)
            : menu_->move(rb().x() - menu_->width() + 1, rb().y() + 3);

        gmenu_->move(rb().x() - menu_->width() + 1, rb().y() + menu_->height() + 5);
    }

    if(status_ == LOCKED) {
        switch (edit_status_) {
        case PAINTING_RECTANGLE: rectangle_end_ = event->pos(); setCursor(Qt::CrossCursor); break;
        case PAINTING_CIRCLE: circle_end_ = event->pos(); setCursor(Qt::CrossCursor); break;
        case PAINTING_ARROW: arrow_end_ = event->pos(); setCursor(Qt::CrossCursor); break;
        case PAINTING_LINE: line_end_ = event->pos(); setCursor(Qt::CrossCursor); break;
        case PAINTING_CURVES: curves_end_ = event->pos(); setCursor(Qt::CrossCursor); break;
        case PAINTING_TEXT: setCursor(Qt::IBeamCursor); break;
        default:break;
        }
    }

    this->update();
    Selector::mouseMoveEvent(event);
}

void ScreenCapturer::mouseReleaseEvent(QMouseEvent *event)
{
    if(status_ == LOCKED) {
        switch (edit_status_) {
        case PAINTING_RECTANGLE: edit_status_ = END_PAINTING_RECTANGLE; break;
        case PAINTING_CIRCLE: edit_status_ = END_PAINTING_CIRCLE; break;
        case PAINTING_ARROW: edit_status_ = END_PAINTING_ARROW; break;
        case PAINTING_LINE: line_end_ = event->pos(); edit_status_ = END_PAINTING_LINE; break;
        case PAINTING_CURVES: curves_end_ = event->pos(); edit_status_ = END_PAINTING_CURVES; break;
        default: break;
        }
    }

    Selector::mouseReleaseEvent(event);
}

void ScreenCapturer::keyPressEvent(QKeyEvent *event)
{
    if((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && status_ == CAPTURED) {
        copy2clipboard();
        exit_capture();
    }

    Selector::keyPressEvent(event);
}

void ScreenCapturer::paintEvent(QPaintEvent *event)
{
    QRect roi = selected();
    captured_image_ = captured_screen_.copy(roi);

    painter_.begin(this);
    painter_.setRenderHint(QPainter::Antialiasing, true);
    painter_.drawPixmap(0, 0, captured_screen_);
    painter_.fillRect(captured_screen_.rect(), QColor(0, 0, 0, 100));
    painter_.drawPixmap(roi.topLeft(), captured_image_);

    painter_.setPen(QPen(QColor(color_.rgb()), 1, Qt::DashDotLine, Qt::FlatCap));

    if(status_ == LOCKED) {
        QPainter edit_painter;
        edit_painter.begin(&captured_screen_);
        edit_painter.setRenderHint(QPainter::Antialiasing, true);
        edit_painter.setPen(QPen(color_, pen_width_, Qt::SolidLine, Qt::FlatCap));
        QPoint points[6];
        QTextOption options;
        options.setFlags(QTextOption::ShowTabsAndSpaces);
        if(fill_) edit_painter.setBrush(color_);

        switch(edit_status_) {
        case PAINTING_RECTANGLE: painter_.drawRect(QRect(rectangle_begin_, rectangle_end_)); break;
        case END_PAINTING_RECTANGLE: edit_painter.drawRect(QRect(rectangle_begin_, rectangle_end_)); edit_status_ = START_PAINTING_RECTANGLE; break;

        case PAINTING_CIRCLE: painter_.drawEllipse(QRect(circle_begin_, circle_end_)); break;
        case END_PAINTING_CIRCLE: edit_painter.drawEllipse(QRect(circle_begin_, circle_end_)); edit_status_ = START_PAINTING_CIRCLE; break;

        case PAINTING_ARROW: getArrowPoints(arrow_begin_, arrow_end_, points); painter_.drawPolygon(points, 6); break;
        case END_PAINTING_ARROW:
            getArrowPoints(arrow_begin_, arrow_end_, points);
            edit_painter.setBrush(color_);
            edit_painter.drawPolygon(points, 6);
            edit_status_ = START_PAINTING_ARROW;
            break;

        case PAINTING_LINE: painter_.drawLine(line_begin_, line_end_); break;
        case END_PAINTING_LINE: edit_painter.drawLine(line_begin_, line_end_); edit_status_ = START_PAINTING_LINE; break;

        case PAINTING_CURVES:
        case END_PAINTING_CURVES: edit_painter.drawLine(curves_begin_, curves_end_); curves_begin_ = curves_end_; break;

        case PAINTING_TEXT: edit_painter.drawText(text_pos_, text_edit_->toPlainText()); break;
        case END_PAINTING_TEXT:

        default: break;
        }
        edit_painter.end();
    }
    this->painter_.end();

    Selector::paintEvent(event);
}

void ScreenCapturer::getArrowPoints(QPoint begin, QPoint end, QPoint* points)
{
    double par = 10.0;
    double par2 = 27.0;
    double slopy = atan2((end.y() - begin.y()), (end.x() - begin.x()));
    double alpha = 30 * 3.14 / 180;

    points[1] = QPoint(end.x() - int(par * cos(alpha + slopy)) - int(9 * cos(slopy)), end.y() - int(par*sin(alpha + slopy)) - int(9 * sin(slopy)));
    points[5] = QPoint(end.x() - int(par * cos(alpha - slopy)) - int(9 * cos(slopy)), end.y() + int(par*sin(alpha - slopy)) - int(9 * sin(slopy)));

    points[2] = QPoint(end.x() - int(par2 * cos(alpha + slopy)), end.y() - int(par2*sin(alpha + slopy)));
    points[4] = QPoint(end.x() - int(par2 * cos(alpha - slopy)), end.y() + int(par2*sin(alpha - slopy)));

    points[0] = begin;
    points[3] = end;
}


void ScreenCapturer::save_image()
{
    auto filename = QFileDialog::getSaveFileName(this, tr("Save Image"), "/", tr("Image Files (*.png *.jpeg *.jpg *.bmp)"));

    if(!filename.isEmpty()) {
        captured_image_.save(filename);
        exit_capture();
    }
}

void ScreenCapturer::copy2clipboard()
{
    QApplication::clipboard()->setPixmap(captured_image_);
    exit_capture();
}

void ScreenCapturer::fix_image()
{
    FIX_IMAGE(captured_image_);
    exit_capture();
}

void ScreenCapturer::exit_capture()
{
    if(!captured_image_.isNull() && status_ == CAPTURED) {
        CAPTURE_SCREEN_DONE(captured_image_);
    }

    status_ =  INITIAL;
    end_ = begin_;

    menu_->hide();
    gmenu_->hide();
    this->hide();
}
