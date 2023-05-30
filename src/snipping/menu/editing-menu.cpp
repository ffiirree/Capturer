#include "editing-menu.h"

#include "buttongroup.h"
#include "editing-submenu.h"
#include "separator.h"

#include <map>
#include <QBrush>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QMoveEvent>
#include <QPixmap>

ImageEditMenu::ImageEditMenu(QWidget *parent, uint32_t groups)
    : QWidget(parent)
{
    setCursor(Qt::ArrowCursor);

    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowFlags(windowFlags() | Qt::ToolTip | Qt::WindowStaysOnTopHint);

    setLayout(new QHBoxLayout(this));
    layout()->setSpacing(0);
    layout()->setContentsMargins({});

    group_ = new ButtonGroup(this);
    connect(group_, &ButtonGroup::uncheckedAll, [this]() {
        graph_ = graph_t::none;
        graphChanged(graph_t::none);
    });

    if (groups & GRAPH_GROUP) {
        btn_menus_[graph_t::rectangle].first = new QCheckBox(this);
        btn_menus_[graph_t::rectangle].first->setObjectName("rect-btn");
        btn_menus_[graph_t::rectangle].first->setToolTip(tr("Rectangle"));
        btn_menus_[graph_t::rectangle].second =
            new StyleMenu(StyleMenu::WIDTH_BTN | StyleMenu::FILL_BTN | StyleMenu::COLOR_PENAL, this);

        btn_menus_[graph_t::ellipse].first = new QCheckBox(this);
        btn_menus_[graph_t::ellipse].first->setObjectName("circle-btn");
        btn_menus_[graph_t::ellipse].first->setToolTip(tr("Ellipse"));
        btn_menus_[graph_t::ellipse].second =
            new StyleMenu(StyleMenu::WIDTH_BTN | StyleMenu::FILL_BTN | StyleMenu::COLOR_PENAL, this);

        btn_menus_[graph_t::arrow].first = new QCheckBox(this);
        btn_menus_[graph_t::arrow].first->setObjectName("arrow-btn");
        btn_menus_[graph_t::arrow].first->setToolTip(tr("Arrow"));
        btn_menus_[graph_t::arrow].second = new StyleMenu(StyleMenu::COLOR_PENAL, this);
        btn_menus_[graph_t::arrow].second->fill(true);

        btn_menus_[graph_t::line].first = new QCheckBox(this);
        btn_menus_[graph_t::line].first->setObjectName("line-btn");
        btn_menus_[graph_t::line].first->setToolTip(tr("Line"));
        btn_menus_[graph_t::line].second =
            new StyleMenu(StyleMenu::WIDTH_BTN | StyleMenu::COLOR_PENAL, this);

        btn_menus_[graph_t::curve].first = new QCheckBox(this);
        btn_menus_[graph_t::curve].first->setObjectName("pen-btn");
        btn_menus_[graph_t::curve].first->setToolTip(tr("Pencil"));
        btn_menus_[graph_t::curve].second =
            new StyleMenu(StyleMenu::WIDTH_BTN | StyleMenu::COLOR_PENAL, this);

        btn_menus_[graph_t::text].first = new QCheckBox(this);
        btn_menus_[graph_t::text].first->setObjectName("text-btn");
        btn_menus_[graph_t::text].first->setToolTip(tr("Text"));
        btn_menus_[graph_t::text].second =
            new StyleMenu(StyleMenu::FONT_BTNS | StyleMenu::COLOR_PENAL, this);

        btn_menus_[graph_t::mosaic].first = new QCheckBox(this);
        btn_menus_[graph_t::mosaic].first->setObjectName("mosaic-btn");
        btn_menus_[graph_t::mosaic].first->setToolTip(tr("Mosaic"));
        btn_menus_[graph_t::mosaic].second = new StyleMenu(StyleMenu::WIDTH_BTN, this);
        // btn_menus_[graph_t::mosaic].second->lineWidth(15);

        btn_menus_[graph_t::eraser].first = new QCheckBox(this);
        btn_menus_[graph_t::eraser].first->setObjectName("eraser-btn");
        btn_menus_[graph_t::eraser].first->setToolTip(tr("Eraser"));
        btn_menus_[graph_t::eraser].second = new StyleMenu(StyleMenu::WIDTH_BTN, this);
        // btn_menus_[graph_t::eraser].second->lineWidth(15);

        for (auto&& [g, bm] : btn_menus_) {
            auto graph = g;
            auto btn   = bm.first;
            auto menu  = bm.second;

            group_->addButton(btn);
            layout()->addWidget(btn);

            // clang-format off
            connect(menu, &StyleMenu::penChanged,   [graph, this](auto pen) { penChanged(graph, pen); });
            connect(menu, &StyleMenu::brushChanged, [graph, this](auto brush) { emit brushChanged(graph, brush); });
            connect(menu, &StyleMenu::fontChanged,  [graph, this](auto font) { emit fontChanged(graph, font); });
            connect(menu, &StyleMenu::fillChanged,  [graph, this](auto filled) { emit fillChanged(graph, filled); });
            // clang-format on

            connect(this, &ImageEditMenu::moved, [=, this]() {
                if (menu->isVisible())
                    menu->move(pos().x(), pos().y() + (height() + 3) * (sub_menu_show_pos_ ? -1 : 1));
            });
            connect(btn, &QCheckBox::toggled, [=, this](bool checked) {
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
        layout()->addWidget(new Separator());

        undo_btn_ = new QCheckBox(this);
        undo_btn_->setCheckable(false);
        undo_btn_->setObjectName("undo-btn");
        undo_btn_->setDisabled(true);
        connect(undo_btn_, &QCheckBox::clicked, this, &ImageEditMenu::undo);
        layout()->addWidget(undo_btn_);

        redo_btn_ = new QCheckBox(this);
        redo_btn_->setCheckable(false);
        redo_btn_->setObjectName("redo-btn");
        redo_btn_->setDisabled(true);
        connect(redo_btn_, &QCheckBox::clicked, this, &ImageEditMenu::redo);
        layout()->addWidget(redo_btn_);
    }

    /////////////////////////////////////////////////////////////////////////////////
    if (groups & SAVE_GROUP) {
        layout()->addWidget(new Separator());

        auto pin_btn = new QCheckBox(this);
        pin_btn->setCheckable(false);
        pin_btn->setObjectName("pin-btn");
        connect(pin_btn, &QCheckBox::clicked, [this]() {
            group_->uncheckAll();
            pin();
            hide();
        });
        layout()->addWidget(pin_btn);

        auto save_btn = new QCheckBox(this);
        save_btn->setCheckable(false);
        save_btn->setObjectName("save-btn");
        connect(save_btn, &QCheckBox::clicked, this, &ImageEditMenu::save);
        connect(save_btn, &QCheckBox::clicked, [this]() { group_->uncheckAll(); });
        layout()->addWidget(save_btn);
    }

    /////////////////////////////////////////////////////////////////////////////////
    if (groups & EXIT_GROUP) {
        layout()->addWidget(new Separator());

        auto close_btn = new QCheckBox(this);
        close_btn->setCheckable(false);
        close_btn->setObjectName("close-btn");
        connect(close_btn, &QCheckBox::clicked, [this]() {
            group_->uncheckAll();
            exit();
            hide();
        });
        layout()->addWidget(close_btn);

        auto ok_btn = new QCheckBox(this);
        ok_btn->setCheckable(false);
        ok_btn->setObjectName("ok-btn");
        connect(ok_btn, &QCheckBox::clicked, [this]() {
            group_->uncheckAll();
            ok();
            hide();
        });
        layout()->addWidget(ok_btn);
    }
}

void ImageEditMenu::disableUndo(bool val) { undo_btn_->setDisabled(val); }

void ImageEditMenu::disableRedo(bool val) { redo_btn_->setDisabled(val); }

void ImageEditMenu::paintGraph(graph_t graph)
{
    graph_ = graph;
    btn_menus_[graph].first->setChecked(true);
}

QPen ImageEditMenu::pen() const
{
    return (graph_ != graph_t::none) ? btn_menus_.at(graph_).second->pen() : QPen{ Qt::red };
}

void ImageEditMenu::setPen(const QPen& pen)
{
    if (graph_ != graph_t::none) btn_menus_.at(graph_).second->setPen(pen);
}

QBrush ImageEditMenu::brush() const
{
    return (graph_ != graph_t::none) ? btn_menus_.at(graph_).second->brush() : QBrush{};
}

void ImageEditMenu::setBrush(const QBrush& brush)
{
    if (graph_ != graph_t::none) btn_menus_.at(graph_).second->setBrush(brush);
}

bool ImageEditMenu::filled() const
{
    return (graph_ != graph_t::none) ? btn_menus_.at(graph_).second->filled() : false;
}

void ImageEditMenu::fill(bool v)
{
    if (graph_ != graph_t::none) btn_menus_[graph_].second->fill(v);
}

QFont ImageEditMenu::font() const
{
    return (graph_ != graph_t::none) ? btn_menus_.at(graph_).second->font() : QFont{};
}

void ImageEditMenu::setFont(const QFont& font)
{
    if (graph_ != graph_t::none) btn_menus_[graph_].second->setFont(font);
}

void ImageEditMenu::reset() { group_->uncheckAll(); }

void ImageEditMenu::moveEvent(QMoveEvent *) { emit moved(); }