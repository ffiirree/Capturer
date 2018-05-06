#ifndef SETTINGDIALOG_H
#define SETTINGDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QCheckBox>
#include <QSettings>
#include <QDir>
#include <QCoreApplication>
#include <QLabel>
#include <QLineEdit>
#include "config.h"

class SettingWindow : public QWidget
{
    Q_OBJECT

public:
    explicit SettingWindow(QWidget * parent = nullptr);
    ~SettingWindow() = default;

private slots:
    void setAutoRun(int);

private:
    void setupGeneralWidget();
    void setupAppearanceWidget();
    void setupSnipWidget();
    void setupRecordWidget();
    void setupGIFWidget();
    void setupHotkeyWidget();
    void setupAboutWidget();

    Config * cfg_ = nullptr;

    QTabWidget * tabw_ = nullptr;

    QWidget * background_ = nullptr;

    QWidget * general_ = nullptr;
    QWidget * appearance_ = nullptr;
    QWidget * snip_ = nullptr;
    QWidget * record_ = nullptr;
    QWidget * gif_ = nullptr;
    QWidget * hotkey_ = nullptr;
    QWidget * about_ = nullptr;
};

#endif // SETTINGDIALOG_H
