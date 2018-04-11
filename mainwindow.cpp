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

    recorder_ = new ScreenRecorder();

    //@shortcut Ctrl+Alt+A
    auto SCREEN_SHOT_SHORTCUT = new QxtGlobalShortcut(this);
    SCREEN_SHOT_SHORTCUT->setShortcut(QKeySequence("F1"));
    connect(SCREEN_SHOT_SHORTCUT, &QxtGlobalShortcut::activated, capturer_, &ScreenCapturer::start);

    auto FIX_LAST_IMAGE_SHORTCUT = new QxtGlobalShortcut(this);
    FIX_LAST_IMAGE_SHORTCUT->setShortcut(QKeySequence("F3"));
    connect(FIX_LAST_IMAGE_SHORTCUT, &QxtGlobalShortcut::activated, this, &MainWindow::fixLastImage);

    auto SCREEN_RECORDING_SHORTCUT = new QxtGlobalShortcut(this);
    SCREEN_RECORDING_SHORTCUT->setShortcut(QKeySequence("Ctrl+Alt+V"));
    connect(SCREEN_RECORDING_SHORTCUT, &QxtGlobalShortcut::activated, recorder_, &ScreenRecorder::record);

    gifcptr_ = new GifCapturer();
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
