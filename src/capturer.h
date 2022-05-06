#ifndef CAPTURER_H
#define CAPTURER_H

#include <vector>
#include <queue>
#include <memory>
#include <QPixmap>
#include <QSystemTrayIcon>
#include <QMimeData>
#include <QMenu>
#include "screenshoter.h"
#include "imagewindow.h"
#include "screenrecorder.h"
#include "qhotkey.h"
#include "screenshoter.h"
#include "settingdialog.h"
#include "json.hpp"

template <typename T, int MAX_SIZE = 100>
class LimitSizeVector : public std::vector<T> {
public:
    void push(const T& value)
    {
        this->push_back(value);

        if(this->size() > MAX_SIZE) {
            this->erase(this->begin());
        }
    }
};

class Capturer : public QWidget
{
    Q_OBJECT

public:
    explicit Capturer(QWidget *parent = nullptr);
    ~Capturer() override = default;

private slots:
    void pinLastImage();
    void showImages();

    void updateConfig();

    void showMessage(const QString &title, const QString &msg,
                     QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information,
                     int msecs = 10000);

    void clipboardChanged();

private:
    void setupSystemTrayIcon();

    ScreenShoter * sniper_ = nullptr;
    ScreenRecorder * recorder_ = nullptr;
    ScreenRecorder* gifcptr_ = nullptr;

    LimitSizeVector<ImageWindow *> clipboard_history_;

    QSystemTrayIcon *sys_tray_icon_ = nullptr;

    SettingWindow * setting_dialog_ = nullptr;

    // hotkey
    QHotkey *snip_sc_ = nullptr;
    QHotkey *show_pin_sc_ = nullptr;
    QHotkey *pin_sc_ = nullptr;
    QHotkey *gif_sc_ = nullptr;
    QHotkey *video_sc_ = nullptr;

    bool images_visible_ = true;
};

#endif // CAPTURER_H
