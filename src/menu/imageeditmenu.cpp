#include "imageeditmenu.h"
#include <QPushButton>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QStyle>
#include <QHBoxLayout>
#include <QMoveEvent>
#include <QButtonGroup>
#include "iconbutton.h"

#define CONNECT_BTN_MENU(X, BTN, MENU)      connect(BTN, &IconButton::toggled, [=](bool checked) {      \
                                                if(checked) {                                           \
                                                    emit paint(X);                                      \
                                                    MENU->show();                                       \
                                                    MENU->move(pos().x(), pos().y() +                   \
                                                        (height() + 3) * (sub_menu_show_pos_ ? -1 : 1));\
                                                }                                                       \
                                                else {                                                  \
                                                    MENU->hide();                                       \
                                                }                                                       \
                                            });                                                         \
                                            connect(this, &EditMenu::moved, [this]() {                  \
                                                if(MENU->isVisible())                                   \
                                                    MENU->move(pos().x(), pos().y() +                   \
                                                        (height() + 3) * (sub_menu_show_pos_ ? -1 : 1));\
                                            })

ImageEditMenu::ImageEditMenu(QWidget* parent)
    : EditMenu(parent)
{
    group_ = new ButtonGroup(this);
    connect(group_, &ButtonGroup::uncheckedAll, [this](){ paint(Graph::NONE); });

    const auto icon_size = QSize(HEIGHT, HEIGHT);

    auto rectangle_btn = new IconButton(QPixmap(":/icon/res/rectangle"), icon_size, { ICON_W, ICON_W }, true);
    rectangle_btn->setToolTip(tr("Rectangle"));
    rectangle_menu_ = new GraphMenu(this);
    connect(rectangle_menu_, &EditMenu::changed, [this](){ emit changed(Graph::RECTANGLE); });
    CONNECT_BTN_MENU(Graph::RECTANGLE, rectangle_btn, rectangle_menu_);
    group_->addButton(rectangle_btn);
    addButton(rectangle_btn);
    buttons_[Graph::RECTANGLE] = rectangle_btn;

    auto circle_btn = new IconButton(QPixmap(":/icon/res/circle"), icon_size, { ICON_W, ICON_W }, true);
    circle_btn->setToolTip(tr("Ellipse"));
    circle_menu_ = new GraphMenu(this);
    connect(circle_menu_, &EditMenu::changed, [this](){ emit changed(Graph::CIRCLE); });
    CONNECT_BTN_MENU(Graph::CIRCLE, circle_btn, circle_menu_);
    group_->addButton(circle_btn);
    addButton(circle_btn);
    buttons_[Graph::CIRCLE] = circle_btn;

    auto arrow_btn = new IconButton(QPixmap(":/icon/res/arrow"), icon_size, { ICON_W, ICON_W }, true);
    arrow_btn->setToolTip(tr("Arrow"));
    arrow_menu_ = new ArrowEditMenu(this);
    connect(arrow_menu_, &EditMenu::changed, [this](){ emit changed(Graph::ARROW); });
    CONNECT_BTN_MENU(Graph::ARROW, arrow_btn, arrow_menu_);
    group_->addButton(arrow_btn);
    addButton(arrow_btn);
    buttons_[Graph::ARROW] = arrow_btn;

    auto line_btn = new IconButton(QPixmap(":/icon/res/line"), icon_size, { ICON_W, ICON_W }, true);
    line_btn->setToolTip(tr("Line"));
    line_menu_ = new LineEditMenu(this);
    connect(line_menu_, &EditMenu::changed, [this](){ emit changed(Graph::LINE); });
    CONNECT_BTN_MENU(Graph::LINE, line_btn, line_menu_);
    group_->addButton(line_btn);
    addButton(line_btn);
    buttons_[Graph::LINE] = line_btn;

    auto pen_btn = new IconButton(QPixmap(":/icon/res/feather"), icon_size, { ICON_W, ICON_W }, true);
    pen_btn->setToolTip(tr("Pencil"));
    curves_menu_ = new LineEditMenu(this);
    connect(curves_menu_, &EditMenu::changed, [this](){ emit changed(Graph::CURVES); });
    CONNECT_BTN_MENU(Graph::CURVES, pen_btn, curves_menu_);
    group_->addButton(pen_btn);
    addButton(pen_btn);
    buttons_[Graph::CURVES] = pen_btn;

    auto text_btn = new IconButton(QPixmap(":/icon/res/text"), icon_size, { ICON_W, ICON_W }, true);
    text_btn->setToolTip(tr("Text"));
    text_menu_ = new FontMenu(this);
    connect(text_menu_, &EditMenu::changed, [this](){ emit changed(Graph::TEXT); });
    CONNECT_BTN_MENU(Graph::TEXT, text_btn, text_menu_);
    group_->addButton(text_btn);
    addButton(text_btn);
    buttons_[Graph::TEXT] = text_btn;

    auto mosaic_btn = new IconButton(QPixmap(":/icon/res/mosaic"), icon_size, { ICON_W, ICON_W }, true);
    mosaic_btn->setToolTip(tr("Mosaic"));
    mosaic_menu_ = new EraseMenu(this);
    connect(mosaic_menu_, &EditMenu::changed, [this](){ emit changed(Graph::MOSAIC); });
    CONNECT_BTN_MENU(Graph::MOSAIC, mosaic_btn, mosaic_menu_);
    group_->addButton(mosaic_btn);
    addButton(mosaic_btn);
    buttons_[Graph::MOSAIC] = mosaic_btn;

    auto eraser_btn = new IconButton(QPixmap(":/icon/res/eraser"), icon_size, { ICON_W, ICON_W }, true);
    eraser_btn->setToolTip(tr("Eraser"));
    erase_menu_ = new EraseMenu(this);
    connect(erase_menu_, &EditMenu::changed, [this](){ emit changed(Graph::ERASER); });
    CONNECT_BTN_MENU(Graph::ERASER, eraser_btn, erase_menu_);
    group_->addButton(eraser_btn);
    addButton(eraser_btn);
    buttons_[Graph::ERASER] = eraser_btn;

    /////////////////////////////////////////////////////////////////////////////////
    addSeparator();

    undo_btn_ = new IconButton(QPixmap(":/icon/res/undo"), icon_size, { ICON_W, ICON_W });
    undo_btn_->setDisabled(true);
    connect(undo_btn_, &IconButton::clicked, this, &ImageEditMenu::undo);
    addButton(undo_btn_);

    redo_btn_ = new IconButton(QPixmap(":/icon/res/redo"), icon_size, { ICON_W, ICON_W });
    redo_btn_->setDisabled(true);
    connect(redo_btn_, &CustomButton::clicked, this, &ImageEditMenu::redo);
    addButton(redo_btn_);

    /////////////////////////////////////////////////////////////////////////////////
    addSeparator();

    auto fix_btn = new IconButton(QPixmap(":/icon/res/pin"), icon_size, { ICON_W, ICON_W });
    connect(fix_btn, &IconButton::clicked, [this]() { fix(); hide(); });
    connect(fix_btn, &IconButton::clicked, [=]() { group_->uncheckAll(); });
    addButton(fix_btn);

    auto save_btn = new IconButton(QPixmap(":/icon/res/save"), icon_size, { ICON_W, ICON_W });
    connect(save_btn, &IconButton::clicked, this, &ImageEditMenu::save);
    connect(save_btn, &IconButton::clicked, [=]() { group_->uncheckAll(); });
    addButton(save_btn);

    addSeparator();

    auto close_btn = new IconButton(QPixmap(":/icon/res/wrong"), icon_size, { ICON_W, ICON_W });
    connect(close_btn, &IconButton::clicked, [this]() { exit(); hide(); });
    connect(close_btn, &IconButton::clicked, [=]() { group_->uncheckAll(); });
    close_btn->setIconColor(QColor("#ee0000"));
    close_btn->setIconHoverColor(QColor("#ee0000"));
    close_btn->setBackgroundHoverColor(QColor("#d0d0d5"));
    addWidget(close_btn);

    auto ok_btn = new IconButton(QPixmap(":/icon/res/right"), icon_size, { ICON_W, ICON_W });
    connect(ok_btn, &IconButton::clicked, [this]() { ok(); hide(); });
    connect(ok_btn, &IconButton::clicked, [=]() { group_->uncheckAll(); });
    ok_btn->setIconColor(QColor("#07b913"));
    ok_btn->setIconHoverColor(QColor("#07b913"));
    ok_btn->setBackgroundHoverColor(QColor("#d0d0d5"));
    addWidget(ok_btn);
}

