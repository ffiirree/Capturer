#ifndef IMAGE_EDIT_MENU_H
#define IMAGE_EDIT_MENU_H

#include "utils.h"
#include "grapheditmenu.h"
#include "arroweditmenu.h"
#include "lineeditmenu.h"
#include "texteditmenu.h"
#include "erasemenu.h"
#include "iconbutton.h"
#include "buttongroup.h"

class ImageEditMenu : public EditMenu
{
    Q_OBJECT

public:
    enum MenuGroups : int {
        NONE = 0x00,
        GRAPH_GROUP = 0x01,
        REDO_UNDO_GROUP = 0x02,
        SAVE_GROUP = 0x04,
        EXIT_GROUP = 0x08,
        ALL = 0xff
    };

public:
    explicit ImageEditMenu(QWidget* = nullptr, uint32_t = MenuGroups::ALL);

    void reset();

    bool fill(Graph);
    void fill(Graph, bool);
    QPen pen(Graph);
    void pen(Graph, QPen);
    void style(Graph, QPen, bool);
    QFont font(Graph);
    void font(const QFont& font);

    void setSubMenuShowAbove() { sub_menu_show_pos_ = true; }
    void setSubMenuShowBelow() { sub_menu_show_pos_ = false; }

signals:
    void save();
    void fix();
    void ok();
    void exit();

    void graphChanged(Graph);          // start painting
    void styleChanged(Graph);          // the style changed

    void undo();
    void redo();

public slots:
    void disableUndo(bool val) { undo_btn_->setDisabled(val); }
    void disableRedo(bool val) { redo_btn_->setDisabled(val); }
    void paintGraph(Graph graph) { buttons_[graph]->setChecked(true); }

private:
    IconButton* undo_btn_ = nullptr;
    IconButton* redo_btn_ = nullptr;

    GraphMenu* rectangle_menu_ = nullptr;
    GraphMenu* circle_menu_ = nullptr;
    ArrowEditMenu* arrow_menu_ = nullptr;
    LineEditMenu* line_menu_ = nullptr;
    LineEditMenu* curves_menu_ = nullptr;
    FontMenu* text_menu_ = nullptr;
    EraseMenu* mosaic_menu_ = nullptr;
    EraseMenu* erase_menu_ = nullptr;

    ButtonGroup* group_ = nullptr;
    std::map<Graph, IconButton*> buttons_; // bind graph with buttons

    bool sub_menu_show_pos_ = false;
};


#endif // IMAGE_EDIT_MENU_H
