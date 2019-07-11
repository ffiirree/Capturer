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
                                            })

ImageEditMenu::ImageEditMenu(QWidget* parent)
    : EditMenu(parent)
{
    group_ = new ButtonGroup(this);
    connect(group_, &ButtonGroup::uncheckedAll, [this](){ paint(NONE); });

    const auto icon_size = QSize(HEIGHT, HEIGHT);

    auto rectangle_btn = new IconButton(QPixmap(":/icon/res/rectangle"), icon_size, { ICON_W, 22 }, true);
    rectangle_btn->setToolTip(tr("Rectangle"));
    rectangle_menu_ = new GraphMenu(this);
    connect(rectangle_menu_, &EditMenu::changed, [this](){ emit changed(RECTANGLE); });
    CONNECT_BTN_MENU(RECTANGLE, rectangle_btn, rectangle_menu_);
    group_->addButton(rectangle_btn);
    addButton(rectangle_btn);

    auto circle_btn = new IconButton(QPixmap(":/icon/res/circle"), icon_size, { ICON_W, ICON_W }, true);
    circle_btn->setToolTip(tr("Ellipse"));
    circle_menu_ = new GraphMenu(this);
    connect(circle_menu_, &EditMenu::changed, [this](){ emit changed(CIRCLE); });
    CONNECT_BTN_MENU(CIRCLE, circle_btn, circle_menu_);
    group_->addButton(circle_btn);
    addButton(circle_btn);

    auto arrow_btn = new IconButton(QPixmap(":/icon/res/arrow"), icon_size, { ICON_W, ICON_W }, true);
    arrow_btn->setToolTip(tr("Arrow"));
    arrow_menu_ = new ArrowEditMenu(this);
    connect(arrow_menu_, &EditMenu::changed, [this](){ emit changed(ARROW); });
    CONNECT_BTN_MENU(ARROW, arrow_btn, arrow_menu_);
    group_->addButton(arrow_btn);
    addButton(arrow_btn);

    auto line_btn = new IconButton(QPixmap(":/icon/res/line"), icon_size, { ICON_W, ICON_W }, true);
    line_btn->setToolTip(tr("Line"));
    line_menu_ = new LineEditMenu(this);
    connect(line_menu_, &EditMenu::changed, [this](){ emit changed(LINE); });
    CONNECT_BTN_MENU(LINE, line_btn, line_menu_);
    group_->addButton(line_btn);
    addButton(line_btn);

    auto pen_btn = new IconButton(QPixmap(":/icon/res/feather"), icon_size, { ICON_W, ICON_W }, true);
    pen_btn->setToolTip(tr("Pencil"));
    curves_menu_ = new LineEditMenu(this);
    connect(curves_menu_, &EditMenu::changed, [this](){ emit changed(CURVES); });
    CONNECT_BTN_MENU(CURVES, pen_btn, curves_menu_);
    group_->addButton(pen_btn);
    addButton(pen_btn);

    auto text_btn = new IconButton(QPixmap(":/icon/res/text"), icon_size, { ICON_W, ICON_W }, true);
    text_btn->setToolTip(tr("Text"));
    text_menu_ = new FontMenu(this);
    connect(text_menu_, &EditMenu::changed, [this](){ emit changed(TEXT); });
    CONNECT_BTN_MENU(TEXT, text_btn, text_menu_);
    group_->addButton(text_btn);
    addButton(text_btn);

    auto mosaic_btn = new IconButton(QPixmap(":/icon/res/mosaic"), icon_size, { ICON_W, ICON_W }, true);
    mosaic_btn->setToolTip(tr("Mosaic"));
    mosaic_menu_ = new EraseMenu(this);
    connect(mosaic_menu_, &EditMenu::changed, [this](){ emit changed(MOSAIC); });
    CONNECT_BTN_MENU(MOSAIC, mosaic_btn, mosaic_menu_);
    group_->addButton(mosaic_btn);
    addButton(mosaic_btn);

    auto eraser_btn = new IconButton(QPixmap(":/icon/res/eraser"), icon_size, { ICON_W, ICON_W }, true);
    eraser_btn->setToolTip(tr("Eraser"));
    erase_menu_ = new EraseMenu(this);
    connect(erase_menu_, &EditMenu::changed, [this](){ emit changed(ERASER); });
    CONNECT_BTN_MENU(ERASER, eraser_btn, erase_menu_);
    group_->addButton(eraser_btn);
    addButton(eraser_btn);

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
    connect(fix_btn, &IconButton::clicked, this, &ImageEditMenu::fix);
    connect(fix_btn, &IconButton::clicked, [=]() { group_->uncheckAll(); });
    addButton(fix_btn);

    auto save_btn = new IconButton(QPixmap(":/icon/res/save"), icon_size, { ICON_W, ICON_W });
    connect(save_btn, &IconButton::clicked, this, &ImageEditMenu::save);
    connect(save_btn, &IconButton::clicked, [=]() { group_->uncheckAll(); });
    addButton(save_btn);

    auto close_btn = new IconButton(QPixmap(":/icon/res/wrong"), icon_size, { ICON_W, ICON_W });
    connect(close_btn, &IconButton::clicked, this, &ImageEditMenu::exit);
    connect(close_btn, &IconButton::clicked, [=]() { group_->uncheckAll(); });
    close_btn->setIconColor(QColor("#ff0000"));
    close_btn->setIconHoverColor(QColor("#ff0000"));
    close_btn->setBackgroundColor(QColor("#ffffff"));
    close_btn->setBackgroundHoverColor(QColor("#d0d0d5"));
    addWidget(close_btn);

    auto copy_btn = new IconButton(QPixmap(":/icon/res/right"), icon_size, { ICON_W, ICON_W });
    connect(copy_btn, &IconButton::clicked, this, &ImageEditMenu::copy);
    connect(copy_btn, &IconButton::clicked, [=]() { group_->uncheckAll(); });
    copy_btn->setIconColor(QColor("#00ff00"));
    copy_btn->setIconHoverColor(QColor("#00ff00"));
    copy_btn->setBackgroundColor(QColor("#ffffff"));
    copy_btn->setBackgroundHoverColor(QColor("#d0d0d5"));
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
