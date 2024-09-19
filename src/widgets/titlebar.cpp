#include "titlebar.h"

#include <fmt/format.h>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QShortcut>
#include <QStyle>
#include <QWindow>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#endif

static void SetWindowStayOnTop(QWidget *win, bool top = true)
{
    if (!win || !win->winId()) return;

#ifdef Q_OS_WIN
    ::SetWindowPos(reinterpret_cast<HWND>(win->winId()), top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE);
#else
    win->setWindowFlag(Qt::WindowStaysOnTopHint, top);
    win->show();
#endif
}

TitleBar::TitleBar(FramelessWindow *parent)
    : QWidget(parent), window_(parent)
{
    setAttribute(Qt::WA_StyledBackground);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    const auto layout = new QHBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins({});

    // icon
    icon_btn_ = new QPushButton();
    icon_btn_->setAttribute(Qt::WA_TransparentForMouseEvents);
    icon_btn_->setObjectName("icon");
    icon_btn_->setIcon(window_->windowIcon());
    layout->addWidget(icon_btn_, 0, Qt::AlignCenter);

    // title
    title_label_ = new QLabel(window_->windowTitle());
    title_label_->setObjectName("title");
    layout->addWidget(title_label_);

    // blank
    layout->addStretch();

    // pin button
    if (window_->windowFlags() & Qt::WindowStaysOnTopHint) {
        pin_btn_ = new QCheckBox(this);
        pin_btn_->setObjectName("pin-btn");
        pin_btn_->setCheckable(true);
        pin_btn_->setChecked(window_->windowFlags() & Qt::WindowStaysOnTopHint);
        connect(pin_btn_, &QCheckBox::toggled,
                [this](auto checked) { SetWindowStayOnTop(window_, checked); });
        layout->addWidget(pin_btn_, 0, Qt::AlignTop);
    }

    // minimize button
    if (window_->windowFlags() & Qt::WindowMinimizeButtonHint) {
        min_btn_ = new QCheckBox(this);
        min_btn_->setObjectName("min-btn");
        min_btn_->setCheckable(false);
        connect(min_btn_, &QCheckBox::clicked, window_, &QWidget::showMinimized);
        layout->addWidget(min_btn_, 0, Qt::AlignTop);
    }

    // maximize button
    if (window_->windowFlags() & Qt::WindowMaximizeButtonHint) {
        max_btn_ = new QCheckBox(this);
        max_btn_->setObjectName("max-btn");
        max_btn_->setCheckable(true);
        connect(max_btn_, &QCheckBox::clicked,
                [this](int) { window_->isMaximized() ? window_->showNormal() : window_->showMaximized(); });
        layout->addWidget(max_btn_, 0, Qt::AlignTop);
    }

    // fullscreen button
    if (window_->windowFlags() & Qt::WindowFullscreenButtonHint) {
        full_btn_ = new QCheckBox(this);
        full_btn_->setObjectName("full-btn");
        full_btn_->setCheckable(true);
        connect(full_btn_, &QCheckBox::clicked,
                [this] { window_->isFullScreen() ? window_->showNormal() : window_->showFullScreen(); });
        layout->addWidget(full_btn_, 0, Qt::AlignTop);
    }

    // close button
    if (window_->windowFlags() & Qt::WindowCloseButtonHint) {
        close_btn_ = new QCheckBox(this);
        close_btn_->setObjectName("close-btn");
        close_btn_->setCheckable(false);
        connect(close_btn_, &QCheckBox::clicked, window_, &QWidget::close);
        layout->addWidget(close_btn_, 0, Qt::AlignTop);
    }

    //
    connect(new QShortcut(Qt::Key_F11, window_), &QShortcut::activated, full_btn_, &QCheckBox::click);

    window_->installEventFilter(this);
}

bool TitleBar::isInSystemButtons(const QPoint& pos) const
{
    return (pin_btn_ && pin_btn_->geometry().contains(pos)) ||
           (min_btn_ && min_btn_->geometry().contains(pos)) ||
           (max_btn_ && max_btn_->geometry().contains(pos)) ||
           (full_btn_ && full_btn_->geometry().contains(pos)) ||
           (close_btn_ && close_btn_->geometry().contains(pos));
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *)
{
    if (max_btn_) max_btn_->click();
}

bool TitleBar::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == window_) {
        switch (event->type()) {
        case QEvent::WindowIconChange:
            if (icon_btn_) icon_btn_->setIcon(window_->windowIcon());
            break;
        case QEvent::WindowTitleChange:
            if (title_label_) title_label_->setText(window_->windowTitle());
            break;
        case QEvent::WindowStateChange:
            if (max_btn_) max_btn_->setChecked(window_->isMaximized());
            if (full_btn_) full_btn_->setChecked(window_->isFullScreen());

            if (window_->windowState() == Qt::WindowNoState && !isVisible()) show();
            break;
        default: break;
        }
    }

    return QWidget::eventFilter(obj, event);
}