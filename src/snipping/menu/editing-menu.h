#ifndef IMAGE_EDIT_MENU_H
#define IMAGE_EDIT_MENU_H

#include "canvas/types.h"

#include <QWidget>

class ButtonGroup;
class QCheckBox;
class EditingSubmenu;

class EditingMenu : public QWidget
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
    explicit EditingMenu(QWidget * = nullptr, uint32_t = MenuGroups::ALL);

    void reset();

    canvas::graphics_t graph() const { return graph_; }

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
    void copy();
    void exit();
    void undo();
    void redo();

    void graphChanged(canvas::graphics_t); // start painting

    void penChanged(canvas::graphics_t, const QPen&);
    void brushChanged(canvas::graphics_t, const QBrush&);
    void fontChanged(canvas::graphics_t, const QFont&);
    void fillChanged(canvas::graphics_t, bool);
    void imageArrived(const QPixmap&);

    void moved();

public slots:
    void disableUndo(bool val);
    void disableRedo(bool val);

    void paintGraph(canvas::graphics_t graph);

protected:
    void moveEvent(QMoveEvent *) override;
    void closeEvent(QCloseEvent *) override;

private:
    QCheckBox *undo_btn_{};
    QCheckBox *redo_btn_{};

    ButtonGroup *group_{};
    std::map<canvas::graphics_t, EditingSubmenu *> submenus_; // bind graph with buttons

    QString pictures_path_ {};

    canvas::graphics_t graph_{ canvas::none };

    bool sub_menu_show_pos_{ false };
};

#endif // IMAGE_EDIT_MENU_H
