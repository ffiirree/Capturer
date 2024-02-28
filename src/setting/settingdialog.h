#ifndef CAPTURER_SETTING_DIALOG_H
#define CAPTURER_SETTING_DIALOG_H

#include "framelesswindow.h"

class SettingWindow final : public FramelessWindow
{
    Q_OBJECT

public:
    explicit SettingWindow(QWidget * = nullptr);

private:
    QWidget *setupGeneralWidget();
    QWidget *setupSnipWidget();
    QWidget *setupRecordWidget();
    QWidget *setupGIFWidget();
    QWidget *setupDevicesWidget();
    QWidget *setupHotkeyWidget();
    QWidget *setupAboutWidget();
};

#endif //! CAPTURER_SETTING_DIALOG_H
