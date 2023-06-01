#include "editing-menu.h"

#include "buttongroup.h"
#include "editing-submenu.h"
#include "separator.h"

#include <map>
#include <QBrush>
#include <QCheckBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMoveEvent>
#include <QPixmap>
#include <QStandardPaths>

EditingMenu::EditingMenu(QWidget *parent, uint32_t groups)
    : QWidget(parent)
{
    setCursor(Qt::ArrowCursor);

    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowFlags(windowFlags() | Qt::ToolTip | Qt::WindowStaysOnTopHint);
    setFocusPolicy(Qt::NoFocus);

    setLayout(new QHBoxLayout(this));
    layout()->setSpacing(0);
    layout()->setContentsMargins({});

    //
    pictures_path_ = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);

    //
    group_ = new ButtonGroup(this);
    connect(group_, &ButtonGroup::emptied, [this]() {
        graph_ = canvas::none;
        graphChanged(canvas::none);
    });

    if (groups & GRAPH_GROUP) {
        // rectangle
        group_->addButton(new QCheckBox(this), canvas::rectangle);
        group_->button(canvas::rectangle)->setObjectName("rect-btn");
        group_->button(canvas::rectangle)->setToolTip("Rectangle");
        submenus_[canvas::rectangle] = new EditingSubmenu(
            EditingSubmenu::WIDTH_BTN | EditingSubmenu::FILL_BTN | EditingSubmenu::COLOR_PENAL, this);

        // ellipse
        group_->addButton(new QCheckBox(this), canvas::ellipse);
        group_->button(canvas::ellipse)->setObjectName("ellipse-btn");
        group_->button(canvas::ellipse)->setToolTip("Ellipse");
        submenus_[canvas::ellipse] = new EditingSubmenu(
            EditingSubmenu::WIDTH_BTN | EditingSubmenu::FILL_BTN | EditingSubmenu::COLOR_PENAL, this);

        // arrow
        group_->addButton(new QCheckBox(this), canvas::arrow);
        group_->button(canvas::arrow)->setObjectName("arrow-btn");
        group_->button(canvas::arrow)->setToolTip("Arrow");
        submenus_[canvas::arrow] = new EditingSubmenu(EditingSubmenu::COLOR_PENAL, this);
        submenus_[canvas::arrow]->fill(true);

        // line
        group_->addButton(new QCheckBox(this), canvas::line);
        group_->button(canvas::line)->setObjectName("line-btn");
        group_->button(canvas::line)->setToolTip("Line");
        submenus_[canvas::line] =
            new EditingSubmenu(EditingSubmenu::WIDTH_BTN | EditingSubmenu::COLOR_PENAL, this);

        // pencil
        group_->addButton(new QCheckBox(this), canvas::curve);
        group_->button(canvas::curve)->setObjectName("pencil-btn");
        group_->button(canvas::curve)->setToolTip("Pencil");
        submenus_[canvas::curve] =
            new EditingSubmenu(EditingSubmenu::WIDTH_BTN | EditingSubmenu::COLOR_PENAL, this);

        // pixmap
        group_->addButton(new QCheckBox(this), canvas::pixmap);
        group_->button(canvas::pixmap)->setCheckable(false);
        group_->button(canvas::pixmap)->setObjectName("image-btn");
        group_->button(canvas::pixmap)->setToolTip("Image");

        connect(group_->button(canvas::pixmap), &QCheckBox::clicked, [=, this]() {
            graph_ = canvas::pixmap;
            emit graphChanged(canvas::pixmap);

            auto filename = QFileDialog::getOpenFileName(this, tr("Open Image"), pictures_path_,
                                                         "Image Files(*.png *.jpg *.jpeg *.bmp *.svg)");

            if (!filename.isEmpty()) {
                pictures_path_ = QFileInfo(filename).absolutePath();

                emit imageArrived(QPixmap(filename));
            }
        });

        // counter
        group_->addButton(new QCheckBox(this), canvas::counter);
        group_->button(canvas::counter)->setObjectName("counter-btn");
        group_->button(canvas::counter)->setToolTip("Counter");

        // text
        group_->addButton(new QCheckBox(this), canvas::text);
        group_->button(canvas::text)->setObjectName("text-btn");
        group_->button(canvas::text)->setToolTip("Text");
        submenus_[canvas::text] =
            new EditingSubmenu(EditingSubmenu::FONT_BTNS | EditingSubmenu::COLOR_PENAL, this);

        // mosaic
        group_->addButton(new QCheckBox(this), canvas::mosaic);
        group_->button(canvas::mosaic)->setObjectName("mosaic-btn");
        group_->button(canvas::mosaic)->setToolTip("Mosaic");
        submenus_[canvas::mosaic] = new EditingSubmenu(EditingSubmenu::WIDTH_BTN, this);
        submenus_[canvas::mosaic]->setPen(QPen{ Qt::transparent, 15 });

        // mosaic
        group_->addButton(new QCheckBox(this), canvas::eraser);
        group_->button(canvas::eraser)->setObjectName("eraser-btn");
        group_->button(canvas::eraser)->setToolTip("Eraser");
        submenus_[canvas::eraser] = new EditingSubmenu(EditingSubmenu::WIDTH_BTN, this);
        submenus_[canvas::eraser]->setPen(QPen{ Qt::transparent, 15 });

        //
        for (const auto button : group_->buttons()) {
            auto gtype = static_cast<canvas::graphics_t>(group_->id(button));

            layout()->addWidget(button);

            // style changed event
            if (submenus_.contains(gtype)) {
                // clang-format off
                connect(submenus_[gtype], &EditingSubmenu::penChanged,   [gtype, this](auto pen)    { emit penChanged(gtype, pen); });
                connect(submenus_[gtype], &EditingSubmenu::brushChanged, [gtype, this](auto brush)  { emit brushChanged(gtype, brush); });
                connect(submenus_[gtype], &EditingSubmenu::fontChanged,  [gtype, this](auto font)   { emit fontChanged(gtype, font); });
                connect(submenus_[gtype], &EditingSubmenu::fillChanged,  [gtype, this](auto filled) { emit fillChanged(gtype, filled); });
                // clang-format on

                connect(this, &EditingMenu::moved, [=, this]() {
                    if (submenus_[gtype]->isVisible())
                        submenus_[gtype]->move(pos().x(),
                                               pos().y() + (height() + 3) * (sub_menu_show_pos_ ? -1 : 1));
                });
            }
        }

        // toggled
        connect(group_, &ButtonGroup::buttonToggled, [=, this](auto button, auto checked) {
            if (checked) {
                graph_ = static_cast<canvas::graphics_t>(group_->id(button));

                emit graphChanged(graph_);
            }

            canvas::graphics_t button_graph = static_cast<canvas::graphics_t>(group_->id(button));
            if (submenus_.contains(button_graph)) {
                submenus_[button_graph]->setVisible(checked);
                if (checked) {
                    submenus_[button_graph]->move(pos().x(), pos().y() + (height() + 3) *
                                                                             (sub_menu_show_pos_ ? -1 : 1));
                }
            }
        });
    }

    /////////////////////////////////////////////////////////////////////////////////
    if (groups & REDO_UNDO_GROUP) {
        layout()->addWidget(new Separator());

        undo_btn_ = new QCheckBox(this);
        undo_btn_->setCheckable(false);
        undo_btn_->setObjectName("undo-btn");
        undo_btn_->setDisabled(true);
        connect(undo_btn_, &QCheckBox::clicked, this, &EditingMenu::undo);
        layout()->addWidget(undo_btn_);

        redo_btn_ = new QCheckBox(this);
        redo_btn_->setCheckable(false);
        redo_btn_->setObjectName("redo-btn");
        redo_btn_->setDisabled(true);
        connect(redo_btn_, &QCheckBox::clicked, this, &EditingMenu::redo);
        layout()->addWidget(redo_btn_);
    }

    /////////////////////////////////////////////////////////////////////////////////
    if (groups & SAVE_GROUP) {
        layout()->addWidget(new Separator());

        auto pin_btn = new QCheckBox(this);
        pin_btn->setCheckable(false);
        pin_btn->setObjectName("pin-btn");
        connect(pin_btn, &QCheckBox::clicked, [this]() { emit pin(); hide(); });
        layout()->addWidget(pin_btn);

        auto save_btn = new QCheckBox(this);
        save_btn->setCheckable(false);
        save_btn->setObjectName("save-btn");
        connect(save_btn, &QCheckBox::clicked, this, &EditingMenu::save);
        connect(save_btn, &QCheckBox::clicked, group_, &ButtonGroup::clear);
        layout()->addWidget(save_btn);
    }

    /////////////////////////////////////////////////////////////////////////////////
    if (groups & EXIT_GROUP) {
        layout()->addWidget(new Separator());

        auto close_btn = new QCheckBox(this);
        close_btn->setCheckable(false);
        close_btn->setObjectName("close-btn");
        connect(close_btn, &QCheckBox::clicked, [this]() { emit exit(); close(); });
        layout()->addWidget(close_btn);

        auto ok_btn = new QCheckBox(this);
        ok_btn->setCheckable(false);
        ok_btn->setObjectName("ok-btn");
        connect(ok_btn, &QCheckBox::clicked, [this]() { emit copy(); close(); });
        layout()->addWidget(ok_btn);
    }
}

