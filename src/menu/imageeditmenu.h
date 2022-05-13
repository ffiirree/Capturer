#ifndef IMAGE_EDIT_MENU_H
#define IMAGE_EDIT_MENU_H

#include "utils.h"
#include "stylemenu.h"
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

    Graph graph() const { return graph_; }

    QColor color() override;
    void color(const QColor& c) override;

    int lineWidth()  override;
    void lineWidth(int w) override;

    bool fill() override;
    void fill(bool fill) override;

    QFont font() override;
    void font(const QFont& font) override;

    void setSubMenuShowAbove() { sub_menu_show_pos_ = true; }
    void setSubMenuShowBelow() { sub_menu_show_pos_ = false; }

signals:
    void save();
    void fix();
    void ok();
    void exit();

    void graphChanged(Graph);          // start painting

    void undo();
    void redo();

public slots:
    void disableUndo(bool val) { undo_btn_->setDisabled(val); }
    void disableRedo(bool val) { redo_btn_->setDisabled(val); }
    void paintGraph(Graph graph) { graph_ = graph; btn_menus_[graph].first->setChecked(true); }

private:
    IconButton* undo_btn_ = nullptr;
    IconButton* redo_btn_ = nullptr;

    ButtonGroup* group_ = nullptr;

    Graph graph_{ Graph::NONE };
    map<Graph, std::pair<IconButton*, EditMenu*>> btn_menus_; // bind graph with buttons

    bool sub_menu_show_pos_ = false;
};


#endif // IMAGE_EDIT_MENU_H
