#include "imageeditmenu.h"
#include <QPixmap>
#include <QStyle>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QMoveEvent>
#include "logging.h"

ImageEditMenu::ImageEditMenu(QWidget* parent, uint32_t groups)
    : EditMenu(parent)
{
    group_ = new ButtonGroup(this);
    connect(group_, &ButtonGroup::uncheckedAll, [this]() { graph_ = Graph::NONE; graphChanged(Graph::NONE); });

    if (groups & GRAPH_GROUP) {
        btn_menus_[Graph::RECTANGLE].first = new QCheckBox(this);
        btn_menus_[Graph::RECTANGLE].first->setObjectName("rect-btn");
        btn_menus_[Graph::RECTANGLE].first->setToolTip(tr("Rectangle"));
        btn_menus_[Graph::RECTANGLE].second = new StyleMenu(StyleMenu::WIDTH_BTN | StyleMenu::FILL_BTN | StyleMenu::COLOR_PENAL, this);

        btn_menus_[Graph::ELLIPSE].first = new QCheckBox(this);
        btn_menus_[Graph::ELLIPSE].first->setObjectName("circle-btn");
        btn_menus_[Graph::ELLIPSE].first->setToolTip(tr("Ellipse"));
        btn_menus_[Graph::ELLIPSE].second = new StyleMenu(StyleMenu::WIDTH_BTN | StyleMenu::FILL_BTN | StyleMenu::COLOR_PENAL, this);

        btn_menus_[Graph::ARROW].first = new QCheckBox(this);
        btn_menus_[Graph::ARROW].first->setObjectName("arrow-btn");
        btn_menus_[Graph::ARROW].first->setToolTip(tr("Arrow"));
        btn_menus_[Graph::ARROW].second = new StyleMenu(StyleMenu::COLOR_PENAL, this);
        btn_menus_[Graph::ARROW].second->fill(true);

        btn_menus_[Graph::LINE].first = new QCheckBox(this);
        btn_menus_[Graph::LINE].first->setObjectName("line-btn");
        btn_menus_[Graph::LINE].first->setToolTip(tr("Line"));
        btn_menus_[Graph::LINE].second = new StyleMenu(StyleMenu::WIDTH_BTN | StyleMenu::COLOR_PENAL, this);

        btn_menus_[Graph::CURVES].first = new QCheckBox(this);
        btn_menus_[Graph::CURVES].first->setObjectName("pen-btn");
        btn_menus_[Graph::CURVES].first->setToolTip(tr("Pencil"));
        btn_menus_[Graph::CURVES].second = new StyleMenu(StyleMenu::WIDTH_BTN | StyleMenu::COLOR_PENAL, this);

        btn_menus_[Graph::TEXT].first = new QCheckBox(this);
        btn_menus_[Graph::TEXT].first->setObjectName("text-btn");
        btn_menus_[Graph::TEXT].first->setToolTip(tr("Text"));
        btn_menus_[Graph::TEXT].second = new StyleMenu(StyleMenu::FONT_BTNS | StyleMenu::COLOR_PENAL, this);

        btn_menus_[Graph::MOSAIC].first = new QCheckBox(this);
        btn_menus_[Graph::MOSAIC].first->setObjectName("mosaic-btn");
        btn_menus_[Graph::MOSAIC].first->setToolTip(tr("Mosaic"));
        btn_menus_[Graph::MOSAIC].second = new StyleMenu(StyleMenu::WIDTH_BTN, this);
        btn_menus_[Graph::MOSAIC].second->lineWidth(15);

        btn_menus_[Graph::ERASER].first = new QCheckBox(this);
        btn_menus_[Graph::ERASER].first->setObjectName("eraser-btn");
        btn_menus_[Graph::ERASER].first->setToolTip(tr("Eraser"));
        btn_menus_[Graph::ERASER].second = new StyleMenu(StyleMenu::WIDTH_BTN, this);
        btn_menus_[Graph::ERASER].second->lineWidth(15);

        for (auto&& [g, bm] : btn_menus_) {
            auto graph = g;
            auto btn = bm.first;
            auto menu = bm.second;

            group_->addButton(btn);
            addWidget(btn);

            connect(menu, &EditMenu::changed, this, &ImageEditMenu::changed);
            connect(this, &EditMenu::moved, [=]() {
                if (menu->isVisible())
                    menu->move(pos().x(), pos().y() + (height() + 3) * (sub_menu_show_pos_ ? -1 : 1));
            });
            connect(btn, &QCheckBox::toggled, [=](bool checked) {
                if (checked) {
                        emit graphChanged(graph);
                        graph_ = graph;
                        menu->show();
                        menu->move(pos().x(), pos().y() + (height() + 3) * (sub_menu_show_pos_ ? -1 : 1)); 
                }                                                       
                else {
                    menu->hide();
                }                                                       
            });                                                         
        }
    }

    /////////////////////////////////////////////////////////////////////////////////
    if (groups & REDO_UNDO_GROUP) {
        addSeparator();

        undo_btn_ = new QCheckBox(this);
        undo_btn_->setCheckable(false);
        undo_btn_->setObjectName("undo-btn");
        undo_btn_->setDisabled(true);
        connect(undo_btn_, &QCheckBox::clicked, this, &ImageEditMenu::undo);
        addWidget(undo_btn_);

        redo_btn_ = new QCheckBox(this);
        redo_btn_->setCheckable(false);
        redo_btn_->setObjectName("redo-btn");
        redo_btn_->setDisabled(true);
        connect(redo_btn_, &QCheckBox::clicked, this, &ImageEditMenu::redo);
        addWidget(redo_btn_);
    }

    /////////////////////////////////////////////////////////////////////////////////
    if (groups & SAVE_GROUP) {
        addSeparator();

        auto pin_btn = new QCheckBox(this);
        pin_btn->setCheckable(false);
        pin_btn->setObjectName("pin-btn");
        connect(pin_btn, &QCheckBox::clicked, [this]() { group_->uncheckAll(); pin(); hide(); });
        addWidget(pin_btn);

        auto save_btn = new QCheckBox(this);
        save_btn->setCheckable(false);
        save_btn->setObjectName("save-btn");
        connect(save_btn, &QCheckBox::clicked, this, &ImageEditMenu::save);
        connect(save_btn, &QCheckBox::clicked, [=]() { group_->uncheckAll(); });
        addWidget(save_btn);
    }

    /////////////////////////////////////////////////////////////////////////////////
    if (groups & EXIT_GROUP) {
        addSeparator();

        auto close_btn = new QCheckBox(this);
        close_btn->setCheckable(false);
        close_btn->setObjectName("close-btn");
        connect(close_btn, &QCheckBox::clicked, [this]() { group_->uncheckAll(); exit(); hide(); });
        addWidget(close_btn);

        auto ok_btn = new QCheckBox(this);
        ok_btn->setCheckable(false);
        ok_btn->setObjectName("ok-btn");
        connect(ok_btn, &QCheckBox::clicked, [this]() { group_->uncheckAll(); ok(); hide(); });
        addWidget(ok_btn);
    }
}

QColor ImageEditMenu::color()
{
    return graph_ ? btn_menus_[graph_].second->color() : Qt::red;
}

void ImageEditMenu::color(const QColor& c)
{
    if (graph_) btn_menus_[graph_].second->color(c);
}

int ImageEditMenu::lineWidth()
{
    return graph_ ? btn_menus_[graph_].second->lineWidth() : 3;
}

void ImageEditMenu::lineWidth(int w)
{
    if (graph_) btn_menus_[graph_].second->lineWidth(w);
}

bool ImageEditMenu::fill()
{
    return graph_ ? btn_menus_[graph_].second->fill() : false;
}

void ImageEditMenu::fill(bool fill)
{
    if (graph_) btn_menus_[graph_].second->fill(fill);
}

QFont ImageEditMenu::font()
{
    return graph_ ? btn_menus_[graph_].second->font() : QFont();
}

void ImageEditMenu::font(const QFont& font)
{
    if (graph_) btn_menus_[graph_].second->font(font);
}

void ImageEditMenu::reset()
{
    group_->uncheckAll();
}
