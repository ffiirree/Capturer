#ifndef CAPTURER_SETTING_DIALOG_H
#define CAPTURER_SETTING_DIALOG_H

#include "config.h"
#include "framelesswindow.h"

#include <QCheckBox>
#include <QStackedWidget>

class SettingWindow : public FramelessWindow
{
    Q_OBJECT

public:
    explicit SettingWindow(QWidget * = nullptr);
    ~SettingWindow() override = default;

private slots:
    void setAutoRun(int);

private:
    QWidget *setupGeneralWidget();
    QWidget *setupSnipWidget();
    QWidget *setupRecordWidget();
    QWidget *setupGIFWidget();
    QWidget *setupDevicesWidget();
    QWidget *setupHotkeyWidget();
    QWidget *setupAboutWidget();

    Config& config = Config::instance();

    QStackedWidget *pages_{ nullptr };

    QCheckBox *autorun_{ nullptr };
};

#endif //! CAPTURER_SETTING_DIALOG_H
