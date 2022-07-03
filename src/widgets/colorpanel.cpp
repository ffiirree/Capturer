#include "colorpanel.h"
#include <QWheelEvent>
#include <QGridLayout>

ColorButton::ColorButton(QWidget *parent)
    : ColorButton(Qt::blue, parent)
{ }

ColorButton::ColorButton(const QColor& c, QWidget *parent)
    : QPushButton(parent), color_(c) {

    setAutoFillBackground(true);

    connect(this, &QPushButton::clicked, [this](){
        emit clicked(color_);
    });
}

void ColorButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    // background for alpha channel of color
    painter.setBrush(Qt::DiagCrossPattern);
    painter.drawRect(rect());

    // border & color
    painter.setPen(border_pen_);
    painter.setBrush(QBrush(color_));
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.drawRect(rect());

    painter.end();
}

void ColorButton::enterEvent(QEvent*)
{
    border_pen_.setColor(hover_color_);
}

void ColorButton::leaveEvent(QEvent *)
{
    border_pen_.setColor(default_color_);
}

///////////////////////////////////////////////////////////////////////////
ColorDialogButton::ColorDialogButton(QWidget *parent)
    : ColorButton(Qt::blue, parent)
{}

ColorDialogButton::ColorDialogButton(const QColor& color, QWidget *parent)
    : ColorButton(color, parent)
{
    color_dialog_ = new QColorDialog();
    color_dialog_->setOptions(QColorDialog::DontUseNativeDialog | QColorDialog::ShowAlphaChannel);
    color_dialog_->setWindowFlags(Qt::Dialog | Qt::WindowStaysOnTopHint);

    connect(this, &ColorDialogButton::clicked, [=](){
        color_dialog_->setCurrentColor(color_);
        color_dialog_->show();
    });

    connect(color_dialog_, &QColorDialog::colorSelected, [&](const QColor& color){
        if(color.isValid()) {
            color_ = color;
            emit changed(color_);
        }
    });
}

void ColorDialogButton::wheelEvent(QWheelEvent* event)
{
    color_.setAlpha(color_.alpha() + event->angleDelta().y() / 60);
    color(color_);
}

ColorDialogButton::~ColorDialogButton()
{
    delete color_dialog_;
}

///////////////////////////////////////////////////////////////////////////
ColorPanel::ColorPanel(QWidget * parent)
    : QWidget(parent)
{
    auto layout = new QGridLayout();
    layout->setContentsMargins(5, 0, 5, 0);
    layout->setVerticalSpacing(1);
    layout->setHorizontalSpacing(2);

    color_dialog_btn_ = new ColorDialogButton(Qt::red);
    color_dialog_btn_->setFixedSize(29, 29);
    layout->addWidget(color_dialog_btn_, 0, 0, 2, 2);

#define ADD_COLOR(COLOR, X, Y)   do {                                   \
                                    auto cbtn = new ColorButton(COLOR); \
                                    cbtn->setFixedSize(14, 14);         \
                                    layout->addWidget(cbtn, X, Y);       \
                                    connect(cbtn, &ColorButton::clicked, this, &ColorPanel::setColor);\
                                } while(0)
    ADD_COLOR(QColor(000, 000, 000), 0, 2);
    ADD_COLOR(QColor(128, 128, 128), 1, 2);
    ADD_COLOR(QColor(192, 192, 192), 0, 3);
    ADD_COLOR(QColor(255, 255, 255), 1, 3);
    ADD_COLOR(QColor(255, 000, 000), 0, 4);
    ADD_COLOR(QColor(255, 255, 000), 1, 4);
    ADD_COLOR(QColor(128, 000, 000), 0, 5);
    ADD_COLOR(QColor(128, 128, 000), 1, 5);
    ADD_COLOR(QColor(000, 255, 000), 0, 6);
    ADD_COLOR(QColor(000, 128, 000), 1, 6);
    ADD_COLOR(QColor(000, 255, 255), 0, 7);
    ADD_COLOR(QColor(000, 128, 128), 1, 7);
    ADD_COLOR(QColor(000, 000, 255), 0, 8);
    ADD_COLOR(QColor(000, 000, 128), 1, 8);
    ADD_COLOR(QColor(255, 000, 255), 0, 9);
    ADD_COLOR(QColor(128, 255, 128), 1, 9);
#undef ADD_COLOR

    setLayout(layout);

    connect(color_dialog_btn_, &ColorDialogButton::changed, this, &ColorPanel::changed);
}
