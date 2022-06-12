#ifndef SETTING_DIALOG_H
#define SETTING_DIALOG_H

#include "apptabcontrol.h"
#include "config.h"

class SettingWindow : public QWidget
{
    Q_OBJECT

public:
    explicit SettingWindow(QWidget* = nullptr);
    ~SettingWindow() override = default;

private slots:
    void setAutoRun(int);

private:
    void setupGeneralWidget();
    void setupSnipWidget();
    void setupRecordWidget();
    void setupGIFWidget();
    void setupDevicesWidget();
    void setupHotkeyWidget();
    void setupAboutWidget();

    Config& config = Config::instance();

    AppTabControl* tabwidget_{ nullptr };
};

#endif // SETTING_DIALOG_H
