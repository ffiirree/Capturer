#ifndef CAPTURER_MAINWINDOW_H
#define CAPTURER_MAINWINDOW_H

#include <vector>
#include <queue>
#include <memory>
#include <QPixmap>
#include <QSystemTrayIcon>
#include <QMimeData>
#include <QMenu>
#include "webview.h"
#include "screenshoter.h"
#include "imagewindow.h"
#include "screenrecorder.h"
#include "qxtglobalshortcut.h"
#include "screenshoter.h"
#include "gifcapturer.h"
#include "settingdialog.h"
#include "json.hpp"

template <typename T, int MAX_SIZE = 10>
class LimitSizeQueue : public std::queue<T> {
public:
    void push(const T& value) { std::queue<T>::push(value); if(this->size() > MAX_SIZE) this->pop(); }
    void push(T&& value) { std::queue<T>::push(value); if(this->size() > MAX_SIZE) this->pop();}
};

class Capturer : public QWidget
{
    Q_OBJECT

public:
    explicit Capturer(QWidget *parent = nullptr);
    ~Capturer();

private slots:
    void pinImage(const QPixmap& image, const QPoint& pos);
    void pinLastImage();

    void setSnipHotKey(const QKeySequence&);
    void setFixImgHotKey(const QKeySequence&);
    void setGIFHotKey(const QKeySequence&);
    void setVideoHotKey(const QKeySequence&);
    void updateConfig();

    void showMessage(const QString &title, const QString &msg,
                     QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information,
                     int msecs = 10000);

    void clipboardChanged();

private:
    void setupSystemTrayIcon();

    ScreenShoter * sniper_ = nullptr;
    ScreenRecorder * recorder_ = nullptr;
    GifCapturer * gifcptr_ = nullptr;

    LimitSizeQueue<std::pair<QPixmap, QPoint>, 10> clipboard_history_;

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
