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

    // setting
    setting_dialog_ = new SettingWindow();
    connect(setting_dialog_, &SettingWindow::snipShortcutChanged, this, &Capturer::setSnipHotKey);
    connect(setting_dialog_, &SettingWindow::pinImgShortcutChanged, this, &Capturer::setFixImgHotKey);
    connect(setting_dialog_, &SettingWindow::gifShortcutChanged, this, &Capturer::setGIFHotKey);
    connect(setting_dialog_, &SettingWindow::videoShortcutChanged, this, &Capturer::setVideoHotKey);

    connect(setting_dialog_, &SettingWindow::snipBorderColorChanged, sniper_, &ScreenShoter::setBorderColor);
    connect(setting_dialog_, &SettingWindow::snipBorderWidthChanged, sniper_, &ScreenShoter::setBorderWidth);
    connect(setting_dialog_, &SettingWindow::snipBorderStyleChanged, sniper_, &ScreenShoter::setBorderStyle);
    connect(setting_dialog_, &SettingWindow::snipMaskColorChanged, sniper_, &ScreenShoter::setMaskColor);
    connect(setting_dialog_, &SettingWindow::snipDetectWindowChanged, sniper_, &ScreenShoter::setUseDetectWindow);

    connect(setting_dialog_, &SettingWindow::recordBorderColorChanged, recorder_, &ScreenRecorder::setBorderColor);
    connect(setting_dialog_, &SettingWindow::recordBorderWidthChanged, recorder_, &ScreenRecorder::setBorderWidth);
    connect(setting_dialog_, &SettingWindow::recordBorderStyleChanged, recorder_, &ScreenRecorder::setBorderStyle);
    connect(setting_dialog_, &SettingWindow::recordMaskColorChanged, recorder_, &ScreenRecorder::setMaskColor);
    connect(setting_dialog_, &SettingWindow::recordDetectWindowChanged, recorder_, &ScreenRecorder::setUseDetectWindow);
    connect(setting_dialog_, SIGNAL(recordFramerateChanged(int)), recorder_, SLOT(framerate(int)));

    connect(setting_dialog_, &SettingWindow::gifBorderColorChanged, gifcptr_, &GifCapturer::setBorderColor);
    connect(setting_dialog_, &SettingWindow::gifBorderWidthChanged, gifcptr_, &GifCapturer::setBorderWidth);
    connect(setting_dialog_, &SettingWindow::gifBorderStyleChanged, gifcptr_, &GifCapturer::setBorderStyle);
    connect(setting_dialog_, &SettingWindow::gifMaskColorChanged, gifcptr_, &GifCapturer::setMaskColor);
    connect(setting_dialog_, &SettingWindow::gifDetectWindowChanged, gifcptr_, &GifCapturer::setUseDetectWindow);
    connect(setting_dialog_, SIGNAL(gifFPSChanged(int)), gifcptr_, SLOT(fps(int)));

    // System tray icon
    // @attention Must after setting.
    setupSystemTrayIcon();

    // shortcuts
    // @attention Must after setting.
    registerHotKeys();

    sniper_->setBorderColor(GET_SETTING(["snip"]["selector"]["border"]["color"]));
    sniper_->setBorderWidth(settings()["snip"]["selector"]["border"]["width"].get<int>());
    sniper_->setBorderStyle(Qt::PenStyle(settings()["snip"]["selector"]["border"]["style"].get<int>()));
    sniper_->setMaskColor(GET_SETTING(["snip"]["selector"]["mask"]["color"]));
    sniper_->setUseDetectWindow(settings()["snip"]["detectwindow"].get<bool>());

    recorder_->setBorderColor(GET_SETTING(["record"]["selector"]["border"]["color"]));
    recorder_->setBorderWidth(settings()["record"]["selector"]["border"]["width"].get<int>());
    recorder_->setBorderStyle(Qt::PenStyle(settings()["record"]["selector"]["border"]["style"].get<int>()));
    recorder_->framerate(settings()["record"]["params"]["framerate"].get<int>());
    recorder_->setMaskColor(GET_SETTING(["record"]["selector"]["mask"]["color"]));
    recorder_->setUseDetectWindow(settings()["record"]["detectwindow"].get<bool>());

    gifcptr_->setBorderColor(GET_SETTING(["gif"]["selector"]["border"]["color"]));
    gifcptr_->setBorderWidth(settings()["gif"]["selector"]["border"]["width"].get<int>());
    gifcptr_->setBorderStyle(Qt::PenStyle(settings()["gif"]["selector"]["border"]["style"].get<int>()));
    gifcptr_->fps(settings()["gif"]["params"]["fps"].get<int>());
    gifcptr_->setMaskColor(GET_SETTING(["gif"]["selector"]["mask"]["color"]));
    gifcptr_->setUseDetectWindow(settings()["gif"]["detectwindow"].get<bool>());
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

    sys_tray_icon_ = new QSystemTrayIcon(this);
    sys_tray_icon_->setContextMenu(sys_tray_icon_menu_);
    sys_tray_icon_->setIcon(QIcon(":/icon/res/icon.png"));
    setWindowIcon(QIcon(":/icon/res/icon.png"));
    sys_tray_icon_->show();
}

void Capturer::registerHotKeys()
{
    snip_sc_ = new QxtGlobalShortcut(this);
    setSnipHotKey(GET_SETTING(["snip"]["hotkey"]["snip"]));
    connect(snip_sc_, &QxtGlobalShortcut::activated, sniper_, &ScreenShoter::start);

    pin_sc_ = new QxtGlobalShortcut(this);
    setFixImgHotKey(GET_SETTING(["snip"]["hotkey"]["pin"]));
    connect(pin_sc_, &QxtGlobalShortcut::activated, this, &Capturer::pinLastImage);

    video_sc_ = new QxtGlobalShortcut(this);
    setVideoHotKey(GET_SETTING(["record"]["hotkey"]["record"]));
    connect(video_sc_, &QxtGlobalShortcut::activated, recorder_, &ScreenRecorder::record);

    gif_sc_ = new QxtGlobalShortcut(this);
    setGIFHotKey(GET_SETTING(["gif"]["hotkey"]["record"]));
    connect(gif_sc_, &QxtGlobalShortcut::activated, gifcptr_, &GifCapturer::record);
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
    auto fixed_image = new ImageWindow();
    fixed_image->fix(image);
    fix_windows_.push_back(fixed_image);
}

void Capturer::pinLastImage()
{
    if(images_.empty()) return;

    pinImage(images_.back());
}

void Capturer::keyPressEvent(QKeyEvent *event)
{
    Q_UNUSED(event);
}

Capturer::~Capturer()
{
    delete sniper_;
    delete setting_dialog_;

    for(auto& fw: fix_windows_) {
        delete fw;
    }
}
