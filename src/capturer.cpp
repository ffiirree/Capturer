#include "capturer.h"

#include "logging.h"
#include "probe/system.h"

#include <QApplication>
#include <QClipboard>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMimeData>
#include <QPixmap>
#include <QTextEdit>

#define SET_HOTKEY(X, Y)                                                                                   \
    st(if (!X->setShortcut(Y, true)) {                                                                     \
        LOG(WARNING) << "Failed to register hotkey : " << Y.toString().toStdString();                      \
        error += tr("Failed to register hotkey : <%1>\n").arg(Y.toString());                               \
    })

Capturer::Capturer(QWidget *parent)
    : QWidget(parent)
{
    sniper_   = new ScreenShoter(this);
    recorder_ = new ScreenRecorder(ScreenRecorder::VIDEO, this);
    gifcptr_  = new ScreenRecorder(ScreenRecorder::GIF, this);

    connect(sniper_, &ScreenShoter::pinSnipped, this, &Capturer::pinPixmap);

    sys_tray_icon_ = new QSystemTrayIcon(QIcon(":/icon/res/capturer.png"), this);

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

    setWindowIcon(QIcon(":/icon/res/capturer.png"));

    // show message
    connect(sniper_, &ScreenShoter::SHOW_MESSAGE, this, &Capturer::showMessage);
    connect(recorder_, &ScreenRecorder::SHOW_MESSAGE, this, &Capturer::showMessage);
    connect(gifcptr_, &ScreenRecorder::SHOW_MESSAGE, this, &Capturer::showMessage);

    // clipboard
    connect(QApplication::clipboard(), &QClipboard::dataChanged, [this]() { clipboard_changed_ = true; });

    LOG(INFO) << "initialized.";
}

void Capturer::updateConfig()
{
    auto& config = Config::instance();

    QString error = "";
    SET_HOTKEY(snip_sc_, config["snip"]["hotkey"].get<QKeySequence>());
    SET_HOTKEY(pin_sc_, config["pin"]["hotkey"].get<QKeySequence>());
    SET_HOTKEY(show_pin_sc_, config["pin"]["visible"]["hotkey"].get<QKeySequence>());
    SET_HOTKEY(gif_sc_, config["gif"]["hotkey"].get<QKeySequence>());
    SET_HOTKEY(video_sc_, config["record"]["hotkey"].get<QKeySequence>());
    if (!error.isEmpty()) showMessage("Capturer", error, QSystemTrayIcon::Critical);

    sniper_->updateTheme();
    recorder_->updateTheme();
    gifcptr_->updateTheme();
}

void Capturer::setupSystemTray()
{
    auto menu = new QMenu(this);
    menu->setObjectName("tray-menu");
    menu->setWindowFlag(Qt::FramelessWindowHint);
    menu->setWindowFlag(Qt::NoDropShadowWindowHint);
    menu->setAttribute(Qt::WA_TranslucentBackground);

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
        menu->addAction(QIcon(":/icon/res/screenshot-" + icon_color),   tr("Screenshot"),   sniper_, &ScreenShoter::start);
        menu->addAction(QIcon(":/icon/res/capture-" + icon_color),      tr("Record Video"), recorder_, &ScreenRecorder::record);
        menu->addAction(QIcon(":/icon/res/gif-" + icon_color),          tr("Record GIF"),   gifcptr_, &ScreenRecorder::record);
        menu->addSeparator();
        menu->addAction(QIcon(":/icon/res/camera-" + icon_color),       tr("Open Camera"),  recorder_, &ScreenRecorder::switchCamera);
        menu->addSeparator();
        menu->addAction(QIcon(":/icon/res/setting-" + icon_color),      tr("Settings"),     setting_dialog_, &SettingWindow::show);
        menu->addSeparator();
        menu->addAction(QIcon(":/icon/res/exit-" + icon_color),         tr("Quit"),         qApp, &QCoreApplication::exit);
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

std::pair<DataFormat, std::any> Capturer::clipboard_data()
{
    const auto mimedata = QApplication::clipboard()->mimeData();

    if (mimedata->hasHtml() && mimedata->hasImage()) {
        return { DataFormat::PIXMAP, mimedata->imageData().value<QPixmap>() };
    }
    else if (mimedata->hasHtml()) {
        return { DataFormat::HTML, mimedata->html() };
    }
    else if (mimedata->hasFormat("application/x-snipped") && mimedata->hasImage()) {
        QString type = mimedata->data("application/x-snipped");
        return (type == "copied")
                   ? std::pair<DataFormat, std::any>{ DataFormat::PIXMAP,
                                                      mimedata->imageData().value<QPixmap>() }
                   : std::pair<DataFormat, std::any>{ DataFormat::UNKNOWN, nullptr };
    }
    else if (mimedata->hasUrls() &&
             QString("jpg;jpeg;png;bmp;ico;gif;svg")
                 .contains(QFileInfo(mimedata->urls()[0].fileName()).suffix(), Qt::CaseInsensitive)) {
        return { DataFormat::URLS, mimedata->urls() };
    }
    else if (mimedata->hasText()) {
        return { DataFormat::TEXT, mimedata->text() };
    }
    else if (mimedata->hasColor()) {
        return { DataFormat::COLOR, mimedata->colorData().value<QColor>() };
    }
    else {
        return { DataFormat::UNKNOWN, nullptr };
    }
}

std::pair<bool, QPixmap> Capturer::to_pixmap(const std::pair<DataFormat, std::any>& data_pair)
{
    auto& [type, buffer] = data_pair;

    switch (type) {
    case DataFormat::PIXMAP: return { true, std::any_cast<QPixmap>(buffer) };
    case DataFormat::HTML: {
        QTextEdit view;
        view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view.setLineWrapMode(QTextEdit::NoWrap);

        view.setHtml(std::any_cast<QString>(buffer));
        view.setFixedSize(view.document()->size().toSize());

        return { true, view.grab() };
    }
    case DataFormat::TEXT: {
        QLabel label(std::any_cast<QString>(buffer));
        label.setWordWrap(true);
        label.setMargin(10);
        label.setStyleSheet("background-color:white");
        label.setFont({ "Consolas", 12 });
        return { true, label.grab() };
    }
    case DataFormat::URLS: return { true, QPixmap(std::any_cast<QList<QUrl>>(buffer)[0].toLocalFile()) };
    default: LOG(WARNING) << "not support"; return { false, {} };
    }
}

void Capturer::pin()
{
    auto [fmt, value] = clipboard_data();
    if (clipboard_changed_) {
        auto [ok, pixmap] = to_pixmap({ fmt, value });
        if (ok) {
            history_.append({ fmt, value,
                              std::make_shared<ImageWindow>(
                                  pixmap, QRect(probe::graphics::displays()[0].geometry).center() -
                                              QPoint{ pixmap.width(), pixmap.height() } / 2) });
            pin_idx_ = history_.size() - 1;
        }
    }
    else {
        auto& [_1, _2, win] = history_[pin_idx_];
        if (win) {
            win->show();
        }
        pin_idx_ = std::clamp<size_t>(pin_idx_ - 1, 0, history_.size() - 1);
    }

    clipboard_changed_ = false;
}

void Capturer::pinPixmap(const QPixmap& image, const QPoint& pos)
{
    history_.append({ DataFormat::PIXMAP, image, std::make_shared<ImageWindow>(image, pos) });
    pin_idx_ = history_.size() - 1;
}

void Capturer::showImages()
{
    images_visible_ = !images_visible_;
    for (auto& [_1, _2, win] : history_) {
        images_visible_&& win ? win->show(false) : win->hide();
    }
}
