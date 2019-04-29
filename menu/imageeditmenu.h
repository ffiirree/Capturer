#ifndef CAPTURER_MENU_H
#define CAPTURER_MENU_H

#include <map>
#include <QPushButton>
#include "utils.h"
#include "grapheditmenu.h"
#include "arroweditmenu.h"
#include "lineeditmenu.h"
#include "texteditmenu.h"
#include "erasemenu.h"
#include "iconbutton.h"

class ImageEditMenu : public EditMenu
{
    Q_OBJECT

public:
    explicit ImageEditMenu(QWidget* parent = nullptr);

    void reset();

    QPen pen(Graph);
    void pen(Graph, int);
    bool fill(Graph);
    QFont font(Graph);

signals:
    void save();
    void fix();
    void copy();
    void exit();

    void paint(Graph);

    void undo();
    void redo();

    void changed(Graph);

public slots:
    void disableUndo(bool val) { undo_btn_->setDisabled(val); }
    void disableRedo(bool val) { redo_btn_->setDisabled(val); }

private:
    QAbstractButton* undo_btn_ = nullptr;
    QAbstractButton* redo_btn_ = nullptr;

    GraphMenu * rectangle_menu_ = nullptr;
    GraphMenu * circle_menu_ = nullptr;
    ArrowEditMenu *arrow_menu_ = nullptr;
    LineEditMenu * line_menu_ = nullptr;
    LineEditMenu * curves_menu_ = nullptr;
    FontMenu * text_menu_ = nullptr;
    EraseMenu *mosaic_menu_ = nullptr;
    EraseMenu *erase_menu_ = nullptr;

    ButtonGroup * group_ = nullptr;
};


#endif //! CAPTURER_MENU_H
