#include "capturer.h"
#include <QMimeData>
#include <QPixmap>
#include <QKeyEvent>
#include <QApplication>
#include <QClipboard>
#include <QTextEdit>
#include <QScrollBar>
#include "logging.h"

#define SET_HOTKEY(X, Y)    st(if(!X->setShortcut(Y, true)) error += tr("Failed to register hotkey:<%1>\n").arg(Y.toString());)

Capturer::Capturer(QWidget *parent)
    : QWidget(parent)
{
    LOG(INFO) << "initializing.";

    sniper_ = new ScreenShoter(this);
    recorder_ = new ScreenRecorder(ScreenRecorder::VIDEO, this);
    gifcptr_ = new ScreenRecorder(ScreenRecorder::GIF, this);

    connect(sniper_, &ScreenShoter::pinSnipped, this, &Capturer::pinPixmap);

    sys_tray_icon_ = new QSystemTrayIcon(this);

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
    setupSystemTrayIcon();

    updateConfig();

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
    auto &config = Config::instance();

    QString error = "";
    SET_HOTKEY(snip_sc_, config["snip"]["hotkey"].get<QKeySequence>());
    SET_HOTKEY(pin_sc_, config["pin"]["hotkey"].get<QKeySequence>());
    SET_HOTKEY(show_pin_sc_, config["pin"]["visiable"]["hotkey"].get<QKeySequence>());
    SET_HOTKEY(gif_sc_, config["gif"]["hotkey"].get<QKeySequence>());
    SET_HOTKEY(video_sc_, config["record"]["hotkey"].get<QKeySequence>());
    if(!error.isEmpty())
        showMessage("Capturer", error, QSystemTrayIcon::Critical);

    recorder_->setFramerate(config["record"]["framerate"].get<int>());
    gifcptr_->setFramerate(config["gif"]["framerate"].get<int>());

    sniper_->updateTheme();
    recorder_->updateTheme();
    gifcptr_->updateTheme();
}

void Capturer::setupSystemTrayIcon()
{
    // SystemTrayIcon
    auto menu = new QMenu(this);
    menu->setWindowFlag(Qt::FramelessWindowHint);
    menu->setWindowFlag(Qt::NoDropShadowWindowHint);
    menu->setAttribute(Qt::WA_TranslucentBackground);

    QString theme = Config::instance()["theme"].get<QString>() == "dark" ? "light" : "dark";

    menu->addAction(QIcon(":/icon/res/screenshot-" + theme), tr("Screenshot"), sniper_, &ScreenShoter::start);
    menu->addAction(QIcon(":/icon/res/capture-" + theme),    tr("Record Video"), recorder_, &ScreenRecorder::record);
    menu->addAction(QIcon(":/icon/res/gif-" + theme),        tr("Record GIF"), gifcptr_, &ScreenRecorder::record);
    menu->addSeparator();
    menu->addAction(QIcon(":/icon/res/camera-" + theme), tr("Open Camera"), recorder_, &ScreenRecorder::switchCamera);
    menu->addSeparator();
    menu->addAction(QIcon(":/icon/res/setting-" + theme),    tr("Settings"), setting_dialog_, &SettingWindow::show);
    menu->addSeparator();
    menu->addAction(QIcon(":/icon/res/exit-" + theme),       tr("Quit"), qApp, &QCoreApplication::exit);

    sys_tray_icon_->setContextMenu(menu);
    sys_tray_icon_->setIcon(QIcon(":/icon/res/icon.png"));
    setWindowIcon(QIcon(":/icon/res/icon.png"));
    sys_tray_icon_->show();

    connect(sys_tray_icon_, &QSystemTrayIcon::activated, [this](auto&& r) { if (r == QSystemTrayIcon::DoubleClick) sniper_->start(); });
}

void Capturer::showMessage(const QString &title, const QString &msg, QSystemTrayIcon::MessageIcon icon, int msecs)
{
    sys_tray_icon_->showMessage(title, msg, icon, msecs);

    if(icon == QSystemTrayIcon::Critical) LOG(ERROR) << msg;
}

std::pair<DataFormat, std::any> Capturer::clipboard_data()
{
    const auto mimedata = QApplication::clipboard()->mimeData();

    if(mimedata->hasHtml() && mimedata->hasImage()) {
        return { DataFormat::PIXMAP, mimedata->imageData().value<QPixmap>() };
    }
    else if(mimedata->hasHtml()) {
        return { DataFormat::HTML, mimedata->html() };
    }
    else if(mimedata->hasFormat("application/x-snipped") && mimedata->hasImage()) {
        QString type = mimedata->data("application/x-snipped");
        return (type == "copied") ? 
            std::pair<DataFormat, std::any>{DataFormat::PIXMAP, mimedata->imageData().value<QPixmap>()} :
            std::pair<DataFormat, std::any>{DataFormat::UNKNOWN, nullptr};
    }
    else if(mimedata->hasUrls() 
        && QString("jpg;jpeg;png;bmp;ico;gif;svg").contains(QFileInfo(mimedata->urls()[0].fileName()).suffix(), Qt::CaseInsensitive)) {
        return { DataFormat::URLS, mimedata->urls() };
    }
    else if(mimedata->hasText()) {
        return { DataFormat::TEXT, mimedata->text() };
    }
    else if(mimedata->hasColor()) {
        return { DataFormat::COLOR, mimedata->colorData().value<QColor>() };
    } 
    else {
        return { DataFormat::UNKNOWN, nullptr };
    }
}

std::pair<bool, QPixmap> Capturer::to_pixmap(const std::pair<DataFormat, std::any>& data_pair)
{
    auto& [type, data] = data_pair;

    switch (type)
    {
    case DataFormat::PIXMAP: return { true,  std::any_cast<QPixmap>(data) };
    case DataFormat::HTML:
    {
        QTextEdit view;
        view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view.setLineWrapMode(QTextEdit::NoWrap);

        view.setHtml(std::any_cast<QString>(data));
        view.setFixedSize(view.document()->size().toSize());

        return { true, view.grab() };
    }
    case DataFormat::TEXT:
    {
        QLabel label(std::any_cast<QString>(data));
        label.setWordWrap(true);
        label.setMargin(10);
        label.setStyleSheet("background-color:white");
        label.setFont({ "Consolas", 12 });
        return { true, label.grab() };
    }
    case DataFormat::URLS: return { true, QPixmap(std::any_cast<QList<QUrl>>(data)[0].toLocalFile()) };
    default: LOG(WARNING) << "not support"; return { false, {} };
    }
}

void Capturer::pin()
{
    auto [fmt, data] = clipboard_data();
    if (clipboard_changed_) {
        auto [ok, pixmap] = to_pixmap({ fmt, data });
        if (ok) {
            history_.append({
                fmt,
                data,
                std::make_shared<ImageWindow>(
                    pixmap,
                    DisplayInfo::screens()[0]->geometry().center() - QPoint{ pixmap.width(), pixmap.height() } / 2
                )
            });
            pin_idx_ = history_.size() - 1;
        }
    }
    else {
        auto& [_1, _2, win] = history_[pin_idx_];
        if (win) {
            win->show();
        }
        pin_idx_ = std::clamp<size_t>(pin_idx_-1, 0, history_.size() - 1);
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
    for(auto& [_1, _2, win] : history_) {
        images_visible_ && win ? win->show(false) : win->hide();
    }
}
