#include "capturer.h"
#include <QKeyEvent>
#include <QDebug>
#include "imagewindow.h"

Capturer::Capturer(QWidget *parent)
    : QWidget(parent)
{
    sniper_ = new ScreenShoter();
    recorder_ = new ScreenRecorder();
    gifcptr_ = new GifCapturer();

    connect(sniper_, &ScreenShoter::FIX_IMAGE, this, &Capturer::pinImage);
    connect(sniper_, &ScreenShoter::CAPTURE_SCREEN_DONE, [&](QPixmap image){ images_.push_back(image); });

    sys_tray_icon_ = new QSystemTrayIcon(this);

    snip_sc_ = new QxtGlobalShortcut(this);
    connect(snip_sc_, &QxtGlobalShortcut::activated, sniper_, &ScreenShoter::start);

    pin_sc_ = new QxtGlobalShortcut(this);
    connect(pin_sc_, &QxtGlobalShortcut::activated, this, &Capturer::pinLastImage);

    video_sc_ = new QxtGlobalShortcut(this);
    connect(video_sc_, &QxtGlobalShortcut::activated, recorder_, &ScreenRecorder::record);

    gif_sc_ = new QxtGlobalShortcut(this);
    connect(gif_sc_, &QxtGlobalShortcut::activated, gifcptr_, &GifCapturer::record);

    connect(Config::Instance(), &Config::changed, this, &Capturer::updateConfig);
    updateConfig();

    // setting
    setting_dialog_ = new SettingWindow(this);

    // System tray icon
    // @attention Must after setting.
    setupSystemTrayIcon();
}

void Capturer::updateConfig()
{
    auto cfg = Config::Instance();
    setSnipHotKey(cfg->get<QKeySequence>(SNIP_HOTKEY));
    setFixImgHotKey(cfg->get<QKeySequence>(PIN_HOTKEY));
    setVideoHotKey(cfg->get<QKeySequence>(RECORD_HOTKEY));
    setGIFHotKey(cfg->get<QKeySequence>(GIF_HOTKEY));

    sniper_->setBorderColor(cfg->get<QColor>(SNIP_SBC));
    sniper_->setBorderWidth(cfg->get<int>(SNIP_SBW));
    sniper_->setBorderStyle(cfg->get<Qt::PenStyle>(SNIP_SBS));
    sniper_->setMaskColor(cfg->get<QColor>(SNIP_SMC));
    sniper_->setUseDetectWindow(cfg->get<bool>(SNIP_DW));

    recorder_->setBorderColor(cfg->get<QColor>(RECORD_SBC));
    recorder_->setBorderWidth(cfg->get<int>(RECORD_SBW));
    recorder_->setBorderStyle(cfg->get<Qt::PenStyle>(RECORD_SBS));
    recorder_->setMaskColor(cfg->get<QColor>(RECORD_SMC));
    recorder_->setUseDetectWindow(cfg->get<bool>(RECORD_DW));
    recorder_->setFramerate(cfg->get<int>(RECORD_FRAMERATE));

    gifcptr_->setBorderColor(cfg->get<QColor>(GIF_SBC));
    gifcptr_->setBorderWidth(cfg->get<int>(GIF_SBW));
    gifcptr_->setBorderStyle(cfg->get<Qt::PenStyle>(GIF_SBS));
    gifcptr_->setMaskColor(cfg->get<QColor>(GIF_SMC));
    gifcptr_->setUseDetectWindow(cfg->get<bool>(GIF_DW));
    gifcptr_->setFPS(cfg->get<int>(GIF_FPS));
}

void Capturer::setupSystemTrayIcon()
{
    // SystemTrayIcon
    sys_tray_icon_menu_ = new QMenu(this);

    auto screen_shot = new QAction("Screen Shot", this);
    connect(screen_shot, &QAction::triggered, sniper_, &ScreenShoter::start);
    sys_tray_icon_menu_->addAction(screen_shot);

    auto gif = new QAction("GIF", this);
    connect(gif, &QAction::triggered, gifcptr_, &GifCapturer::record);
    sys_tray_icon_menu_->addAction(gif);

    auto record_screen = new QAction("Record Screen", this);
    connect(record_screen, &QAction::triggered, recorder_, &ScreenRecorder::record);
    sys_tray_icon_menu_->addAction(record_screen);

    sys_tray_icon_menu_->addSeparator();

    auto setting = new QAction("Setting", this);
    connect(setting, &QAction::triggered, setting_dialog_, &SettingWindow::show);
    sys_tray_icon_menu_->addAction(setting);

    sys_tray_icon_menu_->addSeparator();

    auto exit_action = new QAction("Quit Capturer", this);
    connect(exit_action, &QAction::triggered, qApp, &QCoreApplication::exit);
    sys_tray_icon_menu_->addAction(exit_action);

    sys_tray_icon_->setContextMenu(sys_tray_icon_menu_);
    sys_tray_icon_->setIcon(QIcon(":/icon/res/icon.png"));
    setWindowIcon(QIcon(":/icon/res/icon.png"));
    sys_tray_icon_->show();
}

void Capturer::setSnipHotKey(const QKeySequence &sc)
{
    if(!snip_sc_->setShortcut(sc)) {
        auto msg = "截屏快捷键<" + sc.toString() + ">注册失败!";
        sys_tray_icon_->showMessage("Capturer", msg, QSystemTrayIcon::Critical);
    }
}

void Capturer::setFixImgHotKey(const QKeySequence &sc)
{
    if(!pin_sc_->setShortcut(sc)){
        auto msg = "贴图快捷键<" + sc.toString() + ">注册失败!";
        sys_tray_icon_->showMessage("Capturer", msg, QSystemTrayIcon::Critical);
    }
}

void Capturer::setGIFHotKey(const QKeySequence &sc)
{
    if(!gif_sc_->setShortcut(sc)){
        auto msg = "GIF快捷键<" + sc.toString() + ">注册失败!";
        sys_tray_icon_->showMessage("Capturer", msg, QSystemTrayIcon::Critical);
    }
}

void Capturer::setVideoHotKey(const QKeySequence &sc)
{
    if(!video_sc_->setShortcut(sc)){
        auto msg = "录屏快捷键<" + sc.toString() + ">注册失败!";
        sys_tray_icon_->showMessage("Capturer", msg, QSystemTrayIcon::Critical);
    }
}

void Capturer::pinImage(QPixmap image)
{
    auto fixed_image = new ImageWindow(this);
    fixed_image->fix(image);
    fix_windows_.push_back(fixed_image);
    fixed_image->show();
}

void Capturer::pinLastImage()
{
    if(images_.empty()) return;

    pinImage(images_.back());
}

Capturer::~Capturer()
{
    delete sniper_;
    delete recorder_;
    delete gifcptr_;
}
