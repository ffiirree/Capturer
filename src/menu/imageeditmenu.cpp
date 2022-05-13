#include "imageeditmenu.h"
#include <QPushButton>
#include <QPixmap>
#include <QStyle>
#include <QHBoxLayout>
#include <QMoveEvent>
#include "logging.h"

ImageEditMenu::ImageEditMenu(QWidget* parent, uint32_t groups)
    : EditMenu(parent)
{
    group_ = new ButtonGroup(this);
    connect(group_, &ButtonGroup::uncheckedAll, [this]() { graph_ = Graph::NONE; graphChanged(Graph::NONE); });

    if (groups & GRAPH_GROUP) {
        btn_menus_[Graph::RECTANGLE].first = new IconButton(QPixmap(":/icon/res/rectangle"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W }, true, this);
        btn_menus_[Graph::RECTANGLE].first->setToolTip(tr("Rectangle"));
        btn_menus_[Graph::RECTANGLE].second = new StyleMenu(StyleMenu::WIDTH_BTN | StyleMenu::FILL_BTN | StyleMenu::COLOR_PENAL, this);

        btn_menus_[Graph::ELLIPSE].first = new IconButton(QPixmap(":/icon/res/circle"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W }, true, this);
        btn_menus_[Graph::ELLIPSE].first->setToolTip(tr("Ellipse"));
        btn_menus_[Graph::ELLIPSE].second = new StyleMenu(StyleMenu::WIDTH_BTN | StyleMenu::FILL_BTN | StyleMenu::COLOR_PENAL, this);

        btn_menus_[Graph::ARROW].first = new IconButton(QPixmap(":/icon/res/arrow"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W }, true, this);
        btn_menus_[Graph::ARROW].first->setToolTip(tr("Arrow"));
        btn_menus_[Graph::ARROW].second = new StyleMenu(StyleMenu::COLOR_PENAL, this);
        btn_menus_[Graph::ARROW].second->fill(true);

        btn_menus_[Graph::LINE].first = new IconButton(QPixmap(":/icon/res/line"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W }, true, this);
        btn_menus_[Graph::LINE].first->setToolTip(tr("Line"));
        btn_menus_[Graph::LINE].second = new StyleMenu(StyleMenu::WIDTH_BTN | StyleMenu::COLOR_PENAL, this);

        btn_menus_[Graph::CURVES].first = new IconButton(QPixmap(":/icon/res/feather"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W }, true, this);
        btn_menus_[Graph::CURVES].first->setToolTip(tr("Pencil"));
        btn_menus_[Graph::CURVES].second = new StyleMenu(StyleMenu::WIDTH_BTN | StyleMenu::COLOR_PENAL, this);

        btn_menus_[Graph::TEXT].first = new IconButton(QPixmap(":/icon/res/text"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W }, true, this);
        btn_menus_[Graph::TEXT].first->setToolTip(tr("Text"));
        btn_menus_[Graph::TEXT].second = new StyleMenu(StyleMenu::FONT_BTNS | StyleMenu::COLOR_PENAL, this);

        btn_menus_[Graph::MOSAIC].first = new IconButton(QPixmap(":/icon/res/mosaic"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W }, true, this);
        btn_menus_[Graph::MOSAIC].first->setToolTip(tr("Mosaic"));
        btn_menus_[Graph::MOSAIC].second = new StyleMenu(StyleMenu::WIDTH_BTN, this);
        btn_menus_[Graph::MOSAIC].second->lineWidth(15);

        btn_menus_[Graph::ERASER].first = new IconButton(QPixmap(":/icon/res/eraser"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W }, true, this);
        btn_menus_[Graph::ERASER].first->setToolTip(tr("Eraser"));
        btn_menus_[Graph::ERASER].second = new StyleMenu(StyleMenu::WIDTH_BTN, this);
        btn_menus_[Graph::ERASER].second->lineWidth(15);

        for (auto&& [g, bm] : btn_menus_) {
            auto graph = g;
            auto btn = bm.first;
            auto menu = bm.second;

            group_->addButton(btn);
            addButton(btn);

            connect(menu, &EditMenu::changed, this, &ImageEditMenu::changed);
            connect(this, &EditMenu::moved, [=]() {
                if (menu->isVisible())
                    menu->move(pos().x(), pos().y() + (height() + 3) * (sub_menu_show_pos_ ? -1 : 1));
            });
            connect(btn, &IconButton::toggled, [=](bool checked) {
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

        undo_btn_ = new IconButton(QPixmap(":/icon/res/undo"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W }, false, this);
        undo_btn_->setDisabled(true);
        connect(undo_btn_, &IconButton::clicked, this, &ImageEditMenu::undo);
        addButton(undo_btn_);

        redo_btn_ = new IconButton(QPixmap(":/icon/res/redo"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W }, false, this);
        redo_btn_->setDisabled(true);
        connect(redo_btn_, &IconButton::clicked, this, &ImageEditMenu::redo);
        addButton(redo_btn_);
    }

    /////////////////////////////////////////////////////////////////////////////////
    if (groups & SAVE_GROUP) {
        addSeparator();

        auto fix_btn = new IconButton(QPixmap(":/icon/res/pin"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W }, false, this);
        connect(fix_btn, &IconButton::clicked, [this]() { group_->uncheckAll(); fix(); hide(); });
        addButton(fix_btn);

        auto save_btn = new IconButton(QPixmap(":/icon/res/save"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W }, false, this);
        connect(save_btn, &IconButton::clicked, this, &ImageEditMenu::save);
        connect(save_btn, &IconButton::clicked, [=]() { group_->uncheckAll(); });
        addButton(save_btn);
    }

    /////////////////////////////////////////////////////////////////////////////////
    if (groups & EXIT_GROUP) {
        addSeparator();

        auto close_btn = new QPushButton(QIcon(":/icon/res/wrong"), QString(), this);
        close_btn->setObjectName("close_btn");
        close_btn->setIconSize({ ICON_W, ICON_W });
        connect(close_btn, &QPushButton::clicked, [this]() { group_->uncheckAll(); exit(); hide(); });
        addWidget(close_btn);

        auto ok_btn = new QPushButton(QIcon(":/icon/res/right"), QString(), this);
        ok_btn->setObjectName("ok_btn");
        ok_btn->setIconSize({ ICON_W, ICON_W });
        connect(ok_btn, &QPushButton::clicked, [this]() { group_->uncheckAll(); ok(); hide(); });
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
