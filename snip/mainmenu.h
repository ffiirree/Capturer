#ifndef CAPTURER_MENU_H
#define CAPTURER_MENU_H

#include <QFrame>
#include <QHBoxLayout>
#include <vector>
#include <map>
#include <QPushButton>

class MainMenu : public QFrame
{
    Q_OBJECT

public:
    explicit MainMenu(QWidget* parent = nullptr);
    ~MainMenu() = default;

    void reset();

signals:
    void save();
    void fix();
    void copy();
    void exit();

    void START_PAINT_RECTANGLE();
    void END_PAINT_RECTANGLE();

    void START_PAINT_CIRCLE();
    void END_PAINT_CIRCLE();

    void START_PAINT_LINE();
    void END_PAINT_LINE();

    void START_PAINT_CURVES();
    void END_PAINT_CURVES();

    void START_PAINT_ARROW();
    void END_PAINT_ARROW();

    void START_PAINT_TEXT();
    void END_PAINT_TEXT();

    void undo();
    void redo();

private:
    void click(QPushButton* btn);
    void unselectAll();

    QHBoxLayout * layout_ = nullptr;
    std::map<int, QPushButton*> btns_;
    QPushButton* selected_btn_ = nullptr;
};

#endif //! CAPTURER_MENU_H
