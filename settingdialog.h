#ifndef SETTINGDIALOG_H
#define SETTINGDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QAction>
#include <QScrollArea>
#include <QMenu>
#include <QButtonGroup>
#include <QPushButton>
#include <QRadioButton>
#include <QPainter>
#include <QScrollBar>

class SettingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingDialog(QWidget * parent = nullptr);

protected:
    virtual void paintEvent(QPaintEvent *event);

private:
    QScrollArea * scroll_area_ = nullptr;
    QWidget * scroll_widget_ = nullptr;
    QVBoxLayout * scroll_layout_ = nullptr;

    QButtonGroup * btn_group = nullptr;
    QVBoxLayout * menu_layout_ = nullptr;


    //
    QWidget * menu_ = nullptr;
    QWidget * shortcut_widget_ = nullptr;
    QWidget * about_widget_ = nullptr;
};

#endif // SETTINGDIALOG_H
