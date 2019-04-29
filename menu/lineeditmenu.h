#ifndef LINEEDITMENU_H
#define LINEEDITMENU_H

#include "editmenu.h"

class LineEditMenu : public EditMenu
{
    Q_OBJECT

public:
    explicit LineEditMenu(QWidget * parent = nullptr);
};

#endif // LINEEDITMENU_H
