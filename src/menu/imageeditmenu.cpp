#include "imageeditmenu.h"
#include <QPushButton>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QStyle>
#include <QHBoxLayout>
#include <QMoveEvent>
#include <QButtonGroup>
#include "iconbutton.h"

#define CONNECT_BTN_MENU(X, BTN, MENU)      connect(BTN, &QPushButton::toggled, [=](bool checked) {      \
                                                if(checked) {                                           \
                                                    emit graphChanged(X);                               \
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

ImageEditMenu::ImageEditMenu(QWidget* parent, uint32_t groups)
    : EditMenu(parent)
{
    group_ = new ButtonGroup(this);
    connect(group_, &ButtonGroup::uncheckedAll, [this]() { graphChanged(Graph::NONE); });

    const auto icon_size = QSize(HEIGHT, HEIGHT);

    if (groups & GRAPH_GROUP) {
        auto rectangle_btn = new QPushButton(this);
        rectangle_btn->setCheckable(true);
        rectangle_btn->setToolTip(tr("Rectangle"));
        rectangle_btn->setObjectName("rectangle_btn");
        rectangle_menu_ = new GraphMenu(this);
        connect(rectangle_menu_, &EditMenu::changed, [this]() { emit styleChanged(Graph::RECTANGLE); });
        CONNECT_BTN_MENU(Graph::RECTANGLE, rectangle_btn, rectangle_menu_);
        group_->addButton(rectangle_btn);
        addWidget(rectangle_btn);
        buttons_[Graph::RECTANGLE] = rectangle_btn;

        auto circle_btn = new QPushButton(this);
        circle_btn->setCheckable(true);
        circle_btn->setToolTip(tr("Ellipse"));
        circle_btn->setObjectName("circle_btn");
        circle_menu_ = new GraphMenu(this);
        connect(circle_menu_, &EditMenu::changed, [this]() { emit styleChanged(Graph::CIRCLE); });
        CONNECT_BTN_MENU(Graph::CIRCLE, circle_btn, circle_menu_);
        group_->addButton(circle_btn);
        addWidget(circle_btn);
        buttons_[Graph::CIRCLE] = circle_btn;

        auto arrow_btn = new QPushButton(this);
        arrow_btn->setCheckable(true);
        arrow_btn->setToolTip(tr("Arrow"));
        arrow_btn->setObjectName("arrow_btn");
        arrow_menu_ = new ArrowEditMenu(this);
        connect(arrow_menu_, &EditMenu::changed, [this]() { emit styleChanged(Graph::ARROW); });
        CONNECT_BTN_MENU(Graph::ARROW, arrow_btn, arrow_menu_);
        group_->addButton(arrow_btn);
        addWidget(arrow_btn);
        buttons_[Graph::ARROW] = arrow_btn;

        auto line_btn = new QPushButton(this);
        line_btn->setCheckable(true);
        line_btn->setToolTip(tr("Line"));
        line_btn->setObjectName("line_btn");
        line_menu_ = new LineEditMenu(this);
        connect(line_menu_, &EditMenu::changed, [this]() { emit styleChanged(Graph::LINE); });
        CONNECT_BTN_MENU(Graph::LINE, line_btn, line_menu_);
        group_->addButton(line_btn);
        addWidget(line_btn);
        buttons_[Graph::LINE] = line_btn;

        auto pen_btn = new QPushButton(this);
        pen_btn->setCheckable(true);
        pen_btn->setToolTip(tr("Pencil"));
        pen_btn->setObjectName("pen_btn");
        curves_menu_ = new LineEditMenu(this);
        connect(curves_menu_, &EditMenu::changed, [this]() { emit styleChanged(Graph::CURVES); });
        CONNECT_BTN_MENU(Graph::CURVES, pen_btn, curves_menu_);
        group_->addButton(pen_btn);
        addWidget(pen_btn);
        buttons_[Graph::CURVES] = pen_btn;

        auto text_btn = new QPushButton(this);
        text_btn->setCheckable(true);
        text_btn->setToolTip(tr("Text"));
        text_btn->setObjectName("text_btn");
        text_menu_ = new FontMenu(this);
        connect(text_menu_, &EditMenu::changed, [this]() { emit styleChanged(Graph::TEXT); });
        CONNECT_BTN_MENU(Graph::TEXT, text_btn, text_menu_);
        group_->addButton(text_btn);
        addWidget(text_btn);
        buttons_[Graph::TEXT] = text_btn;

        auto mosaic_btn = new QPushButton(this);
        mosaic_btn->setCheckable(true);
        mosaic_btn->setToolTip(tr("Mosaic"));
        mosaic_btn->setObjectName("mosaic_btn");
        mosaic_menu_ = new EraseMenu(this);
        connect(mosaic_menu_, &EditMenu::changed, [this]() { emit styleChanged(Graph::MOSAIC); });
        CONNECT_BTN_MENU(Graph::MOSAIC, mosaic_btn, mosaic_menu_);
        group_->addButton(mosaic_btn);
        addWidget(mosaic_btn);
        buttons_[Graph::MOSAIC] = mosaic_btn;

        auto eraser_btn = new QPushButton(this);
        eraser_btn->setCheckable(true);
        eraser_btn->setToolTip(tr("Eraser"));
        eraser_btn->setObjectName("eraser_btn");
        erase_menu_ = new EraseMenu(this);
        connect(erase_menu_, &EditMenu::changed, [this]() { emit styleChanged(Graph::ERASER); });
        CONNECT_BTN_MENU(Graph::ERASER, eraser_btn, erase_menu_);
        group_->addButton(eraser_btn);
        addWidget(eraser_btn);
        buttons_[Graph::ERASER] = eraser_btn;
    }

    /////////////////////////////////////////////////////////////////////////////////
    if (groups & REDO_UNDO_GROUP) {
        addSeparator();

        undo_btn_ = new QPushButton(this);
        undo_btn_->setDisabled(true);
        undo_btn_->setObjectName("undo_btn");
        connect(undo_btn_, &IconButton::clicked, this, &ImageEditMenu::undo);
        addWidget(undo_btn_);

        redo_btn_ = new QPushButton(this);
        redo_btn_->setDisabled(true);
        redo_btn_->setObjectName("redo_btn");
        connect(redo_btn_, &CustomButton::clicked, this, &ImageEditMenu::redo);
        addWidget(redo_btn_);
    }

    /////////////////////////////////////////////////////////////////////////////////
    if (groups & SAVE_GROUP) {
        addSeparator();

        auto fix_btn = new QPushButton(this);
        fix_btn->setObjectName("fix_btn");
        connect(fix_btn, &QPushButton::clicked, [this]() { group_->uncheckAll(); fix(); hide(); });
        addWidget(fix_btn);

        auto save_btn = new QPushButton(this);
        save_btn->setObjectName("save_btn");
        connect(save_btn, &QPushButton::clicked, this, &ImageEditMenu::save);
        connect(save_btn, &QPushButton::clicked, [=]() { group_->uncheckAll(); });
        addWidget(save_btn);
    }

    /////////////////////////////////////////////////////////////////////////////////
    if (groups & EXIT_GROUP) {
        addSeparator();

        auto close_btn = new QPushButton(this);
        close_btn->setObjectName("close_btn");
        connect(close_btn, &QPushButton::clicked, [this]() { group_->uncheckAll(); exit(); hide(); });
        addWidget(close_btn);

        auto ok_btn = new QPushButton(this);
        ok_btn->setObjectName("ok_btn");
        connect(ok_btn, &QPushButton::clicked, [this]() { group_->uncheckAll(); ok(); hide(); });
        addWidget(ok_btn);
    }
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
    if (pen.width() < 1) pen.setWidth(1);

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
    emit styleChanged(graph);
}

void ImageEditMenu::style(Graph graph, QPen pen, bool fill)
{
    if (pen.width() < 1) pen.setWidth(1);

    switch (graph) {
    case Graph::RECTANGLE: rectangle_menu_->style(pen, fill);   break;
    case Graph::CIRCLE:    circle_menu_->style(pen, fill);      break;
    case Graph::LINE:      line_menu_->pen(pen);                break;
    case Graph::CURVES:    curves_menu_->pen(pen);              break;
    case Graph::MOSAIC:    mosaic_menu_->pen(pen);              break;
    case Graph::ARROW:     arrow_menu_->pen(pen);               break;
    case Graph::TEXT:      text_menu_->pen(pen);                break;
    case Graph::ERASER:    erase_menu_->pen(pen);               break;
    default:        break;
    }
    emit styleChanged(graph);
}

bool ImageEditMenu::fill(Graph graph)
{
    switch (graph) {
    case Graph::RECTANGLE: return rectangle_menu_->fill();
    case Graph::CIRCLE:    return circle_menu_->fill();
    case Graph::ARROW:     return true;

    case Graph::LINE:
    case Graph::CURVES:
    case Graph::MOSAIC:
    case Graph::TEXT:
    case Graph::ERASER:
    default:               return false;
    }
}

void ImageEditMenu::fill(Graph graph, bool fill)
{
    switch (graph) {
    case Graph::RECTANGLE: rectangle_menu_->fill(fill); break;
    case Graph::CIRCLE:    circle_menu_->fill(fill);    break;
    default:        break;
    }

    emit styleChanged(graph);
}

QFont ImageEditMenu::font(Graph graph)
{
    switch (graph) {
    case Graph::TEXT:       return text_menu_->font();
    default:                return QFont();
    }
}

void ImageEditMenu::reset()
{
    group_->uncheckAll();
}
