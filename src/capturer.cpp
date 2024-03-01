#include "capturer.h"

#include "clipboard.h"
#include "color-window.h"
#include "config.h"
#include "image-window.h"
#include "libcap/devices.h"
#include "logging.h"
#include "settingdialog.h"
#include "videoplayer.h"

#include <probe/system.h>
#include <QApplication>
#include <QFileInfo>
#include <QKeyEvent>
#include <QScreen>
#include <QUrl>

#define SET_HOTKEY(X, Y)                                                                                   \
    if (!X->setShortcut(Y, true)) {                                                                        \
        LOG(WARNING) << "Failed to register hotkey : " << Y.toString().toStdString();                      \
        error += tr("Failed to register hotkey : <%1>\n").arg(Y.toString());                               \
    }

Capturer::Capturer(int& argc, char **argv)
    : QApplication(argc, argv)
{
    setWindowIcon(QIcon(":/icons/capturer"));

    snip_hotkey_       = new QHotkey(this);
    preview_hotkey_    = new QHotkey(this);
    toggle_hotkey_     = new QHotkey(this);
    video_hotkey_      = new QHotkey(this);
    gif_hotkey_        = new QHotkey(this);
    quicklook_hotkey_  = new QHotkey(this);
    transparent_input_ = new QHotkey(this);

    sniper_.reset(new ScreenShoter());
    recorder_.reset(new ScreenRecorder(ScreenRecorder::VIDEO));
    gifcptr_.reset(new ScreenRecorder(ScreenRecorder::GIF));

    connect(snip_hotkey_, &QHotkey::activated, sniper_.get(), &ScreenShoter::start);
    connect(preview_hotkey_, &QHotkey::activated, this, &Capturer::PreviewClipboard);
    connect(toggle_hotkey_, &QHotkey::activated, this, &Capturer::TogglePreviews);
    connect(video_hotkey_, &QHotkey::activated, recorder_.get(), &ScreenRecorder::record);
    connect(gif_hotkey_, &QHotkey::activated, gifcptr_.get(), &ScreenRecorder::record);
    connect(quicklook_hotkey_, &QHotkey::activated, this, &Capturer::QuickLook);
    connect(transparent_input_, &QHotkey::activated, this, &Capturer::TransparentPreviewInput);
    connect(sniper_.get(), &ScreenShoter::pinData, this, &Capturer::PreviewMimeData);
}

void Capturer::Init()
{
    clipboard::init();

    SystemTrayInit();

    UpdateHotkeys();

    UpdateScreenshotStyle();
    UPdateVideoRecordingStyle();
    UPdateGifRecordingStyle();

    SetTheme(config::definite_theme());

    config::on_theme_changed = [this](const QString& theme) { SetTheme(theme); };

    config::set_autorun(config::autorun);

    //
    // microphones: default or null
    auto asrc            = av::default_audio_source();
    config::devices::mic = asrc.value_or(av::device_t{}).id;

    // speakers: default or null
    auto asink               = av::default_audio_sink();
    config::devices::speaker = asink.value_or(av::device_t{}).id;

    LOG(INFO) << "initialized.";
}

