#include "titlebar.h"

#include <fmt/format.h>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QShortcut>
#include <QStyle>
#include <QWindow>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <windowsx.h>
#endif

static void pinWindow(QWidget *win, bool top = true)
{
    if (!win || !win->winId()) return;

#ifdef Q_OS_WIN
    auto hwnd = reinterpret_cast<HWND>(win->winId());
    ::SetWindowPos(hwnd, top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
#else
    win->setWindowFlag(Qt::WindowStaysOnTopHint, top);
    win->show();
#endif
}

TitleBar::TitleBar(FramelessWindow *parent)
    : QWidget(parent),
      window_(parent)
{
    setAttribute(Qt::WA_StyledBackground);
    setAutoFillBackground(true);

    auto layout = new QHBoxLayout();
    layout->setSpacing(0);
    layout->setContentsMargins({});
    setLayout(layout);

    // icon & title
    auto icon = new QCheckBox(window_->windowTitle());
    icon->setAttribute(Qt::WA_TransparentForMouseEvents);
    icon->setObjectName("icon-title");
    layout->addWidget(icon);

    // blank
    layout->addStretch();

    // pin button
    if (window_->windowFlags() & Qt::WindowStaysOnTopHint) {
        auto pin_btn = new QCheckBox(this);
        pin_btn->setObjectName("pin-btn");
        pin_btn->setCheckable(true);
        pin_btn->setChecked(window_->windowFlags() & Qt::WindowStaysOnTopHint);
        connect(pin_btn, &QCheckBox::toggled, [=, this](auto checked) { pinWindow(window_, checked); });
        layout->addWidget(pin_btn, 0, Qt::AlignTop);
    }

    // minimize button
    if (window_->windowFlags() & Qt::WindowMinimizeButtonHint) {
        auto min_btn = new QCheckBox(this);
        min_btn->setObjectName("min-btn");
        min_btn->setCheckable(false);
        connect(min_btn, &QCheckBox::clicked, window_, &QWidget::showMinimized);
        layout->addWidget(min_btn, 0, Qt::AlignTop);
    }

    // maximize button
    if (window_->windowFlags() & Qt::WindowMaximizeButtonHint) {
        auto max_btn = new QCheckBox(this);
        max_btn->setObjectName("max-btn");
        max_btn->setCheckable(true);
        connect(max_btn, &QCheckBox::toggled, window_, &FramelessWindow::maximize);
        connect(window_, &FramelessWindow::maximized, [=]() { max_btn->setChecked(true); });
        connect(window_, &FramelessWindow::normalized, [=]() { max_btn->setChecked(false); });
        layout->addWidget(max_btn, 0, Qt::AlignTop);
    }

    // fullscreen button
    if (window_->windowFlags() & Qt::WindowFullscreenButtonHint) {
        auto full_btn = new QCheckBox(this);
        full_btn->setObjectName("full-btn");
        full_btn->setCheckable(true);
        connect(full_btn, &QCheckBox::toggled, window_, &FramelessWindow::fullscreen);
        connect(window_, &FramelessWindow::fullscreened, [=]() { full_btn->setChecked(true); });
        connect(window_, &FramelessWindow::normalized, [=]() { full_btn->setChecked(false); });
        layout->addWidget(full_btn, 0, Qt::AlignTop);
    }
    connect(window_, &FramelessWindow::fullscreened, [this]() { setVisible(!hide_on_fullscreen_); });
    connect(window_, &FramelessWindow::normalized, this, &QWidget::show);
    connect(new QShortcut(Qt::Key_F11, window_), &QShortcut::activated, window_,
            &FramelessWindow::toggleFullScreen);

    // close button
    if (window_->windowFlags() & Qt::WindowCloseButtonHint) {
        auto close_btn = new QCheckBox(this);
        close_btn->setObjectName("close-btn");
        close_btn->setCheckable(false);
        connect(close_btn, &QCheckBox::clicked, window_, &QWidget::close);
        layout->addWidget(close_btn, 0, Qt::AlignTop);
    }

    // title
    connect(window_, &QWidget::windowTitleChanged, icon, &QCheckBox::setText);
}

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (!window()->isFullScreen()) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
        window()->windowHandle()->startSystemMove();
#elif defined(Q_OS_WIN)
        if (window() && ReleaseCapture())
            SendMessage(reinterpret_cast<HWND>(window()->winId()), WM_SYSCOMMAND, SC_MOVE + HTCAPTION, 0);
#else
        if (event->button() == Qt::LeftButton) {
            begin_  = event->globalPos() - window()->pos();
            moving_ = true;
            setCursor(Qt::SizeAllCursor);
        }
#endif
    }
}

void TitleBar::mouseMoveEvent(QMouseEvent *event)
{
#ifndef Q_OS_WIN
    if ((event->buttons() & Qt::LeftButton) && moving_) {
        window()->move(event->globalPos() - begin_);
    }
#endif
}

void TitleBar::mouseReleaseEvent(QMouseEvent *)
{
#ifndef Q_OS_WIN
    if (moving_) {
        moving_ = false;
        setCursor(Qt::ArrowCursor);
    }
#endif
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *) { window()->toggleMaximized(); }
