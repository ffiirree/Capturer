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
    auto CTRAL_ALT_A = new QxtGlobalShortcut(this);
    CTRAL_ALT_A->setShortcut(QKeySequence("Ctrl+Alt+S"));

    connect(CTRAL_ALT_A, &QxtGlobalShortcut::activated, capturer_, &ScreenCapturer::start);

    auto F3 = new QxtGlobalShortcut(this);
    F3->setShortcut(QKeySequence("F3"));
    connect(F3, &QxtGlobalShortcut::activated, this, &MainWindow::fixLastImage);

    auto CTRAL_ALT_V = new QxtGlobalShortcut(this);
    CTRAL_ALT_V->setShortcut(QKeySequence("Ctrl+Alt+V"));
    connect(CTRAL_ALT_V, &QxtGlobalShortcut::activated, recorder_, &ScreenRecorder::record);

    gifcptr_ = new GifCapturer();
    auto CTRAL_ALT_G = new QxtGlobalShortcut(this);
    CTRAL_ALT_G->setShortcut(QKeySequence("Ctrl+Alt+G"));
    connect(CTRAL_ALT_G, &QxtGlobalShortcut::activated, gifcptr_, &GifCapturer::record);
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
