#include "capturer.h"
#include <QKeyEvent>
#include <QDebug>
#include "imagewindow.h"

Capturer::Capturer(QWidget *parent)
    : QWidget(parent)
{
    capturer_ = new ScreenCapturer();
    connect(capturer_, &ScreenCapturer::FIX_IMAGE, this, &Capturer::fixImage);
    connect(capturer_, &ScreenCapturer::CAPTURE_SCREEN_DONE, [&](QPixmap image){ images_.push_back(image); });

    recorder_ = new ScreenRecorder();

    // setting
    setting_dialog_ = new SettingDialog();
    connect(setting_dialog_, &SettingDialog::snipShortcutChanged, this, &Capturer::setSnipHotKey);
    connect(setting_dialog_, &SettingDialog::fixImgShortcutChanged, this, &Capturer::setFixImgHotKey);
    connect(setting_dialog_, &SettingDialog::gifShortcutChanged, this, &Capturer::setGIFHotKey);
    connect(setting_dialog_, &SettingDialog::videoShortcutChanged, this, &Capturer::setVideoHotKey);

    connect(setting_dialog_, &SettingDialog::borderColorChanged, this, &Capturer::setBorderColor);
    connect(setting_dialog_, &SettingDialog::borderWidthChanged, this, &Capturer::setBorderWidth);
    connect(setting_dialog_, &SettingDialog::borderStyleChanged, this, &Capturer::setBorderStyle);

    // shortcuts
    // @attention Must after setting.
    registerHotKeys();

    // System tray icon
    // @attention Must after setting.
    setupSystemTrayIcon();

    setBorderColor(GET_SETTING(["selector"]["border"]["color"]));
    setBorderWidth(settings()["selector"]["border"]["width"].get<int>());
    setBorderStyle(Qt::PenStyle(settings()["selector"]["border"]["style"].get<int>()));
}

void Capturer::setupSystemTrayIcon()
{
    // SystemTrayIcon
    sys_tray_icon_menu_ = new QMenu(this);

    auto screen_shot = new QAction("Screen Shot", this);
    connect(screen_shot, &QAction::triggered, capturer_, &ScreenCapturer::start);
    sys_tray_icon_menu_->addAction(screen_shot);

    auto gif = new QAction("GIF", this);
    connect(gif, &QAction::triggered, gifcptr_, &GifCapturer::record);
    sys_tray_icon_menu_->addAction(gif);

    auto record_screen = new QAction("Record Screen", this);
    connect(record_screen, &QAction::triggered, recorder_, &ScreenRecorder::record);
    sys_tray_icon_menu_->addAction(record_screen);

    sys_tray_icon_menu_->addSeparator();

    auto setting = new QAction("Setting", this);
    connect(setting, &QAction::triggered, setting_dialog_, &SettingDialog::show);
    sys_tray_icon_menu_->addAction(setting);

    sys_tray_icon_menu_->addSeparator();

    auto exit_action = new QAction("Quit Capturer", this);
    connect(exit_action, &QAction::triggered, this, &Capturer::close);
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
    snip_sc_->setShortcut(GET_SETTING(["hotkey"]["snip"]));
    connect(snip_sc_, &QxtGlobalShortcut::activated, capturer_, &ScreenCapturer::start);

    fix_sc_ = new QxtGlobalShortcut(this);
    fix_sc_->setShortcut(GET_SETTING(["hotkey"]["fix_image"]));
    connect(fix_sc_, &QxtGlobalShortcut::activated, this, &Capturer::fixLastImage);

    video_sc_ = new QxtGlobalShortcut(this);
    video_sc_->setShortcut(GET_SETTING(["hotkey"]["video"]));
    connect(video_sc_, &QxtGlobalShortcut::activated, recorder_, &ScreenRecorder::record);

    gifcptr_ = new GifCapturer(); // Must be there ????
    gif_sc_ = new QxtGlobalShortcut(this);
    gif_sc_->setShortcut(GET_SETTING(["hotkey"]["gif"]));
    connect(gif_sc_, &QxtGlobalShortcut::activated, gifcptr_, &GifCapturer::record);
}

void Capturer::setSnipHotKey(const QKeySequence &sc)
{
    snip_sc_->setShortcut(sc);
}

void Capturer::setFixImgHotKey(const QKeySequence &sc)
{
    fix_sc_->setShortcut(sc);
}

void Capturer::setGIFHotKey(const QKeySequence &sc)
{
    gif_sc_->setShortcut(sc);
}

void Capturer::setVideoHotKey(const QKeySequence &sc)
{
    video_sc_->setShortcut(sc);
}

void Capturer::setBorderColor(const QColor& c)
{
    capturer_->setBorderColor(c);
    gifcptr_->setBorderColor(c);
    recorder_->setBorderColor(c);
}

void Capturer::setBorderWidth(int w)
{
    capturer_->setBorderWidth(w);
    gifcptr_->setBorderWidth(w);
    recorder_->setBorderWidth(w);
}

void Capturer::setBorderStyle(Qt::PenStyle style)
{
    capturer_->setBorderStyle(style);
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
    delete capturer_;
    delete setting_dialog_;

    for(auto& fw: fix_windows_) {
        delete fw;
    }
}
