#ifndef CAPTURER_H
#define CAPTURER_H

#include "camera-player.h"
#include "framelesswindow.h"
#include "menu.h"
#include "screenrecorder.h"
#include "screenshoter.h"
#include "settingdialog.h"

#include <memory>
#include <QApplication>
#include <qhotkey.h>
#include <QMimeData>
#include <QPointer>
#include <QScopedPointer>
#include <QSystemTrayIcon>

class Capturer final : public QApplication
{
    Q_OBJECT

public:
    Capturer(int& argc, char **argv);

    void Init();

public slots:
    void QuickLook();

    void PreviewClipboard();
    void PreviewMimeData(const std::shared_ptr<QMimeData>& mimedata);
    void TogglePreviews();
    void TransparentPreviewInput();

    void ToggleCamera();
    void OpenSettingsDialog();

    void UpdateHotkeys();

    void TrayActivated(QSystemTrayIcon::ActivationReason reason);
    void ShowMessage(const QString& title, const QString& msg,
                     QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information, int msecs = 10000);

    void SetTheme(const QString& theme);

    void UpdateScreenshotStyle();

    void RecordVideo();
    void RecordGIF();

private:
    void SystemTrayInit();

    QString theme_{};

    QPointer<QTranslator> sys_translator_{};
    QPointer<QTranslator> app_translator_{};

    QScopedPointer<QSystemTrayIcon> tray_{};
    QScopedPointer<Menu>            tray_menu_{};
    QPointer<QAction>               tray_snip_{};
    QPointer<QAction>               tray_record_video_{};
    QPointer<QAction>               tray_record_gif_{};
    QPointer<QAction>               tray_open_camera_{};
    QPointer<QAction>               tray_settings_{};
    QPointer<QAction>               tray_exit_{};

    QPointer<SettingWindow> settings_window_{};

    // hotkey
    QPointer<QHotkey> snip_hotkey_{};        // screenshot
    QPointer<QHotkey> repeat_snip_hotkey_{}; // screenshot
    QPointer<QHotkey> video_hotkey_{};       // video recording
    QPointer<QHotkey> gif_hotkey_{};         // gif recording
    QPointer<QHotkey> preview_hotkey_{};     // preview
    QPointer<QHotkey> quicklook_hotkey_{};   // Explorer window, Windows only
    QPointer<QHotkey> transparent_input_{};  // for preview window
    QPointer<QHotkey> toggle_hotkey_{};      // toggle previews

    QScopedPointer<ScreenShoter> sniper_{};
    QPointer<ScreenRecorder>     recorder_{};
    QPointer<ScreenRecorder>     gifcptr_{};
    QPointer<CameraPlayer>       camera_{};

    std::list<QPointer<FramelessWindow>> previews_{};
};

inline Capturer *App() { return dynamic_cast<Capturer *>(qApp); }

#endif // CAPTURER_H