void Capturer::SystemTrayInit()
{
    tray_.reset(new QSystemTrayIcon(QIcon(":/icons/capturer"), this));
    tray_->setToolTip("Capturer Settings");

    tray_menu_.reset(new Menu());
    tray_menu_->setObjectName("tray-menu");

    QString color = (config::definite_theme() == "dark") ? "light" : "dark";

    tray_snip_         = new QAction(QIcon(":/icons/screenshot-" + color), tr("Screenshot"));
    tray_record_video_ = new QAction(QIcon(":/icons/capture-" + color), tr("Record Video"));
    tray_record_gif_   = new QAction(QIcon(":/icons/gif-" + color), tr("Record GIF"));
    tray_open_camera_  = new QAction(QIcon(":/icons/camera-" + color), tr("Open Camera"));
    tray_settings_     = new QAction(QIcon(":/icons/setting-" + color), tr("Settings"));
    tray_exit_         = new QAction(QIcon(":/icons/exit-" + color), tr("Quit"));

    tray_menu_->addAction(tray_snip_);
    tray_menu_->addAction(tray_record_video_);
    tray_menu_->addAction(tray_record_gif_);
    tray_menu_->addSeparator();
    tray_menu_->addAction(tray_open_camera_);
    tray_menu_->addSeparator();
    tray_menu_->addAction(tray_settings_);
    tray_menu_->addSeparator();
    tray_menu_->addAction(tray_exit_);

    tray_->setContextMenu(tray_menu_.data());
    tray_->show();

    connect(tray_.data(), &QSystemTrayIcon::activated, this, &Capturer::TrayActivated);
    connect(tray_snip_, &QAction::triggered, sniper_.data(), &ScreenShoter::start);
    connect(tray_record_video_, &QAction::triggered, recorder_.data(), &ScreenRecorder::start);
    connect(tray_record_gif_, &QAction::triggered, gifcptr_.data(), &ScreenRecorder::start);
    connect(tray_open_camera_, &QAction::triggered, this, &Capturer::ToggleCamera);
    connect(tray_settings_, &QAction::triggered, this, &Capturer::OpenSettingsDialog);
    connect(tray_exit_, &QAction::triggered, qApp, &QCoreApplication::exit);
}

void Capturer::PreviewClipboard() { PreviewMimeData(clipboard::back(true)); }

void Capturer::PreviewMimeData(const std::shared_ptr<QMimeData>& mimedata)
{
    if (mimedata != nullptr) {

        FramelessWindow *preview{};

        if (mimedata->hasUrls() && mimedata->urls().size() == 1 && mimedata->urls()[0].isLocalFile() &&
            QFileInfo(mimedata->urls()[0].toLocalFile()).isFile() &&
            QString("gif;mp4;mkv;m2ts;avi;wmv")
                .split(';')
                .contains(QFileInfo(mimedata->urls()[0].fileName()).suffix(), Qt::CaseInsensitive)) {

            preview = new VideoPlayer();
            dynamic_cast<VideoPlayer *>(preview)->open(mimedata->urls()[0].toLocalFile().toStdString(), {});

            mimedata->setData(clipboard::MIME_TYPE_STATUS, "P");
        }
        else if (mimedata->hasColor()) {
            preview = new ColorWindow(mimedata);
        }
        else {
            preview = new ImageWindow(mimedata);
        }

        preview->setAttribute(Qt::WA_DeleteOnClose);
        preview->show();
        if (!mimedata->hasFormat(clipboard::MIME_TYPE_POINT)) {
            preview->move(QApplication::screenAt(QCursor::pos())->geometry().center() -
                          preview->rect().center());
        }
        previews_.push_back(preview);

        connect(preview, &FramelessWindow::closed, [this]() {
            previews_.erase(
                std::remove_if(previews_.begin(), previews_.end(), [](auto ptr) { return !ptr; }),
                previews_.end());
        });
    }
}

