#ifndef IMAGE_EDIT_MENU_H
#define IMAGE_EDIT_MENU_H

#include "utils.h"

#include <QWidget>

class QAbstractButton;
class ButtonGroup;
class QCheckBox;

class StyleMenu;

class ImageEditMenu : public QWidget
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
    QPen pen() const;

    void setPen(const QPen&);

    // brush
    QBrush brush() const;

    void setBrush(const QBrush&);

    // fill: pen | brush
    bool filled() const;

    void fill(bool);

    // font
    QFont font() const;

    void setFont(const QFont& font);

    // submenu position
    void setSubMenuShowAbove(bool v) { sub_menu_show_pos_ = v; }

signals:
    void save();
    void pin();
    void ok();
    void exit();

    void graphChanged(graph_t); // start painting

    void undo();
    void redo();

    void penChanged(graph_t, const QPen&);
    void brushChanged(graph_t, const QBrush&);
    void fontChanged(graph_t, const QFont&);
    void fillChanged(graph_t, bool);
    void imageArrived(const QPixmap&);

    void moved();

public slots:
    void disableUndo(bool val);
    void disableRedo(bool val);

    void paintGraph(graph_t graph);

protected:
    void moveEvent(QMoveEvent *);

private:
    QCheckBox *undo_btn_{ nullptr };
    QCheckBox *redo_btn_{ nullptr };

    ButtonGroup *group_{ nullptr };

    graph_t graph_{ graph_t::none };
    std::map<graph_t, std::pair<QAbstractButton *, StyleMenu *>> btn_menus_; // bind graph with buttons

    bool sub_menu_show_pos_{ false };
};

#endif // IMAGE_EDIT_MENU_H
