#include "titlebar.h"

#include <fmt/format.h>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QStyle>
#include <QWindow>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <windowsx.h>
#endif

TitleBar::TitleBar(FramelessWindow *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground);
    setAutoFillBackground(true);

    auto layout = new QHBoxLayout();
    layout->setSpacing(0);
    layout->setContentsMargins({});
    setLayout(layout);

    // icon & title
    auto icon = new QCheckBox(parent->windowTitle());
    icon->setAttribute(Qt::WA_TransparentForMouseEvents);
    icon->setObjectName("icon-title");
    layout->addWidget(icon);

    // blank
    layout->addStretch();

    // minimize button
    auto min_btn = new QCheckBox(this);
    min_btn->setObjectName("min-btn");
    min_btn->setCheckable(false);
    connect(min_btn, &QPushButton::clicked, parent, &QWidget::showMinimized);
    layout->addWidget(min_btn, 0, Qt::AlignTop);

    // maximize button
    auto max_btn = new QCheckBox(this);
    max_btn->setObjectName("max-btn");
    max_btn->setCheckable(true);
    connect(max_btn, &QPushButton::toggled, parent, &FramelessWindow::maximize);
    connect(parent, &FramelessWindow::maximized, [=]() { max_btn->setChecked(true); });
    connect(parent, &FramelessWindow::normalized, [=]() { max_btn->setChecked(false); });
    connect(parent, &FramelessWindow::fullscreened, this, &QWidget::hide);
    connect(parent, &FramelessWindow::normalized, this, &QWidget::show);
    layout->addWidget(max_btn, 0, Qt::AlignTop);

    // close button
    auto close_btn = new QCheckBox(this);
    close_btn->setObjectName("close-btn");
    close_btn->setCheckable(false);
    connect(close_btn, &QPushButton::clicked, parent, &QWidget::close);
    layout->addWidget(close_btn, 0, Qt::AlignTop);

    // title
    connect(parent, &QWidget::windowTitleChanged, icon, &QCheckBox::setText);
}

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (!parent()->isFullScreen()) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
        parent()->windowHandle()->startSystemMove();
#elif defined(Q_OS_WIN)
        if (parent() && ReleaseCapture())
            SendMessage(reinterpret_cast<HWND>(parent()->winId()), WM_SYSCOMMAND, SC_MOVE + HTCAPTION, 0);
#else
        if (event->button() == Qt::LeftButton) {
            begin_  = event->globalPos() - parent()->pos();
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
        parent()->move(event->globalPos() - begin_);
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

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event) { parent()->maximize(!parent()->isMaximized()); }
