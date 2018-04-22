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
    explicit GraphMenu(QWidget* parent = nullptr);
    ~GraphMenu() = default;

signals:
    void setWidth(int);
    void setFill(bool);
    void setColor(const QColor& color);

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
