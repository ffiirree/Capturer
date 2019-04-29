#include "imageeditmenu.h"
#include <QPushButton>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QStyle>
#include <QHBoxLayout>
#include <QMoveEvent>
#include <QButtonGroup>
#include "separator.h"
#include "iconbutton.h"

#define CONNECT_BTN_MENU(X, BTN, MENU)      connect(BTN, &IconButton::toggled, [=](bool checked) {      \
                                                if(checked) {                                           \
                                                    emit paint(X);                                      \
                                                    MENU->show();                                       \
                                                    MENU->move(pos().x(), pos().y() + height() + 3);    \
                                                }                                                       \
                                                else {                                                  \
                                                    MENU->hide();                                       \
                                                }                                                       \
                                            })

ImageEditMenu::ImageEditMenu(QWidget* parent)
    : EditMenu(parent)
{
    group_ = new ButtonGroup(this);
    connect(group_, &ButtonGroup::uncheckedAll, [this](){ paint(NONE); });

    auto rectangle_btn = new IconButton(QPixmap(":/icon/res/rectangle"), { HEIGHT, HEIGHT }, { ICON_W, 22 }, true);
    rectangle_btn->setToolTip(tr("绘制矩形"));
    rectangle_menu_ = new GraphMenu(this);
    connect(rectangle_menu_, &EditMenu::changed, [this](){ emit changed(RECTANGLE); });
    CONNECT_BTN_MENU(RECTANGLE, rectangle_btn, rectangle_menu_);
    group_->addButton(rectangle_btn);
    addWidget(rectangle_btn);

    auto circle_btn = new IconButton(QPixmap(":/icon/res/circle"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W }, true);
    circle_btn->setToolTip(tr("绘制椭圆"));
    circle_menu_ = new GraphMenu(this);
    connect(circle_menu_, &EditMenu::changed, [this](){ emit changed(CIRCLE); });
    CONNECT_BTN_MENU(CIRCLE, circle_btn, circle_menu_);
    group_->addButton(circle_btn);
    addWidget(circle_btn);

    auto arrow_btn = new IconButton(QPixmap(":/icon/res/arrow"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W }, true);
    arrow_btn->setToolTip(tr("绘制箭头"));
    arrow_menu_ = new ArrowEditMenu(this);
    connect(arrow_menu_, &EditMenu::changed, [this](){ emit changed(ARROW); });
    CONNECT_BTN_MENU(ARROW, arrow_btn, arrow_menu_);
    group_->addButton(arrow_btn);
    addWidget(arrow_btn);

    auto line_btn = new IconButton(QPixmap(":/icon/res/line"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W }, true);
    line_btn->setToolTip(tr("绘制直线"));
    line_menu_ = new LineEditMenu(this);
    connect(line_menu_, &EditMenu::changed, [this](){ emit changed(LINE); });
    CONNECT_BTN_MENU(LINE, line_btn, line_menu_);
    group_->addButton(line_btn);
    addWidget(line_btn);

    auto pen_btn = new IconButton(QPixmap(":/icon/res/feather"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W }, true);
    pen_btn->setToolTip(tr("画刷"));
    curves_menu_ = new LineEditMenu(this);
    connect(curves_menu_, &EditMenu::changed, [this](){ emit changed(CURVES); });
    CONNECT_BTN_MENU(CURVES, pen_btn, curves_menu_);
    group_->addButton(pen_btn);
    addWidget(pen_btn);

    auto text_btn = new IconButton(QPixmap(":/icon/res/text"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W }, true);
    text_btn->setToolTip(tr("绘制文本"));
    text_menu_ = new FontMenu(this);
    connect(text_menu_, &EditMenu::changed, [this](){ emit changed(TEXT); });
    CONNECT_BTN_MENU(TEXT, text_btn, text_menu_);
    group_->addButton(text_btn);
    addWidget(text_btn);

    auto mosaic_btn = new IconButton(QPixmap(":/icon/res/mosaic"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W }, true);
    mosaic_btn->setToolTip(tr("绘制马赛克"));
    mosaic_menu_ = new EraseMenu(this);
    connect(mosaic_menu_, &EditMenu::changed, [this](){ emit changed(MOSAIC); });
    CONNECT_BTN_MENU(MOSAIC, mosaic_btn, mosaic_menu_);
    group_->addButton(mosaic_btn);
    addWidget(mosaic_btn);

    auto eraser_btn = new IconButton(QPixmap(":/icon/res/eraser"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W }, true);
    eraser_btn->setToolTip(tr("橡皮擦"));
    erase_menu_ = new EraseMenu(this);
    connect(erase_menu_, &EditMenu::changed, [this](){ emit changed(ERASER); });
    CONNECT_BTN_MENU(ERASER, eraser_btn, erase_menu_);
    group_->addButton(eraser_btn);
    addWidget(eraser_btn);

    /////////////////////////////////////////////////////////////////////////////////
    addWidget(new Separator());

    undo_btn_ = new IconButton(QPixmap(":/icon/res/undo"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W });
    undo_btn_->setDisabled(true);
    connect(undo_btn_, &IconButton::clicked, this, &ImageEditMenu::undo);
    addWidget(undo_btn_);

    redo_btn_ = new IconButton(QPixmap(":/icon/res/redo"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W });
    redo_btn_->setDisabled(true);
    connect(redo_btn_, &CustomButton::clicked, this, &ImageEditMenu::redo);
    addWidget(redo_btn_);

    /////////////////////////////////////////////////////////////////////////////////
    addWidget(new Separator());

    auto fix_btn = new IconButton(QPixmap(":/icon/res/pin"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W });
    connect(fix_btn, &IconButton::clicked, this, &ImageEditMenu::fix);
    connect(fix_btn, &IconButton::clicked, [=]() { group_->uncheckAll(); });
    addWidget(fix_btn);

    auto save_btn = new IconButton(QPixmap(":/icon/res/save"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W });
    connect(save_btn, &IconButton::clicked, this, &ImageEditMenu::save);
    connect(save_btn, &IconButton::clicked, [=]() { group_->uncheckAll(); });
    addWidget(save_btn);

    auto close_btn = new IconButton(QPixmap(":/icon/res/wrong"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W });
    connect(close_btn, &IconButton::clicked, this, &ImageEditMenu::exit);
    connect(close_btn, &IconButton::clicked, [=]() { group_->uncheckAll(); });
    addWidget(close_btn);

    auto copy_btn = new IconButton(QPixmap(":/icon/res/right"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W });
    connect(copy_btn, &IconButton::clicked, this, &ImageEditMenu::copy);
    connect(copy_btn, &IconButton::clicked, [=]() { group_->uncheckAll(); });
    addWidget(copy_btn);
}

