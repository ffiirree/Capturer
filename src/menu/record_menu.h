#ifndef RECORD_MENU_H
#define RECORD_MENU_H

#include <QWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QTimer>
#include <QTime>

class RecordMenu : public QWidget
{
    Q_OBJECT

public:
    explicit RecordMenu(QWidget* parent = nullptr);

signals:
    void started();
    void paused();
    void resumed();
    void muted(int, bool);
    void stopped();

public slots:
    void start();
    void pause();
    void resume();
    void update();

private:
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    QWidget* window_ = nullptr;

    QPushButton* close_btn_ = nullptr;
    QCheckBox* mic_ = nullptr;
    QCheckBox* speaker_ = nullptr;
    QCheckBox* pause_ = nullptr;
    QLabel* time_label_ = nullptr;
    QTimer* timer_ = nullptr;
    QTime time_{ 0,0,0 };
    qint64 counter_ = 0;

    QPoint begin_pos_{ 0,0 };
    bool moving_ = false;
};

#endif // RECORD_MENU_H
