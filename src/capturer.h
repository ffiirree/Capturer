#ifndef CAPTURER_H
#define CAPTURER_H

#include "framelesswindow.h"
#include "qhotkey.h"
#include "screenrecorder.h"
#include "screenshoter.h"
#include "settingdialog.h"

#include <memory>
#include <QSystemTrayIcon>

class Capturer : public QWidget
{
    Q_OBJECT

public:
    explicit Capturer(QWidget *parent = nullptr);

private slots:
    void pin();
    void pinMimeData(const std::shared_ptr<QMimeData>&);
    void showImages();

    void updateConfig();

    void showMessage(const QString& title, const QString& msg,
                     QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information, int msecs = 10'000);

private:
    void setupSystemTray();

    ScreenShoter *sniper_{ nullptr };
    ScreenRecorder *recorder_{ nullptr };
    ScreenRecorder *gifcptr_{ nullptr };

    QSystemTrayIcon *sys_tray_icon_{ nullptr };

    std::shared_ptr<SettingWindow> setting_dialog_{};

    // hotkey
    QHotkey *snip_sc_{ nullptr };
    QHotkey *show_pin_sc_{ nullptr };
    QHotkey *pin_sc_{ nullptr };
    QHotkey *gif_sc_{ nullptr };
    QHotkey *video_sc_{ nullptr };

    std::list<FramelessWindow *> windows_{};
};

#endif // CAPTURER_H
