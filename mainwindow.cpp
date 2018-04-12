#include "mainwindow.h"
#include <QKeyEvent>
#include <QDebug>
#include "fixedwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    capturer_ = new ScreenCapturer();
    connect(capturer_, &ScreenCapturer::FIX_IMAGE, this, &MainWindow::fixImage);
    connect(capturer_, &ScreenCapturer::CAPTURE_SCREEN_DONE, [&](QPixmap image){ images_.push_back(image); });

    gifcptr_ = new GifCapturer();

    recorder_ = new ScreenRecorder();

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
    sys_tray_icon_menu_->addAction(setting);

    sys_tray_icon_menu_->addSeparator();

    auto exit_action = new QAction("Quit Capturer", this);
    connect(exit_action, &QAction::triggered, this, &MainWindow::close);
    sys_tray_icon_menu_->addAction(exit_action);

    sys_tray_icon_ = new QSystemTrayIcon(this);
    sys_tray_icon_->setContextMenu(sys_tray_icon_menu_);
    sys_tray_icon_->setIcon(QIcon(":/icon/res/icon.png"));
    setWindowIcon(QIcon(":/icon/res/icon.png"));
    sys_tray_icon_->show();

    // shortcut
    auto SCREEN_SHOT_SHORTCUT = new QxtGlobalShortcut(this);
    SCREEN_SHOT_SHORTCUT->setShortcut(QKeySequence("F1"));
    connect(SCREEN_SHOT_SHORTCUT, &QxtGlobalShortcut::activated, capturer_, &ScreenCapturer::start);

    auto FIX_LAST_IMAGE_SHORTCUT = new QxtGlobalShortcut(this);
    FIX_LAST_IMAGE_SHORTCUT->setShortcut(QKeySequence("F3"));
    connect(FIX_LAST_IMAGE_SHORTCUT, &QxtGlobalShortcut::activated, this, &MainWindow::fixLastImage);

    auto SCREEN_RECORDING_SHORTCUT = new QxtGlobalShortcut(this);
    SCREEN_RECORDING_SHORTCUT->setShortcut(QKeySequence("Ctrl+Alt+V"));
    connect(SCREEN_RECORDING_SHORTCUT, &QxtGlobalShortcut::activated, recorder_, &ScreenRecorder::record);

    auto GIF_SHORTCUT = new QxtGlobalShortcut(this);
    GIF_SHORTCUT->setShortcut(QKeySequence("Ctrl+Alt+G"));
    connect(GIF_SHORTCUT, &QxtGlobalShortcut::activated, gifcptr_, &GifCapturer::record);
}

void MainWindow::fixImage(QPixmap image)
{
    auto fixed_image = new FixImageWindow();
    fixed_image->fix(image);
    fix_windows_.push_back(fixed_image);
}

void MainWindow::fixLastImage()
{
    if(images_.empty()) return;

    fixImage(images_.back());
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    Q_UNUSED(event);
}

MainWindow::~MainWindow()
{
    delete capturer_;

    for(auto& fw: fix_windows_) {
        delete fw;
    }
}