void EditingMenu::disableUndo(bool val) { undo_btn_->setDisabled(val); }

void EditingMenu::disableRedo(bool val) { redo_btn_->setDisabled(val); }

void EditingMenu::paintGraph(canvas::graphics_t graph)
{
    graph_ = graph;
    if (group_->button(graph_)) group_->button(graph_)->setChecked(true);
}

QPen EditingMenu::pen() const
{
    return (submenus_.contains(graph_)) ? submenus_.at(graph_)->pen() : QPen{ Qt::red };
}

void EditingMenu::setPen(const QPen& pen)
{
    if (submenus_.contains(graph_)) submenus_.at(graph_)->setPen(pen);
}

QBrush EditingMenu::brush() const
{
    return (submenus_.contains(graph_)) ? submenus_.at(graph_)->brush() : QBrush{};
}

void EditingMenu::setBrush(const QBrush& brush)
{
    if (submenus_.contains(graph_)) submenus_.at(graph_)->setBrush(brush);
}

bool EditingMenu::filled() const
{
    return (submenus_.contains(graph_)) ? submenus_.at(graph_)->filled() : false;
}

void EditingMenu::fill(bool v)
{
    if (submenus_.contains(graph_)) submenus_.at(graph_)->fill(v);
}

QFont EditingMenu::font() const
{
    return (submenus_.contains(graph_)) ? submenus_.at(graph_)->font() : QFont{};
}

void EditingMenu::setFont(const QFont& font)
{
    if (submenus_.contains(graph_)) submenus_.at(graph_)->setFont(font);
}

void EditingMenu::reset()
{
    group_->clear();
    redo_btn_->setDisabled(true);
    undo_btn_->setDisabled(true);
}

void EditingMenu::moveEvent(QMoveEvent *) { emit moved(); }

void EditingMenu::closeEvent(QCloseEvent *) { reset(); }