#include "capturer.h"
#include <QKeyEvent>
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include "imagewindow.h"
#include "webview.h"

Capturer::Capturer(QWidget *parent)
    : QWidget(parent)
{
    sniper_ = new ScreenShoter(this);
    recorder_ = new ScreenRecorder(this);
    gifcptr_ = new GifCapturer(this);

    connect(sniper_, &ScreenShoter::FIX_IMAGE, this, &Capturer::pinImage);
    connect(sniper_, &ScreenShoter::SNIPPED, [&](const QPixmap image, const QPoint& pos) {
        clipboard_history_.push({image, pos});
    });

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

    // show message
    connect(sniper_, &ScreenShoter::SHOW_MESSAGE, this, &Capturer::showMessage);
    connect(recorder_, &ScreenRecorder::SHOW_MESSAGE, this, &Capturer::showMessage);
    connect(gifcptr_, &GifCapturer::SHOW_MESSAGE, this, &Capturer::showMessage);

    // clipboard
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &Capturer::clipboardChanged);
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

    auto screen_shot = new QAction(QIcon(":/icon/res/screenshot"), "Screen Shot", this);
    connect(screen_shot, &QAction::triggered, sniper_, &ScreenShoter::start);
    sys_tray_icon_menu_->addAction(screen_shot);

    auto record_screen = new QAction(QIcon(":/icon/res/stop"),"Screen Record", this);
    connect(record_screen, &QAction::triggered, recorder_, &ScreenRecorder::record);
    sys_tray_icon_menu_->addAction(record_screen);

    auto gif = new QAction(QIcon(":/icon/res/gif"), "GIF", this);
    connect(gif, &QAction::triggered, gifcptr_, &GifCapturer::record);
    sys_tray_icon_menu_->addAction(gif);

    sys_tray_icon_menu_->addSeparator();

    auto setting = new QAction(QIcon(":/icon/res/setting"), "Setting", this);
    connect(setting, &QAction::triggered, setting_dialog_, &SettingWindow::show);
    sys_tray_icon_menu_->addAction(setting);

    sys_tray_icon_menu_->addSeparator();

    auto exit_action = new QAction(QIcon(":/icon/res/exit"), "Quit", this);
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
}

void Capturer::setSnipHotKey(const QKeySequence &sc)
{
    if(!snip_sc_->setShortcut(sc)) {
        auto msg = "截屏快捷键<" + sc.toString() + ">注册失败!";
        sys_tray_icon_->showMessage("Capturer", msg, QSystemTrayIcon::Critical);
        LOG(ERROR) << msg;
    }
}

void Capturer::setFixImgHotKey(const QKeySequence &sc)
{
    if(!pin_sc_->setShortcut(sc)){
        auto msg = "贴图快捷键<" + sc.toString() + ">注册失败!";
        sys_tray_icon_->showMessage("Capturer", msg, QSystemTrayIcon::Critical);
        LOG(ERROR) << msg;
    }
}

void Capturer::setGIFHotKey(const QKeySequence &sc)
{
    if(!gif_sc_->setShortcut(sc)){
        auto msg = "GIF快捷键<" + sc.toString() + ">注册失败!";
        sys_tray_icon_->showMessage("Capturer", msg, QSystemTrayIcon::Critical);
        LOG(ERROR) << msg;
    }
}

void Capturer::setVideoHotKey(const QKeySequence &sc)
{
    if(!video_sc_->setShortcut(sc)){
        auto msg = "录屏快捷键<" + sc.toString() + ">注册失败!";
        sys_tray_icon_->showMessage("Capturer", msg, QSystemTrayIcon::Critical);
        LOG(ERROR) << msg;
    }
}

void Capturer::clipboardChanged()
{
    const auto mimedata = QApplication::clipboard()->mimeData();

    if(mimedata->hasFormat("application/x-qt-image"))  return;

    if(mimedata->hasHtml()) {
        auto view = new WebView();

#if (QT_VERSION > 0x050700)
        connect(view, &WebView::contentsSizeChanged, [&, view](const QSize& size){
            view->resize(size);
#else
        connect(view, &WebView::loadFinished, [&, view](){

            view->page()->runJavaScript("document.documentElement.scrollWidth;",[=](const QVariant& width){
                view->resize(width.toInt() + 10,view->height());
            });

            view->page()->runJavaScript("document.documentElement.scrollHeight;",[=](const QVariant& height){
                view->resize(view->width(), height.toInt());
            });
#endif
            QTimer::singleShot(100, [&, view](){
                clipboard_history_.push({ view->grab(), QCursor::pos() });
                view->close();
            });
        });

        view->setHtml(mimedata->html());
        view->show();
    }
    else if(mimedata->hasText()) {
        auto label = new QLabel(mimedata->text());
        label->setWordWrap(true);

        QPalette palette = label->palette();
        palette.setColor(label->backgroundRole(), Qt::white);
        label->setPalette(palette);
        label->setMargin(10);

        QFont font;
        font.setPointSize(12);
        font.setFamily("Consolas");
        label->setFont(font);

        clipboard_history_.push({ label->grab(), QCursor::pos() });
    }
    else if(mimedata->hasImage()) {
        QPixmap p;
        p.loadFromData(mimedata->data("image/ *"), "PNG");
        clipboard_history_.push({ p, QCursor::pos() });
    }
    else if(mimedata->hasUrls()) {
        // Do nothing.
    }
    else if(mimedata->hasColor()) {
        // Do nothing.
    }
}

void Capturer::pinImage(const QPixmap& image, const QPoint& pos)
{
    auto fixed_image = new ImageWindow(this);
    fixed_image->fix(image);
    fixed_image->move(pos - QPoint{ 5, 5 }/*window margin*/);
}

void Capturer::pinLastImage()
{
    if(clipboard_history_.empty()) return;

    const auto& last = clipboard_history_.back();
    pinImage(last.first, last.second);
}
