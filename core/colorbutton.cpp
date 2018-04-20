#include "colorbutton.h"

ColorButton::ColorButton(QWidget *parent)
    : ColorButton(Qt::blue, parent)
{}

ColorButton::ColorButton(const QColor& color, QWidget *parent)
    : QPushButton(parent), color_(color)
{
    color_dialog_ = new QColorDialog();
    color_dialog_->setOptions(QColorDialog::DontUseNativeDialog | QColorDialog::ShowAlphaChannel);
    color_dialog_->setWindowFlags(Qt::WindowStaysOnTopHint);
    color_dialog_->setWindowState(Qt::WindowActive);

    connect(this, &ColorButton::clicked, [=](){
        color_dialog_->show();
    });

    connect(color_dialog_, &QColorDialog::colorSelected, [&](const QColor& color){
        if(color.isValid()) {
            color_ = color;
            emit changed(color_);
        }
    });
}

ColorButton::~ColorButton()
{
    delete color_dialog_;
}

void ColorButton::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);
    painter_.begin(this);
    painter_.setBrush(QBrush(color_));
    painter_.setPen("#CECECE");
    painter_.drawRect(rect());
    painter_.end();
}
