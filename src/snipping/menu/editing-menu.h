#ifndef IMAGE_EDIT_MENU_H
#define IMAGE_EDIT_MENU_H

#include "editmenu.h"
#include "utils.h"

class QAbstractButton;
class ButtonGroup;
class QCheckBox;

class ImageEditMenu : public EditMenu
{
    Q_OBJECT

public:
    enum MenuGroups : int
    {
        NONE            = 0x00,
        GRAPH_GROUP     = 0x01,
        REDO_UNDO_GROUP = 0x02,
        SAVE_GROUP      = 0x04,
        EXIT_GROUP      = 0x08,
        ALL             = 0xff
    };

public:
    explicit ImageEditMenu(QWidget * = nullptr, uint32_t = MenuGroups::ALL);

    void reset();

    graph_t graph() const { return graph_; }

    // pen
    QPen pen() const override;

    void pen(const QPen&) override;

    // brush
    QBrush brush() const override;

    void brush(const QBrush&) override;

    // fill: pen | brush
    bool fill() const override;

    void fill(bool) override;

    // font
    QFont font() const override;

    void font(const QFont& font) override;

    // submenu position
    void setSubMenuShowAbove(bool v) { sub_menu_show_pos_ = v; }

signals:
    void save();
    void pin();
    void ok();
    void exit();

    void graphChanged(graph_t); // start painting
    void styleChanged();

    void undo();
    void redo();

public slots:
    void disableUndo(bool val);
    void disableRedo(bool val);

    void paintGraph(graph_t graph);

private:
    QCheckBox *undo_btn_{ nullptr };
    QCheckBox *redo_btn_{ nullptr };

    ButtonGroup *group_{ nullptr };

    graph_t graph_{ graph_t::none };
    std::map<graph_t, std::pair<QAbstractButton *, EditMenu *>> btn_menus_; // bind graph with buttons

    bool sub_menu_show_pos_{ false };
};

#endif // IMAGE_EDIT_MENU_H