QPen ImageEditMenu::pen(Graph graph)
{
    switch (graph) {
    case Graph::RECTANGLE: return rectangle_menu_->pen();
    case Graph::CIRCLE:    return circle_menu_->pen();
    case Graph::LINE:      return line_menu_->pen();
    case Graph::CURVES:    return curves_menu_->pen();
    case Graph::MOSAIC:    return mosaic_menu_->pen();
    case Graph::ARROW:     return arrow_menu_->pen();
    case Graph::TEXT:      return text_menu_->pen();
    case Graph::ERASER:    return erase_menu_->pen();
    default:        break;
    }
    return QPen();
}

void ImageEditMenu::pen(Graph graph, QPen pen)
{
    if(pen.width() < 1) pen.setWidth(1);

    switch (graph) {
    case Graph::RECTANGLE: rectangle_menu_->pen(pen);    break;
    case Graph::CIRCLE:    circle_menu_->pen(pen);       break;
    case Graph::LINE:      line_menu_->pen(pen);         break;
    case Graph::CURVES:    curves_menu_->pen(pen);       break;
    case Graph::MOSAIC:    mosaic_menu_->pen(pen);       break;
    case Graph::ARROW:     arrow_menu_->pen(pen);        break;
    case Graph::TEXT:      text_menu_->pen(pen);         break;
    case Graph::ERASER:    erase_menu_->pen(pen);        break;
    default:        break;
    }
    emit changed(graph);
}

