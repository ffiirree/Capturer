#include "screenshoter.h"
#include <cmath>
#include <QApplication>
#include <QDesktopWidget>
#include <QFileDialog>
#include <QClipboard>
#include <QMouseEvent>
#include <QShortcut>
#include <QDateTime>
#include <QMimeData>
#include "logging.h"

ScreenShoter::ScreenShoter(QWidget *parent)
    : Selector(parent)
{
    menu_ = new ImageEditMenu(this);
    canvas_ = new Canvas(menu_, this);

    magnifier_ = new Magnifier(this);

    connect(canvas_, &Canvas::changed, [this]() { update(); });
    connect(this, &ScreenShoter::locked, canvas_, &Canvas::enable);
    connect(this, &ScreenShoter::captured, canvas_, &Canvas::disable);

    menu_->installEventFilter(this);
    installEventFilter(canvas_);

    connect(menu_, &ImageEditMenu::save, this, &ScreenShoter::save);
    connect(menu_, &ImageEditMenu::ok,  this, &ScreenShoter::copy);
    connect(menu_, &ImageEditMenu::fix,  this, &ScreenShoter::pin);
    connect(menu_, &ImageEditMenu::exit, this, &ScreenShoter::exit);

    connect(menu_, &ImageEditMenu::graphChanged, [this](Graph graph) {
        if (graph != Graph::NONE) LOCKED();
    });

    // show menu
    connect(this, &ScreenShoter::captured, [this](){ menu_->show(); moveMenu(); });

    // move menu
    connect(this, &ScreenShoter::moved, this, &ScreenShoter::moveMenu);
    connect(this, &ScreenShoter::resized, this, &ScreenShoter::moveMenu);

    // shortcuts
    registerShortcuts();
}

void ScreenShoter::start()
{
    if(status_ != SelectorStatus::INITIAL) return;

    auto virtual_geometry = QGuiApplication::primaryScreen()->virtualGeometry();
    captured_screen_ = QGuiApplication::primaryScreen()->grabWindow(QApplication::desktop()->winId(),
                                                                    virtual_geometry.left(),
                                                                    virtual_geometry.top(),
                                                                    virtual_geometry.width(),
                                                                    virtual_geometry.height());
    canvas_->canvas(captured_screen_);
    moveMagnifier();

    Selector::start();
}

void ScreenShoter::exit()
{
    // prevent the window flinker
    box_.reset({ 0, 0, DisplayInfo::maxSize().width(), DisplayInfo::maxSize().height() });
    canvas_->reset();
    canvas_->disable();
    repaint();

    menu_->reset();
    menu_->hide();

    Selector::exit();
}

bool ScreenShoter::eventFilter(QObject * obj, QEvent * event)
{
    if (obj == menu_) {
        switch (event->type())
        {
        case QEvent::KeyPress:      keyPressEvent(static_cast<QKeyEvent*>(event));  return true;
        case QEvent::KeyRelease:    keyReleaseEvent(static_cast<QKeyEvent*>(event));return true;
        default:                                                                    return Selector::eventFilter(obj, event);
        }
    }
    else {
        return Selector::eventFilter(obj, event);
    }
}

void ScreenShoter::mouseMoveEvent(QMouseEvent* event)
{
    if (status_ == SelectorStatus::LOCKED) {
        setCursor(canvas_->getCursorShape());
    }

    moveMagnifier();
    Selector::mouseMoveEvent(event);
}

void ScreenShoter::keyPressEvent(QKeyEvent* event)
{
    // hotkey 'Space': move the selector while editing.
    if (status_ == SelectorStatus::LOCKED
        && event->key() == Qt::Key_Space && !event->isAutoRepeat()
        && canvas_->editing()) {
        CAPTURED();
    }

    Selector::keyPressEvent(event);
}

void ScreenShoter::keyReleaseEvent(QKeyEvent *event)
{
    // stop moving the selector while editing
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()
        && canvas_->editing()) {
        LOCKED();
    }

    Selector::keyReleaseEvent(event);
}

void ScreenShoter::mouseDoubleClickEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton && status_ >= SelectorStatus::CAPTURED) {
        snipped();

        exit();
    }

    Selector::mouseDoubleClickEvent(event);
}

void ScreenShoter::moveMenu()
{
    auto area = selected();
    auto right = area.right() - menu_->width() + 1;
    if(right < 0) right = 0;

    if(area.bottom() + (menu_->height() * 2 + 5/*margin*/) < rect().height()) {
        menu_->move(right, area.bottom() + 3);
        menu_->setSubMenuShowBelow();
    }
    else if(area.top() - (menu_->height() * 2 + 5) > 0) {
        menu_->move(right, area.top() - menu_->height() - 3);
        menu_->setSubMenuShowAbove();
    }
    else {
        menu_->move(right, area.bottom() - menu_->height() - 3);
        menu_->setSubMenuShowAbove();
    }
}

void ScreenShoter::moveMagnifier()
{
    if(status_ < SelectorStatus::CAPTURED || status_ == SelectorStatus::RESIZING) {
        magnifier_->pixmap(captured_screen_.copy(magnifier_->mrect()));
        magnifier_->show();

        auto cx = QCursor::pos().x(), cy = QCursor::pos().y();

        int mx = rect().right() - cx > magnifier_->width() ? cx + 10 : cx - magnifier_->width() - 10;
        int my = rect().bottom() - cy > magnifier_->height() ? cy + 10 : cy - magnifier_->height() - 10;
        magnifier_->move(mx, my);
    } else if (magnifier_->isVisible()) {
        magnifier_->hide();
    }
}

