#ifndef CAPTURER_MAINWINDOW_H
#define CAPTURER_MAINWINDOW_H

#include <QFrame>
#include <QPixmap>
#include <QSystemTrayIcon>
#include <vector>
#include <QMenu>
#include <memory>
#include "screenshoter.h"
#include "imagewindow.h"
#include "screenrecorder.h"
#include "qxtglobalshortcut.h"
#include "screenshoter.h"
#include "gifcapturer.h"
#include "settingdialog.h"
#include "json.hpp"

class Capturer : public QWidget
{
    Q_OBJECT

public:
    explicit Capturer(QWidget *parent = nullptr);
    ~Capturer();

    inline nlohmann::json settings() const { return setting_dialog_->settings(); }

private slots:
    void pinImage(QPixmap image);
    void pinLastImage();

    void setSnipHotKey(const QKeySequence&);
    void setFixImgHotKey(const QKeySequence&);
    void setGIFHotKey(const QKeySequence&);
    void setVideoHotKey(const QKeySequence&);

private:
    void keyPressEvent(QKeyEvent *event);

    void setupSystemTrayIcon();
    void registerHotKeys();

    ScreenShoter * sniper_ = nullptr;
    ScreenRecorder * recorder_ = nullptr;
    GifCapturer * gifcptr_ = nullptr;

    std::vector<QPixmap> images_;
    std::vector<ImageWindow *> fix_windows_;

    QSystemTrayIcon *sys_tray_icon_ = nullptr;
    QMenu * sys_tray_icon_menu_ = nullptr;

    SettingWindow * setting_dialog_ = nullptr;

    // hotkey
    QxtGlobalShortcut *snip_sc_ = nullptr;
    QxtGlobalShortcut *pin_sc_ = nullptr;
    QxtGlobalShortcut *gif_sc_ = nullptr;
    QxtGlobalShortcut *video_sc_ = nullptr;
};

#endif // MAINWINDOW_H
