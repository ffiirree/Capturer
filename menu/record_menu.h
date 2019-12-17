#ifndef RECORD_MENU_H
#define RECORD_MENU_H

#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QTimer>
#include <QTime>

#define MENU_HEIGHT             45

class RecordMenu : public QFrame
{
    Q_OBJECT

public:
    explicit RecordMenu(QWidget* parent = nullptr);
    ~RecordMenu();

signals:
    void STOP();

public slots:
    void start();
    void stop();
    void update();

private:
    QPushButton * close_btn_ = nullptr;
    QHBoxLayout * layout_ = nullptr;
    QLabel * flag_label_ = nullptr;
    QLabel * time_label_ = nullptr;
    QTimer * timer_ = nullptr;
    QTime * time_ = nullptr;
};

#endif // RECORD_MENU_H
