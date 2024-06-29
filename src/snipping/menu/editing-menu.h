#ifndef CAPTURER_EDITING_MENU_H
#define CAPTURER_EDITING_MENU_H

#include "buttongroup.h"
#include "canvas/types.h"
#include "editing-submenu.h"
#include "framelesswindow.h"

class QCheckBox;

class EditingMenu final : public FramelessWindow
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
        ADVANCED_GROUP  = 0x10,
        ALL             = 0xff
    };

public:
    explicit EditingMenu(QWidget *, uint32_t = MenuGroups::ALL);

    void reset();

    [[nodiscard]] canvas::graphics_t graph() const { return graph_; }

    // pen
    [[nodiscard]] QPen pen() const;

    void setPen(const QPen&, bool silence = true);

    // fill: pen | brush
    [[nodiscard]] bool filled() const;

    void fill(bool);

    // font
    [[nodiscard]] QFont font() const;

    void setFont(const QFont& font);

    // submenu position
    void setSubMenuShowAbove(bool v) { sub_menu_show_pos_ = v; }

signals:
    void scroll(); // scrolling screenshot

    void save();
    void pin();
    void copy();
    void exit();
    void undo();
    void redo();

    void graphChanged(canvas::graphics_t); // start painting

    void penChanged(canvas::graphics_t, const QPen&);
    void fontChanged(canvas::graphics_t, const QFont&);
    void fillChanged(canvas::graphics_t, bool);

    void imageArrived(const QPixmap&);

    void moved();

public slots:
    void canUndo(bool val);
    void canRedo(bool val);

    void paintGraph(canvas::graphics_t graph);

protected:
    void moveEvent(QMoveEvent *) override;
    void closeEvent(QCloseEvent *) override;

private:
    QCheckBox *undo_btn_{};
    QCheckBox *redo_btn_{};

    ButtonGroup                                   *group_{};
    std::map<canvas::graphics_t, EditingSubmenu *> submenus_; // bind graph with buttons

    QString pictures_path_{};

    canvas::graphics_t graph_{ canvas::none };

    bool sub_menu_show_pos_{ false };
};

#endif //! CAPTURER_EDITING_MENU_H
