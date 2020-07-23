#include "capturer.h"
#include <QKeyEvent>
#include <QApplication>
#include <QClipboard>
#include <QTextEdit>
#include <QScrollBar>
#include "logging.h"
#include "imagewindow.h"

#define SET_HOTKEY(X, Y)  st(if(!X->setShortcut(Y)) showMessage("Capturer", tr("Failed to register shortcut <%1>").arg(Y.toString()), QSystemTrayIcon::Critical);)

Capturer::Capturer(QWidget *parent)
    : QWidget(parent)
{
    LOG(INFO) << "initializing.";

    LOAD_QSS(this, ":/qss/capturer.qss");

    sniper_ = new ScreenShoter(this);
    recorder_ = new ScreenRecorder(this);
    gifcptr_ = new GifCapturer(this);

    connect(sniper_, &ScreenShoter::FIX_IMAGE, this, &Capturer::pinImage);

    sys_tray_icon_ = new QSystemTrayIcon(this);

    snip_sc_ = new QxtGlobalShortcut(this);
    connect(snip_sc_, &QxtGlobalShortcut::activated, sniper_, &ScreenShoter::start);

    pin_sc_ = new QxtGlobalShortcut(this);
    connect(pin_sc_, &QxtGlobalShortcut::activated, this, &Capturer::pinLastImage);

    video_sc_ = new QxtGlobalShortcut(this);
    connect(video_sc_, &QxtGlobalShortcut::activated, recorder_, &ScreenRecorder::record);

    gif_sc_ = new QxtGlobalShortcut(this);
    connect(gif_sc_, &QxtGlobalShortcut::activated, gifcptr_, &GifCapturer::record);

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
    connect(gifcptr_, &GifCapturer::SHOW_MESSAGE, this, &Capturer::showMessage);

    // clipboard
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &Capturer::clipboardChanged);

    LOG(INFO) << "initialized.";
}

void Capturer::updateConfig()
{
    auto &config = Config::instance();
    SET_HOTKEY(snip_sc_, config["snip"]["hotkey"].get<QKeySequence>());
    SET_HOTKEY(pin_sc_, config["pin"]["hotkey"].get<QKeySequence>());
    SET_HOTKEY(gif_sc_, config["gif"]["hotkey"].get<QKeySequence>());
    SET_HOTKEY(video_sc_, config["record"]["hotkey"].get<QKeySequence>());

    recorder_->setFramerate(config["record"]["framerate"].get<int>());
    gifcptr_->setFramerate(config["gif"]["framerate"].get<int>());

    sniper_->updateTheme();
    recorder_->updateTheme();
    gifcptr_->updateTheme();
}

void Capturer::setupSystemTrayIcon()
{
    // SystemTrayIcon
    sys_tray_icon_menu_ = new QMenu(this);
    sys_tray_icon_menu_->setObjectName("sys_tray_menu");

    auto screen_shot = new QAction(QIcon(":/icon/res/screenshot"), tr("Snip"), this);
    connect(screen_shot, &QAction::triggered, sniper_, &ScreenShoter::start);
    sys_tray_icon_menu_->addAction(screen_shot);

    auto record_screen = new QAction(QIcon(":/icon/res/sr"),tr("Screen Record"), this);
    connect(record_screen, &QAction::triggered, recorder_, &ScreenRecorder::record);
    sys_tray_icon_menu_->addAction(record_screen);

    auto gif = new QAction(QIcon(":/icon/res/gif"), tr("GIF Record"), this);
    connect(gif, &QAction::triggered, gifcptr_, &GifCapturer::record);
    sys_tray_icon_menu_->addAction(gif);

    sys_tray_icon_menu_->addSeparator();

    auto setting = new QAction(QIcon(":/icon/res/setting"), tr("Settings"), this);
    connect(setting, &QAction::triggered, setting_dialog_, &SettingWindow::show);
    sys_tray_icon_menu_->addAction(setting);

    sys_tray_icon_menu_->addSeparator();

    auto exit_action = new QAction(QIcon(":/icon/res/exit"), tr("Quit"), this);
    connect(exit_action, &QAction::triggered, qApp, &QCoreApplication::exit);
    sys_tray_icon_menu_->addAction(exit_action);

    sys_tray_icon_->setContextMenu(sys_tray_icon_menu_);
    sys_tray_icon_->setIcon(QIcon(":/icon/res/icon.png"));
    setWindowIcon(QIcon(":/icon/res/icon.png"));
    sys_tray_icon_->show();
}


void Capturer::showMessage(const QString &title, const QString &msg, QSystemTrayIcon::MessageIcon icon, int msecs)
{
    sys_tray_icon_->showMessage(title, msg, icon, msecs);

    if(icon == QSystemTrayIcon::Critical) LOG(ERROR) << msg;
}

void Capturer::clipboardChanged()
{
    const auto mimedata = QApplication::clipboard()->mimeData();

    // only image
    if(mimedata->hasHtml() && mimedata->hasImage()) {
        LOG(INFO) << "IAMGE";

        auto image_rect = mimedata->imageData().value<QPixmap>().rect();
        image_rect.moveCenter(DisplayInfo::screens()[0]->geometry().center());
        clipboard_history_.push({ mimedata->imageData().value<QPixmap>(), image_rect.topLeft()});
    }
    else if(mimedata->hasHtml()) {
        LOG(INFO) << "HTML";

        QTextEdit view;
        view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view.setLineWrapMode(QTextEdit::NoWrap);

        view.setHtml(mimedata->html());
        view.setFixedSize(view.document()->size().toSize());

        clipboard_history_.push({ view.grab(), QCursor::pos() });
    }
    else if(mimedata->hasFormat("application/qpoint") && mimedata->hasImage()) {
        LOG(INFO) << "SNIP";

        auto pos = *reinterpret_cast<QPoint *>(mimedata->data("application/qpoint").data());
        clipboard_history_.push({ mimedata->imageData().value<QPixmap>(), pos });
    }

    else if(mimedata->hasUrls() && QString("jpg;jpeg;png;JPG;JPEG;PNG;bmp;BMP").contains(QFileInfo(mimedata->urls()[0].fileName()).suffix())) {
        LOG(INFO) << "IMAGE URL";

        QPixmap pixmap;
        pixmap.load(mimedata->urls()[0].toLocalFile());
        auto image_rect = pixmap.rect();
        image_rect.moveCenter(DisplayInfo::screens()[0]->geometry().center());
        clipboard_history_.push({ pixmap, image_rect.topLeft()});
    }
    else if(mimedata->hasText()) {
        LOG(INFO) << "TEXT";

        auto label = new QLabel(mimedata->text());
        label->setWordWrap(true);
        label->setMargin(10);
        label->setStyleSheet("background-color:white");
        label->setFont({"Consolas", 12});

        clipboard_history_.push({ label->grab(), QCursor::pos() });
    }
    else if(mimedata->hasColor()) {
        // Do nothing.
        LOG(WARNING) << "COLOR";
    }
}

void Capturer::pinImage(const QPixmap& image, const QPoint& pos)
{
    auto fixed_image = new ImageWindow(this);
    fixed_image->fix(image);

    fixed_image->move(pos + QPoint{-10, -10});
}

void Capturer::pinLastImage()
{
    if(clipboard_history_.empty()) return;

    const auto& last = clipboard_history_.back();
    pinImage(last.first, last.second);
}
