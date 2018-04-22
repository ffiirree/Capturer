#include "capturer.h"
#include <QKeyEvent>
#include <QDebug>
#include "imagewindow.h"

Capturer::Capturer(QWidget *parent)
    : QWidget(parent)
{
    shoter_ = new ScreenShoter();
    recorder_ = new ScreenRecorder();
    gifcptr_ = new GifCapturer();

    connect(shoter_, &ScreenShoter::FIX_IMAGE, this, &Capturer::fixImage);
    connect(shoter_, &ScreenShoter::CAPTURE_SCREEN_DONE, [&](QPixmap image){ images_.push_back(image); });

    // setting
    setting_dialog_ = new SettingWindow();
    connect(setting_dialog_, &SettingWindow::snipShortcutChanged, this, &Capturer::setSnipHotKey);
    connect(setting_dialog_, &SettingWindow::fixImgShortcutChanged, this, &Capturer::setFixImgHotKey);
    connect(setting_dialog_, &SettingWindow::gifShortcutChanged, this, &Capturer::setGIFHotKey);
    connect(setting_dialog_, &SettingWindow::videoShortcutChanged, this, &Capturer::setVideoHotKey);

    connect(setting_dialog_, &SettingWindow::borderColorChanged, this, &Capturer::setBorderColor);
    connect(setting_dialog_, &SettingWindow::borderWidthChanged, this, &Capturer::setBorderWidth);
    connect(setting_dialog_, &SettingWindow::borderStyleChanged, this, &Capturer::setBorderStyle);

    // System tray icon
    // @attention Must after setting.
    setupSystemTrayIcon();

    // shortcuts
    // @attention Must after setting.
    registerHotKeys();

    setBorderColor(GET_SETTING(["selector"]["border"]["color"]));
    setBorderWidth(settings()["selector"]["border"]["width"].get<int>());
    setBorderStyle(Qt::PenStyle(settings()["selector"]["border"]["style"].get<int>()));
}

void Capturer::setupSystemTrayIcon()
{
    // SystemTrayIcon
    sys_tray_icon_menu_ = new QMenu(this);

    auto screen_shot = new QAction("Screen Shot", this);
    connect(screen_shot, &QAction::triggered, shoter_, &ScreenShoter::start);
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
    setSnipHotKey(GET_SETTING(["hotkey"]["snip"]));
    connect(snip_sc_, &QxtGlobalShortcut::activated, shoter_, &ScreenShoter::start);

    fix_sc_ = new QxtGlobalShortcut(this);
    setFixImgHotKey(GET_SETTING(["hotkey"]["fix_image"]));
    connect(fix_sc_, &QxtGlobalShortcut::activated, this, &Capturer::fixLastImage);

    video_sc_ = new QxtGlobalShortcut(this);
    setVideoHotKey(GET_SETTING(["hotkey"]["video"]));
    connect(video_sc_, &QxtGlobalShortcut::activated, recorder_, &ScreenRecorder::record);

    gif_sc_ = new QxtGlobalShortcut(this);
    setGIFHotKey(GET_SETTING(["hotkey"]["gif"]));
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
    if(!fix_sc_->setShortcut(sc)){
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

void Capturer::setBorderColor(const QColor& c)
{
    shoter_->setBorderColor(c);
    gifcptr_->setBorderColor(c);
    recorder_->setBorderColor(c);
}

void Capturer::setBorderWidth(int w)
{
    shoter_->setBorderWidth(w);
    gifcptr_->setBorderWidth(w);
    recorder_->setBorderWidth(w);
}

void Capturer::setBorderStyle(Qt::PenStyle style)
{
    shoter_->setBorderStyle(style);
    gifcptr_->setBorderStyle(style);
    recorder_->setBorderStyle(style);
}

void Capturer::fixImage(QPixmap image)
{
    auto fixed_image = new ImageWindow();
    fixed_image->fix(image);
    fix_windows_.push_back(fixed_image);
}

void Capturer::fixLastImage()
{
    if(images_.empty()) return;

    fixImage(images_.back());
}

void Capturer::keyPressEvent(QKeyEvent *event)
{
    Q_UNUSED(event);
}

Capturer::~Capturer()
{
    delete shoter_;
    delete setting_dialog_;

    for(auto& fw: fix_windows_) {
        delete fw;
    }
}