void ImageEditMenu::style(Graph graph, QPen pen, bool fill)
{
    if(pen.width() < 1) pen.setWidth(1);

    switch (graph) {
    case Graph::RECTANGLE: rectangle_menu_->style(pen, fill);   break;
    case Graph::CIRCLE:    circle_menu_->style(pen, fill);              break;
    case Graph::LINE:      line_menu_->pen(pen);                break;
    case Graph::CURVES:    curves_menu_->pen(pen);              break;
    case Graph::MOSAIC:    mosaic_menu_->pen(pen);              break;
    case Graph::ARROW:     arrow_menu_->pen(pen);               break;
    case Graph::TEXT:      text_menu_->pen(pen);                break;
    case Graph::ERASER:    erase_menu_->pen(pen);               break;
    default:        break;
    }
    emit changed(graph);
}

bool ImageEditMenu::fill(Graph graph)
{
    switch (graph) {
    case Graph::RECTANGLE: return rectangle_menu_->fill();
    case Graph::CIRCLE:    return circle_menu_->fill();
    case Graph::LINE:      return line_menu_->fill();
    case Graph::CURVES:    return curves_menu_->fill();
    case Graph::MOSAIC:    break;
    case Graph::ARROW:     return true;
    case Graph::TEXT:      return text_menu_->fill();
    case Graph::ERASER:    break;
    default:        break;
    }
    return false;
}

void ImageEditMenu::fill(Graph graph, bool fill)
{
    switch (graph) {
    case Graph::RECTANGLE: rectangle_menu_->fill(fill); break;
    case Graph::CIRCLE:    circle_menu_->fill(fill);    break;
    default:        break;
    }

    emit changed(graph);
}

QFont ImageEditMenu::font(Graph graph)
{
    switch (graph) {
    case Graph::TEXT:      return text_menu_->font();
    default:        break;
    }
    return QFont();
}

void ImageEditMenu::reset()
{
    group_->uncheckAll();
}
