#ifndef CAPTURER_MAINWINDOW_H
#define CAPTURER_MAINWINDOW_H

#include <QFrame>
#include <QPixmap>
#include <QSystemTrayIcon>
#include <vector>
#include <QMenu>
#include <memory>
#include "screencapturer.h"
#include "fixedwindow.h"
#include "screenrecorder.h"
#include "qxtglobalshortcut.h"
#include "screencapturer.h"
#include "gifcapturer.h"
#include "settingdialog.h"

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void fixImage(QPixmap image);
    void fixLastImage();

private:
    void keyPressEvent(QKeyEvent *event);

    ScreenCapturer * capturer_ = nullptr;
    std::vector<QPixmap> images_;
    std::vector<FixImageWindow *> fix_windows_;

    ScreenRecorder * recorder_ = nullptr;
    GifCapturer * gifcptr_ = nullptr;

    QSystemTrayIcon *sys_tray_icon_ = nullptr;
    QMenu * sys_tray_icon_menu_ = nullptr;

    SettingDialog * setting_dialog_ = nullptr;
};

#endif // MAINWINDOW_H