void ScreenShoter::paintEvent(QPaintEvent *event)
{
    // 2. window
    painter_.begin(this);
    painter_.drawPixmap(0, 0, canvas_->canvas());

    // 3. modifying
    canvas_->drawModifying(&painter_);
    canvas_->modified(PaintType::UNMODIFIED);
    painter_.end();

    Selector::paintEvent(event);
}

QPixmap ScreenShoter::snipped()
{
    auto mimedata = new QMimeData();
    auto position = selected().topLeft();
    auto&& image = snippedImage();
    mimedata->setData("application/qpoint", QByteArray().append(reinterpret_cast<char*>(&position), sizeof (QPoint)));
    mimedata->setImageData(QVariant(image));
    // Ownership of the data is transferred to the clipboard: https://doc.qt.io/qt-5/qclipboard.html#setMimeData
    QApplication::clipboard()->setMimeData(mimedata);

    history_.push_back({captured_screen_, selected(), canvas_->commands()});
    history_idx_ = history_.size() - 1;

    return image;
}

QPixmap ScreenShoter::snippedImage()
{
    return canvas_->canvas().copy(selected());
}

void ScreenShoter::save()
{
    QString default_filename = "Capturer_picture_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz") + ".png";
#ifdef _WIN32
    auto filename = QFileDialog::getSaveFileName(this,
                                                 tr("Save Image"),
                                                 save_path_ + QDir::separator() + default_filename,
                                                 "PNG(*.png);;JPEG(*.jpg *.jpeg);;BMP(*.bmp)");

    if(!filename.isEmpty()) {
        QFileInfo fileinfo(filename);
        save_path_ = fileinfo.absoluteDir().path();
        snippedImage().save(filename);
        emit SHOW_MESSAGE("Capturer<PICTURE>", "Path: " + filename);

        snipped();

        exit();
    }
#elif __linux__
    auto filename = save_path_ + QDir::separator() + default_filename;

    snippedImage().save(filename);
    emit SHOW_MESSAGE("Capturer<PICTURE>", "Path: " + filename);

    snipped();

    exit();
#endif
}

void ScreenShoter::copy()
{
    snipped();

    exit();
}

void ScreenShoter::pin()
{
    emit FIX_IMAGE(snipped(), { selected().topLeft() });

    exit();
}

void ScreenShoter::registerShortcuts()
{
    connect(new QShortcut(Qt::CTRL + Qt::Key_S, this), &QShortcut::activated, [this](){
        if(status_ == SelectorStatus::CAPTURED || status_ == SelectorStatus::LOCKED) {
            save();
        }
    });

    connect(new QShortcut(Qt::Key_P, this), &QShortcut::activated, [this](){
        if(status_ == SelectorStatus::CAPTURED || status_ == SelectorStatus::LOCKED) {
            pin();
        }
    });

    connect(new QShortcut(Qt::Key_Return, this), &QShortcut::activated, [this]() { copy(); exit(); });
    connect(new QShortcut(Qt::Key_Enter, this), &QShortcut::activated, [this]() { copy(); exit(); });

    connect(new QShortcut(Qt::Key_Escape, this), &QShortcut::activated, [this]() { exit(); });

    connect(new QShortcut(Qt::Key_Tab, this), &QShortcut::activated, [this]() {
        if (magnifier_->isVisible()) {
            magnifier_->toggleColorFormat();
        }
    });

    connect(new QShortcut(Qt::CTRL + Qt::Key_Z, this), &QShortcut::activated, canvas_, &Canvas::undo);
    connect(new QShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_Z, this), &QShortcut::activated, canvas_, &Canvas::redo);

    auto setCurrent = [this](const History& history) {
        captured_screen_ = history.image_;
        box_.reset(history.rect_);
        CAPTURED();
        LOCKED();
        canvas_->commands(history.commands_);
        LOCKED();
        repaint();
    };

    connect(new QShortcut(Qt::Key_PageUp, this), &QShortcut::activated, [=](){
        if(history_idx_ < history_.size()) {
            setCurrent(history_[history_idx_]);
            if(history_idx_ > 0) history_idx_--;
        }
    });

    connect(new QShortcut(Qt::Key_PageDown, this), &QShortcut::activated, [=](){
        if(history_idx_ < history_.size()) {
            setCurrent(history_[history_idx_]);
            if(history_idx_ < history_.size() - 1) history_idx_++;
        }
    });

    connect(new QShortcut(Qt::CTRL + Qt::Key_C, this), &QShortcut::activated, [=](){
        if(status_ < SelectorStatus::CAPTURED) {
            QApplication::clipboard()->setText(magnifier_->getColorStringValue());
        }
        else {
            canvas_->copy();
        }
    });

    connect(new QShortcut(Qt::CTRL + Qt::Key_V, this), &QShortcut::activated, canvas_, &Canvas::paste);

    connect(new QShortcut(Qt::Key_Delete, this), &QShortcut::activated, canvas_, &Canvas::remove);
}