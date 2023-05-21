#include "screenshoter.h"
#include <QApplication>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QDesktopWidget>
#endif
#include <QScreen>
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
    windows_detection_flags_ = probe::graphics::window_filter_t::visible | probe::graphics::window_filter_t::children;

    menu_ = new ImageEditMenu(this);
    canvas_ = new Canvas(menu_, this);

    magnifier_ = new Magnifier(this);

    connect(canvas_, &Canvas::changed, [this]() { update(); });
    connect(canvas_, &Canvas::cursorChanged, [this]() { setCursor(canvas_->getCursorShape()); });

    connect(this, &ScreenShoter::locked, canvas_, &Canvas::enable);
    connect(this, &ScreenShoter::captured, canvas_, &Canvas::disable);

    menu_->installEventFilter(this);
    installEventFilter(canvas_);

    connect(menu_, &ImageEditMenu::save, this, &ScreenShoter::save);
    connect(menu_, &ImageEditMenu::ok,  this, &ScreenShoter::copy);
    connect(menu_, &ImageEditMenu::pin,  this, &ScreenShoter::pin);
    connect(menu_, &ImageEditMenu::exit, this, &ScreenShoter::exit);

    connect(menu_, &ImageEditMenu::graphChanged, [this](Graph graph) {
        if (graph != Graph::NONE)
            LOCKED();
        else if (!canvas_->editing())
            CAPTURED();
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
    canvas_->pixmap(captured_screen_);
    Selector::start();

    moveMagnifier();
}

void ScreenShoter::exit()
{
    captured_screen_ = {};
    canvas_->reset();
    canvas_->disable();

    menu_->reset();
    menu_->hide();

    Selector::exit();
}

bool ScreenShoter::eventFilter(QObject * obj, QEvent * event)
{
    if (obj == menu_) {
        switch (event->type())
        {
        case QEvent::KeyPress:      keyPressEvent(dynamic_cast<QKeyEvent*>(event));  return true;
        case QEvent::KeyRelease:    keyReleaseEvent(dynamic_cast<QKeyEvent*>(event));return true;
        default:                    return Selector::eventFilter(obj, event);
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
        save2clipboard(snip(), false);
        exit();
    }

//    Selector::mouseDoubleClickEvent(event);
}

void ScreenShoter::moveMenu()
{
    auto area = selected();
    auto right = area.right() - menu_->width() + 1;
    if(right < geometry().left()) right = geometry().left();

    if(area.bottom() + (menu_->height() * 2 + 8/*margin*/) < geometry().bottom()) {
        menu_->move(right, area.bottom() + 6);
        menu_->setSubMenuShowBelow();
    }
    else if(area.top() - (menu_->height() * 2 + 8) > 0) {
        menu_->move(right, area.top() - menu_->height() - 6);
        menu_->setSubMenuShowAbove();
    }
    else {
        menu_->move(right, area.bottom() - menu_->height() - 6);
        menu_->setSubMenuShowAbove();
    }
}

void ScreenShoter::moveMagnifier()
{
    if(status_ < SelectorStatus::CAPTURED || status_ == SelectorStatus::RESIZING) {
        magnifier_->pixmap(captured_screen_.copy(magnifier_->mrect()));
        magnifier_->show();

        auto cx = QCursor::pos().x(), cy = QCursor::pos().y();

        int mx = (geometry().right() - cx > magnifier_->width()) ? cx + 10 : cx - magnifier_->width() - 10;
        int my = (geometry().bottom() - cy > magnifier_->height()) ? cy + 10 : cy - magnifier_->height() - 10;
        magnifier_->move(mx - geometry().left(), my - geometry().top());
    } else if (magnifier_->isVisible()) {
        magnifier_->hide();
    }
}

void ScreenShoter::paintEvent(QPaintEvent *event)
{
    // 2. window
    painter_.begin(this);
    painter_.drawPixmap(0, 0, canvas_->pixmap());

    // 3. modifying
    canvas_->drawModifying(&painter_);
    canvas_->modified(PaintType::UNMODIFIED);
    painter_.end();

    Selector::paintEvent(event);
}

QPixmap ScreenShoter::snip()
{
    history_.push_back(prey_);
    history_idx_ = history_.size() - 1;

    return canvas_->pixmap().copy(selected().translated(-QRect(probe::graphics::virtual_screen_geometry()).topLeft()));
}

void ScreenShoter::save2clipboard(const QPixmap& image, bool pinned)
{
    auto mimedata = new QMimeData();
    mimedata->setImageData(QVariant(image));
    mimedata->setData("application/x-snipped", QByteArray().append(pinned ? "pinned" : "copied"));
    mimedata->setImageData(QVariant(image));
    // Ownership of the data is transferred to the clipboard: https://doc.qt.io/qt-5/qclipboard.html#setMimeData
    QApplication::clipboard()->setMimeData(mimedata);
}

void ScreenShoter::save()
{
    QString default_filename = "Capturer_" + QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss_zzz") + ".png";
 
#ifdef _WIN32
    QString filename = QFileDialog::getSaveFileName(
        this,
        tr("Save Image"),
        save_path_ + QDir::separator() + default_filename,
        "PNG(*.png);;JPEG(*.jpg *.jpeg);;BMP(*.bmp)"
    );
#elif __linux__
    QString filename = save_path_ + QDir::separator() + default_filename;
#endif

    if (!filename.isEmpty()) {
        QFileInfo fileinfo(filename);
        save_path_ = fileinfo.absoluteDir().path();
        
        snip().save(filename);
        
        emit SHOW_MESSAGE("Capturer<PICTURE>", "Path: " + filename);
        exit();
    }
}

void ScreenShoter::copy()
{
    save2clipboard(snip(), false);

    exit();
}

void ScreenShoter::pin()
{
    auto snipped = snip();

    emit pinSnipped(snipped, { selected().topLeft() });

    save2clipboard(snipped, true);

    exit();
}

void ScreenShoter::registerShortcuts()
{
    connect(new QShortcut(Qt::CTRL | Qt::Key_S, this), &QShortcut::activated, [this](){
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

    connect(new QShortcut(Qt::CTRL | Qt::Key_Z, this), &QShortcut::activated, canvas_, &Canvas::undo);
    connect(new QShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_Z, this), &QShortcut::activated, canvas_, &Canvas::redo);

    connect(new QShortcut(Qt::Key_PageUp, this), &QShortcut::activated, [=, this](){
        if(history_idx_ < history_.size()) {
            select(history_[history_idx_]);
            CAPTURED();
            if(history_idx_ > 0) history_idx_--;
        }
    });

    connect(new QShortcut(Qt::Key_PageDown, this), &QShortcut::activated, [=, this](){
        if(history_idx_ < history_.size()) {
            select(history_[history_idx_]);
            CAPTURED();
            if(history_idx_ < history_.size() - 1) history_idx_++;
        }
    });

    connect(new QShortcut(Qt::CTRL | Qt::Key_C, this), &QShortcut::activated, [=, this](){
        if(status_ < SelectorStatus::CAPTURED) {
            QApplication::clipboard()->setText(magnifier_->getColorStringValue());
        }
        else {
            canvas_->copy();
        }
    });

    connect(new QShortcut(Qt::CTRL | Qt::Key_V, this), &QShortcut::activated, canvas_, &Canvas::paste);

    connect(new QShortcut(Qt::Key_Delete, this), &QShortcut::activated, canvas_, &Canvas::remove);
}