QPen ImageEditMenu::pen(Graph graph)
{
    switch (graph) {
    case RECTANGLE: return rectangle_menu_->pen();
    case CIRCLE:    return circle_menu_->pen();
    case LINE:      return line_menu_->pen();
    case CURVES:    return curves_menu_->pen();
    case MOSAIC:    return mosaic_menu_->pen();
    case ARROW:     return arrow_menu_->pen();
    case TEXT:      return text_menu_->pen();
    case ERASER:    return erase_menu_->pen();
    default:        break;
    }
    return QPen();
}

void ImageEditMenu::pen(Graph graph, int width)
{
    switch (graph) {
    case RECTANGLE: rectangle_menu_->pen(width);    break;
    case CIRCLE:    circle_menu_->pen(width);       break;
    case LINE:      line_menu_->pen(width);         break;
    case CURVES:    curves_menu_->pen(width);       break;
    case MOSAIC:    mosaic_menu_->pen(width);       break;
    case ARROW:     arrow_menu_->pen(width);        break;
    case TEXT:      text_menu_->pen(width);         break;
    case ERASER:    erase_menu_->pen(width);        break;
    default:        break;
    }
    emit changed(graph);
}

bool ImageEditMenu::fill(Graph graph)
{
    switch (graph) {
    case RECTANGLE: return rectangle_menu_->fill();
    case CIRCLE:    return circle_menu_->fill();
    case LINE:      return line_menu_->fill();
    case CURVES:    return curves_menu_->fill();
    case MOSAIC:    break;
    case ARROW:     return arrow_menu_->fill();
    case TEXT:      return text_menu_->fill();
    case ERASER:    break;
    default:        break;
    }
    return false;
}

QFont ImageEditMenu::font(Graph graph)
{
    switch (graph) {
    case TEXT:      return text_menu_->font();
    default:        break;
    }
    return QFont();
}

void ImageEditMenu::reset()
{
    group_->uncheckAll();
}
