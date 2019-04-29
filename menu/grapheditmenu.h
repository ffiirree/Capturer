#ifndef CAPTURER_GRAPHMENU_H
#define CAPTURER_GRAPHMENU_H


#include <map>
#include <QPushButton>
#include "editmenu.h"

class GraphMenu : public EditMenu
{
    Q_OBJECT

public:
    explicit GraphMenu(QWidget* parent = nullptr);

private:
    QPushButton * color_btn_ = nullptr;
    QPixmap * color_map_ = nullptr;
    std::map<int, QPushButton*> btns_;
    QPushButton* selected_btn_ = nullptr;
};

#endif // CAPTURER_GRAPHMENU_H
