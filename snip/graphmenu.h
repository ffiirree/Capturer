#ifndef CAPTURER_GRAPHMENU_H
#define CAPTURER_GRAPHMENU_H

#include <QFrame>
#include <QHBoxLayout>
#include <vector>
#include <map>
#include <QPushButton>
#include <QPropertyAnimation>
#include <QStyle>

class GraphMenu : public QFrame
{
    Q_OBJECT
public:
//    enum Button {
//    };

public:
    explicit GraphMenu(QWidget* parent = nullptr);
    ~GraphMenu();

signals:
    void SET_WIDTH_01();
    void SET_WIDTH_02();
    void SET_WIDTH_03();

    void SET_FILL();
    void SET_UNFILL();
    void SET_COLOR(const QColor& color);

private:
    void click(QPushButton* btn);
    void unselectAll();

    QPushButton * color_btn_ = nullptr;
    QPixmap * color_map_ = nullptr;
    QHBoxLayout * layout_ = nullptr;
    std::map<int, QPushButton*> btns_;
    QPushButton* selected_btn_ = nullptr;
};

#endif // CAPTURER_GRAPHMENU_H
