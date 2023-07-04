#include "capturer.h"

#include "clipboard.h"
#include "image-window.h"
#include "logging.h"
#include "menu.h"
#include "probe/system.h"

#include <QApplication>
#include <QFileInfo>
#include <QKeyEvent>

#define SET_HOTKEY(X, Y)                                                                                   \
    if (!X->setShortcut(Y, true)) {                                                                        \
        LOG(WARNING) << "Failed to register hotkey : " << Y.toString().toStdString();                      \
        error += tr("Failed to register hotkey : <%1>\n").arg(Y.toString());                               \
    }

Capturer::Capturer(QWidget *parent)
    : QWidget(parent)
{
    sniper_   = new ScreenShoter(this);
    recorder_ = new ScreenRecorder(ScreenRecorder::VIDEO, this);
    gifcptr_  = new ScreenRecorder(ScreenRecorder::GIF, this);

    connect(sniper_, &ScreenShoter::pinData, this, &Capturer::pinMimeData);

    clipboard::init();

    sys_tray_icon_ = new QSystemTrayIcon(QIcon(":/icons/capturer"), this);

    snip_sc_ = new QHotkey(this);
    connect(snip_sc_, &QHotkey::activated, sniper_, &ScreenShoter::start);

    pin_sc_ = new QHotkey(this);
    connect(pin_sc_, &QHotkey::activated, this, &Capturer::pin);

    show_pin_sc_ = new QHotkey(this);
    connect(show_pin_sc_, &QHotkey::activated, this, &Capturer::showImages);

    video_sc_ = new QHotkey(this);
    connect(video_sc_, &QHotkey::activated, recorder_, &ScreenRecorder::record);

    gif_sc_ = new QHotkey(this);
    connect(gif_sc_, &QHotkey::activated, gifcptr_, &ScreenRecorder::record);

    connect(&Config::instance(), &Config::changed, this, &Capturer::updateConfig);

    // setting
    setting_dialog_ = new SettingWindow(this);

    // System tray icon
    // @attention Must after setting.
    setupSystemTray();

    updateConfig();

    setWindowIcon(QIcon(":/icons/capturer"));

    // show message
    connect(sniper_, &ScreenShoter::SHOW_MESSAGE, this, &Capturer::showMessage);
    connect(recorder_, &ScreenRecorder::SHOW_MESSAGE, this, &Capturer::showMessage);
    connect(gifcptr_, &ScreenRecorder::SHOW_MESSAGE, this, &Capturer::showMessage);

    LOG(INFO) << "initialized.";
}

void Capturer::updateConfig()
{
    auto& config = Config::instance();

    QString error = "";
    // clang-format off
    SET_HOTKEY(snip_sc_,        config["snip"]["hotkey"].get<QKeySequence>());
    SET_HOTKEY(pin_sc_,         config["pin"]["hotkey"].get<QKeySequence>());
    SET_HOTKEY(show_pin_sc_,    config["pin"]["visible"]["hotkey"].get<QKeySequence>());
    SET_HOTKEY(gif_sc_,         config["gif"]["hotkey"].get<QKeySequence>());
    SET_HOTKEY(video_sc_,       config["record"]["hotkey"].get<QKeySequence>());
    // clang-format on
    if (!error.isEmpty()) showMessage("Capturer", error, QSystemTrayIcon::Critical);

    sniper_->updateTheme();
    recorder_->updateTheme();
    gifcptr_->updateTheme();
}

void Capturer::setupSystemTray()
{
    auto menu = new Menu(this);
    menu->setObjectName("tray-menu");

    // SystemTrayIcon
    auto update_tray_menu = [=, this]() {
        QString icon_color = (Config::theme() == "dark") ? "light" : "dark";
#ifdef __linux__
        if (probe::system::os_name().find("Ubuntu") != std::string::npos) {
            auto ver = probe::system::os_version();
            if (ver.major == 20 && ver.minor == 4)
                icon_color = "dark";  // ubuntu 2004, system trays are always light
            else if (ver.major == 18 && ver.minor == 4)
                icon_color = "light"; // ubuntu 1804, system trays are always dark
        }
#endif
        // clang-format off
        menu->clear();
        menu->addAction(QIcon(":/icons/screenshot-" + icon_color),   tr("Screenshot"),   sniper_, &ScreenShoter::start);
        menu->addAction(QIcon(":/icons/capture-" + icon_color),      tr("Record Video"), recorder_, &ScreenRecorder::record);
        menu->addAction(QIcon(":/icons/gif-" + icon_color),          tr("Record GIF"),   gifcptr_, &ScreenRecorder::record);
        menu->addSeparator();
        menu->addAction(QIcon(":/icons/camera-" + icon_color),       tr("Open Camera"),  recorder_, &ScreenRecorder::switchCamera);
        menu->addSeparator();
        menu->addAction(QIcon(":/icons/setting-" + icon_color),      tr("Settings"),     [this](){ setting_dialog_->show(); setting_dialog_->activateWindow(); });
        menu->addSeparator();
        menu->addAction(QIcon(":/icons/exit-" + icon_color),         tr("Quit"),         qApp, &QCoreApplication::exit);
        // clang-format on
    };

    // dark / light theme
    connect(&Config::instance(), &Config::theme_changed, update_tray_menu);
    update_tray_menu();

    sys_tray_icon_->setContextMenu(menu);
    sys_tray_icon_->show();
    sys_tray_icon_->setToolTip("Capturer Settings");

    connect(sys_tray_icon_, &QSystemTrayIcon::activated, [this](auto&& r) {
        if (r == QSystemTrayIcon::DoubleClick) sniper_->start();
    });
}

void Capturer::showMessage(const QString& title, const QString& msg, QSystemTrayIcon::MessageIcon icon,
                           int msecs)
{
    sys_tray_icon_->showMessage(title, msg, icon, msecs);
}

void Capturer::pin() { pinMimeData(clipboard::back(true)); }

void Capturer::pinMimeData(const std::shared_ptr<QMimeData>& mimedata)
{
    if (mimedata != nullptr) {
        auto win = new ImageWindow(mimedata, this);
        win->show();

        auto iter = windows_.insert(windows_.end(), win);
        connect(win, &ImageWindow::closed, [=, this]() { windows_.erase(iter); });
    }
}

void Capturer::showImages()
{
    bool visible = !windows_.empty() && windows_.front()->isVisible();
    for (auto& win : windows_) {
        win->setVisible(!visible);
    }
}
