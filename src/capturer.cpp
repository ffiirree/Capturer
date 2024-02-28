#include "capturer.h"

#include "clipboard.h"
#include "color-window.h"
#include "image-window.h"
#include "libcap/devices.h"
#include "logging.h"
#include "menu.h"
#include "probe/system.h"
#include "videoplayer.h"

#include <QApplication>
#include <QFileInfo>
#include <QKeyEvent>
#include <QScreen>

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
    setting_dialog_ = std::make_shared<SettingWindow>();

    updateConfig();

    // System tray icon
    // @attention Must after setting.
    setupSystemTray();

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
    const auto menu = new Menu(this);
    menu->setObjectName("tray-menu");

    // SystemTrayIcon
    auto update_tray_menu = [=, this]() {
        QString icon_color = (Config::theme() == "dark") ? "light" : "dark";
#ifdef __linux__
        if (probe::system::name().find("Ubuntu") != std::string::npos) {
            const auto ver = probe::system::version();
            if (ver.major == 20 && ver.minor == 4)
                icon_color = "dark";  // ubuntu 2004, system trays are always light
            else if (ver.major == 18 && ver.minor == 4)
                icon_color = "light"; // ubuntu 1804, system trays are always dark
        }
#endif
        // clang-format off
        menu->clear();
        menu->addAction(QIcon(":/icons/screenshot-" + icon_color),   tr("Screenshot"),   sniper_,   &ScreenShoter::start,    snip_sc_->shortcut());
        menu->addAction(QIcon(":/icons/capture-" + icon_color),      tr("Record Video"), recorder_, &ScreenRecorder::record, video_sc_->shortcut());
        menu->addAction(QIcon(":/icons/gif-" + icon_color),          tr("Record GIF"),   gifcptr_,  &ScreenRecorder::record, gif_sc_->shortcut());
        menu->addSeparator();
        menu->addAction(QIcon(":/icons/camera-" + icon_color),       tr("Open Camera"),  this,      &Capturer::openCamera);
        menu->addSeparator();
        menu->addAction(QIcon(":/icons/setting-" + icon_color),      tr("Settings"),     [this](){ setting_dialog_->show(); setting_dialog_->activateWindow(); });
        menu->addSeparator();
        menu->addAction(QIcon(":/icons/exit-" + icon_color),         tr("Quit"),         qApp,      &QCoreApplication::exit);
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

void Capturer::pin() { pinMimeData(clipboard::back(true)); }

void Capturer::pinMimeData(const std::shared_ptr<QMimeData>& mimedata)
{
    if (mimedata != nullptr) {
        std::list<FramelessWindow *>::iterator iter;

        if (mimedata->hasUrls() && mimedata->urls().size() == 1 &&
            QString("gif;mp4;mkv;m2ts;avi;wmv")
                .contains(QFileInfo(mimedata->urls()[0].fileName()).suffix(), Qt::CaseInsensitive)) {

            auto player = new VideoPlayer();
            player->open(mimedata->urls()[0].toLocalFile().toStdString(), {});

            mimedata->setData(clipboard::MIME_TYPE_STATUS, "P");

            iter = windows_.emplace(windows_.end(), player);
        }
        else if (mimedata->hasColor()) {
            auto win = new ColorWindow(mimedata);
            iter     = windows_.emplace(windows_.end(), win);
            win->show();
            win->move(QApplication::screenAt(QCursor::pos())->geometry().center() - win->rect().center());
        }
        else {
            auto win = new ImageWindow(mimedata);
            iter     = windows_.emplace(windows_.end(), win);

            win->show();
            if (!mimedata->hasFormat(clipboard::MIME_TYPE_POINT)) {
                win->move(QApplication::screenAt(QCursor::pos())->geometry().center() -
                          win->rect().center());
            }
        }

        connect(*iter, &FramelessWindow::closed, [=, this]() {
            (*iter)->deleteLater();
            windows_.erase(iter);
        });
    }
}

void Capturer::openCamera()
{
    const auto player = new VideoPlayer(); // delete on close

    if (av::cameras().empty()) {
        LOG(WARNING) << "camera not found";
        return;
    }

    auto camera = Config::instance()["devices"]["cameras"].get<std::string>();
#ifdef _WIN32
    if (!player->open("video=" + camera, { { "format", "dshow" }, { "filters", "hflip" } })) {
#elif __linux__
    if (!player->open(camera, { { "format", "v4l2" }, { "filters", "hflip" } })) {
#endif
        LOG(ERROR) << "failed to open camera";
    }
}

void Capturer::showImages()
{
    const bool visible = !windows_.empty() && windows_.front()->isVisible();
    for (const auto& win : windows_) {
        win->setVisible(!visible);
    }
}

void Capturer::showMessage(const QString& title, const QString& msg, QSystemTrayIcon::MessageIcon icon,
                           int msecs)
{
    sys_tray_icon_->showMessage(title, msg, icon, msecs);
}
