#include "capturer.h"
#include <QKeyEvent>
#include <QApplication>
#include <QClipboard>
#include <QTextEdit>
#include <QScrollBar>
#include "logging.h"
#include "imagewindow.h"

#define SET_HOTKEY(X, Y)    st(                                 \
                                if(!X->setShortcut(Y, true))          \
                                    showMessage("Capturer", tr("Failed to register shortcut <%1>").arg(Y.toString()), QSystemTrayIcon::Critical); \
                            )

Capturer::Capturer(QWidget *parent)
    : QWidget(parent)
{
    LOG(INFO) << "initializing.";

    LOAD_QSS(this, ":/qss/capturer.qss");

    sniper_ = new ScreenShoter(this);
    recorder_ = new ScreenRecorder(this);
    gifcptr_ = new GifCapturer(this);

    connect(sniper_, &ScreenShoter::FIX_IMAGE, [this](const QPixmap& image, const QPoint& pos) {
        auto window = new ImageWindow(image, pos, this);
        clipboard_history_.push(window);
        window->show();
    });

    sys_tray_icon_ = new QSystemTrayIcon(this);

    snip_sc_ = new QHotkey(this);
    connect(snip_sc_, &QHotkey::activated, sniper_, &ScreenShoter::start);

    pin_sc_ = new QHotkey(this);
    connect(pin_sc_, &QHotkey::activated, this, &Capturer::pinLastImage);

    show_pin_sc_ = new QHotkey(this);
    connect(show_pin_sc_, &QHotkey::activated, this, &Capturer::showImages);

    video_sc_ = new QHotkey(this);
    connect(video_sc_, &QHotkey::activated, recorder_, &ScreenRecorder::record);

    gif_sc_ = new QHotkey(this);
    connect(gif_sc_, &QHotkey::activated, gifcptr_, &GifCapturer::record);

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
    SET_HOTKEY(show_pin_sc_, config["pin"]["visiable"]["hotkey"].get<QKeySequence>());
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
    auto menu = new QMenu(this);
    menu->setObjectName("sys_tray_menu");

    menu->addAction(QIcon(":/icon/res/screenshot"), tr("Snip"), sniper_, &ScreenShoter::start);
    menu->addAction(QIcon(":/icon/res/sr"),         tr("Screen Record"), recorder_, &ScreenRecorder::record);
    menu->addAction(QIcon(":/icon/res/gif"),        tr("GIF Record"), gifcptr_, &GifCapturer::record);
    menu->addSeparator();
    menu->addAction(QIcon(":/icon/res/setting"),    tr("Settings"), setting_dialog_, &SettingWindow::show);
    menu->addSeparator();
    menu->addAction(QIcon(":/icon/res/exit"),       tr("Quit"), qApp, &QCoreApplication::exit);

    sys_tray_icon_->setContextMenu(menu);
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
        LOG(INFO) << "IMAGE";

        auto image_rect = mimedata->imageData().value<QPixmap>().rect();
        image_rect.moveCenter(DisplayInfo::screens()[0]->geometry().center());
        clipboard_history_.push(new ImageWindow(mimedata->imageData().value<QPixmap>(), image_rect.topLeft(), this));
    }
    else if(mimedata->hasHtml()) {
        LOG(INFO) << "HTML";

        QTextEdit view;
        view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view.setLineWrapMode(QTextEdit::NoWrap);

        view.setHtml(mimedata->html());
        view.setFixedSize(view.document()->size().toSize());

        clipboard_history_.push(new ImageWindow(view.grab(), QCursor::pos(), this));
    }
    else if(mimedata->hasFormat("application/qpoint") && mimedata->hasImage()) {
        LOG(INFO) << "SNIP";

        auto pos = *reinterpret_cast<QPoint *>(mimedata->data("application/qpoint").data());
        for(const auto& item : clipboard_history_) {
            if(item->image().cacheKey() == mimedata->imageData().value<QPixmap>().cacheKey()) {
                return;
            }
        }
        clipboard_history_.push(new ImageWindow(mimedata->imageData().value<QPixmap>(), pos, this));
    }

    else if(mimedata->hasUrls() 
        && QString("jpg;jpeg;png;JPG;JPEG;PNG;bmp;BMP;ico;ICO;gif;GIF").contains(QFileInfo(mimedata->urls()[0].fileName()).suffix())) {
        LOG(INFO) << "IMAGE URL";

        QPixmap pixmap;
        pixmap.load(mimedata->urls()[0].toLocalFile());
        auto image_rect = pixmap.rect();
        image_rect.moveCenter(DisplayInfo::screens()[0]->geometry().center());
        clipboard_history_.push(new ImageWindow(pixmap, image_rect.topLeft(), this));
    }
    else if(mimedata->hasText()) {
        LOG(INFO) << "TEXT";

        auto label = new QLabel(mimedata->text());
        label->setWordWrap(true);
        label->setMargin(10);
        label->setStyleSheet("background-color:white");
        label->setFont({"Consolas", 12});

        clipboard_history_.push(new ImageWindow(label->grab(), QCursor::pos(), this));
    }
    else if(mimedata->hasColor()) {
        // Do nothing.
        LOG(WARNING) << "COLOR";
    }
}

void Capturer::pinLastImage()
{
    if(clipboard_history_.empty()) return;

    const auto& last = clipboard_history_.back();
    last->show();
}

void Capturer::showImages()
{
    images_visible_ = !images_visible_;
    for(auto& window: clipboard_history_) {
        images_visible_ ? window->show(false) : window->hide();
    }
}
