#ifndef SETTING_DIALOG_H
#define SETTING_DIALOG_H

#include <QStackedWidget>
#include "config.h"

class QCheckBox;

class SettingWindow : public QWidget
{
    Q_OBJECT

public:
    explicit SettingWindow(QWidget* = nullptr);
    ~SettingWindow() override = default;

private slots:
    void setAutoRun(int);

private:
    QWidget* setupGeneralWidget();
    QWidget* setupSnipWidget();
    QWidget* setupRecordWidget();
    QWidget* setupGIFWidget();
    QWidget* setupDevicesWidget();
    QWidget* setupHotkeyWidget();
    QWidget* setupAboutWidget();

    Config& config = Config::instance();

    QStackedWidget* pages_{ nullptr };

    QCheckBox* autorun_{ nullptr };
};

#endif // SETTING_DIALOG_H