void Capturer::ToggleCamera()
{
    // close
    if (camera_) {
        camera_->close();
        return;
    }

    // open
    camera_ = new VideoPlayer();

    if (av::cameras().empty()) {
        LOG(WARNING) << "camera not found";
        return;
    }

    auto& name = config::devices::camera;
#ifdef _WIN32
    if (camera_->open("video=" + name, { { "format", "dshow" }, { "filters", "hflip" } }) < 0) {
#elif __linux__
    if (camera_->open(name, { { "format", "v4l2" }, { "filters", "hflip" } }) < 0) {
#endif
        LOG(ERROR) << "failed to open camera";
    }
}

void Capturer::OpenSettingsDialog()
{
    if (settings_window_) {
        settings_window_->activateWindow();
        return;
    }

    settings_window_ = new SettingWindow();
    settings_window_->show();
}

void Capturer::QuickLook()
{
#if _WIN32
    const auto actived = probe::graphics::active_window();
    if (!actived || actived->pname != "explorer.exe") return;

    const auto file = probe::graphics::explorer_focused(actived->handle);

    if (file.empty()) return;

    auto data = std::make_shared<QMimeData>();
    data->setUrls({ QUrl::fromLocalFile(QString::fromUtf8(file.c_str())) });

    PreviewMimeData(data);
#endif
}

void Capturer::TogglePreviews()
{
    if (previews_.empty()) return;

    bool visible = false; // current state
    for (const auto& win : previews_) {
        if (win && win->isVisible()) {
            visible = true;
            break;
        }
    }

    for (const auto& win : previews_) {
        if (win) {
            win->setVisible(!visible);
        }
    }
}

void Capturer::TransparentPreviewInput()
{
    for (const auto& win : previews_) {
        if (win && win->geometry().contains(QCursor::pos())) {
            win->toggleTransparentInput();
#if _WIN32
            if (win->windowOpacity() == 1.0) win->setWindowOpacity(0.99);
#endif
        }
    }
}

void Capturer::UpdateHotkeys()
{
    QString error = "";
    // clang-format off
    SET_HOTKEY(snip_hotkey_,        config::hotkeys::screenshot);
    SET_HOTKEY(preview_hotkey_,     config::hotkeys::preview);
    SET_HOTKEY(toggle_hotkey_,      config::hotkeys::toggle_previews);
    SET_HOTKEY(video_hotkey_,       config::hotkeys::record_video);
    SET_HOTKEY(gif_hotkey_,         config::hotkeys::record_gif);
#if _WIN32
    SET_HOTKEY(quicklook_hotkey_,   config::hotkeys::quick_look);
#endif
    SET_HOTKEY(transparent_input_,  config::hotkeys::transparent_input);
    // clang-format on
    if (!error.isEmpty()) ShowMessage("Capturer", error, QSystemTrayIcon::Critical);
}

void Capturer::TrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick && sniper_) sniper_->start();
}

void Capturer::ShowMessage(const QString& title, const QString& msg, QSystemTrayIcon::MessageIcon icon,
                           int msecs)
{
    tray_->showMessage(title, msg, icon, msecs);
}

void Capturer::SetTheme(const QString& theme)
{
    if (theme_ == theme || (theme != "dark" && theme != "light")) return;

    theme_ = theme;

    std::vector<QString> files{
        ":/stylesheets/capturer",       ":/stylesheets/capturer-" + theme,
        ":/stylesheets/menu",           ":/stylesheets/menu-" + theme,
        ":/stylesheets/settingswindow", ":/stylesheets/settingswindow-" + theme,
        ":/stylesheets/player",
    };

    QString style{};
    for (auto& qss : files) {

        QFile file(qss);
        file.open(QFile::ReadOnly);

        if (file.isOpen()) {
            style += file.readAll();
            file.close();
        }
    }
    setStyleSheet(style);

    // system tray menu icons
    QString color = (config::definite_theme() == "dark") ? "light" : "dark";

    tray_snip_->setIcon(QIcon(":/icons/screenshot-" + color));
    tray_record_video_->setIcon(QIcon(":/icons/capture-" + color));
    tray_record_gif_->setIcon(QIcon(":/icons/gif-" + color));
    tray_open_camera_->setIcon(QIcon(":/icons/camera-" + color));
    tray_settings_->setIcon(QIcon(":/icons/setting-" + color));
    tray_exit_->setIcon(QIcon(":/icons/exit-" + color));
}

void Capturer::UpdateScreenshotStyle()
{
    if (sniper_) sniper_->setStyle(config::snip::style);
}

void Capturer::UPdateVideoRecordingStyle()
{
    if (recorder_) recorder_->setStyle(config::recording::video::style);
}

void Capturer::UPdateGifRecordingStyle()
{
    if (gifcptr_) gifcptr_->setStyle(config::recording::gif::style);
}