#include "colorpanel.h"

#include <QColorDialog>
#include <QGridLayout>
#include <QPainter>
#include <QStyleOptionFrame>
#include <QWheelEvent>

ColorButton::ColorButton(QWidget *parent)
    : ColorButton(Qt::blue, parent)
{}

ColorButton::ColorButton(const QColor& c, QWidget *parent)
    : QPushButton(parent), color_(c)
{
    connect(this, &QPushButton::clicked, [this]() { emit clicked(color_); });
}

void ColorButton::setColor(const QColor& c, bool silence)
{
    if (!c.isValid() || color_ == c) return;

    color_ = c;
    update();

    if (!silence) emit changed(color_);
}

void ColorButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    painter.setRenderHint(QPainter::Antialiasing, true);

    // color & background
    QStyleOptionButton b{};
    b.initFrom(this);
    const auto r2 = style()->subElementRect(QStyle::SE_PushButtonContents, &b, this);

    painter.fillRect(r2, Qt::DiagCrossPattern);
    painter.fillRect(r2, QBrush(color_));

    // border
    QStyleOptionFrame opt{};
    opt.initFrom(this);
    style()->drawPrimitive(QStyle::PE_Frame, &opt, &painter, this);
}

///////////////////////////////////////////////////////////////////////////
ColorDialogButton::ColorDialogButton(QWidget *parent)
    : ColorButton(Qt::blue, parent)
{}

ColorDialogButton::ColorDialogButton(const QColor& color, QWidget *parent)
    : ColorButton(color, parent)
{
    // FIXME(Linux): can't stay on top
    connect(this, &ColorDialogButton::clicked, [this]() {
        constexpr auto options = QColorDialog::DontUseNativeDialog | QColorDialog::ShowAlphaChannel;
        setColor(QColorDialog::getColor(color_, this, tr("Select Color"), options), false);
    });
}

void ColorDialogButton::wheelEvent(QWheelEvent *event)
{
    QColor tmp(color_);
    tmp.setAlpha(std::clamp<int>(color_.alpha() + event->angleDelta().y() / 60, 0, 255));
    setColor(tmp, false);
}

///////////////////////////////////////////////////////////////////////////
ColorPanel::ColorPanel(QWidget *parent)
    : QWidget(parent)
{
    auto layout = new QGridLayout();
    layout->setContentsMargins({ 2, 0, 5, 0 });
    layout->setSpacing(0);

    color_dialog_btn_ = new ColorDialogButton(Qt::red);
    color_dialog_btn_->setObjectName("selected-color");
    layout->addWidget(color_dialog_btn_, 0, 0, 2, 2);

#define ADD_COLOR(COLOR, X, Y)                                                                             \
    do {                                                                                                   \
        auto cbtn = new ColorButton(COLOR);                                                                \
        layout->addWidget(cbtn, X, Y);                                                                     \
        connect(cbtn, &ColorButton::clicked, [this](auto c) { setColor(c, false); });                      \
    } while (0)

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
