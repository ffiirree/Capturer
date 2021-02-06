#ifndef LINE_EDIT_MENU_H
#define LINE_EDIT_MENU_H

#include "editmenu.h"

class LineEditMenu : public EditMenu
{
    Q_OBJECT

public:
    explicit LineEditMenu(QWidget * parent = nullptr);
};

#endif // LINE_EDIT_MENU_H
