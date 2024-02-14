#include "magnifier.h"

#include "probe/graphics.h"

#include <fmt/format.h>
#include <QApplication>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QScreen>

Magnifier::Magnifier(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::ToolTip)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setVisible(false);

    // pixmap size
    psize_ = msize_ * alpha_;

    // label size
    label_ = new QLabel(this);
    label_->setObjectName("magnifier-color-label");
    label_->setGeometry(0, psize_.height(), psize_.width(), 55);
    label_->setAlignment(Qt::AlignCenter);

    // size
    setFixedSize(psize_.width(), psize_.height() + label_->height());

    // TODO: listen sytem level mouse move event, not just this APP
    qApp->installEventFilter(this);
}

QRect Magnifier::grabRect()
{
    auto mouse_pos = QCursor::pos();

    if (!pixmap_.isNull()) {
        auto offset  = probe::graphics::virtual_screen_geometry();
        mouse_pos   -= QPoint{ offset.x, offset.y };
    }

    return {
        mouse_pos.x() - msize_.width() / 2,
        mouse_pos.y() - msize_.height() / 2,
        msize_.width(),
        msize_.height(),
    };
}

QString Magnifier::colorname(ColorFormat format)
{
    auto fmt = (format == ColorFormat::AUTO) ? cfmt_ : format;
    switch (fmt) {
    case Magnifier::ColorFormat::INT:
        return QString::fromStdString(
            fmt::format("{:3d}, {:3d}, {:3d}", color_.red(), color_.green(), color_.blue()));

    case Magnifier::ColorFormat::FLT:
        return QString::fromStdString(
            fmt::format("{:4.2f}, {:4.2f}, {:4.2f}", color_.redF(), color_.greenF(), color_.blueF()));

    default:
        return QString::fromStdString(
            fmt::format("#{:2X}{:2X}{:2X}", color_.red(), color_.green(), color_.blue()));
    }
}

QPoint Magnifier::position()
{
    auto cx = QCursor::pos().x(), cy = QCursor::pos().y();
    auto rg = probe::graphics::virtual_screen_geometry();

    int mx = (rg.right() - cx > width()) ? cx + 16 : cx - width() - 16;
    int my = (rg.bottom() - cy > height()) ? cy + 16 : cy - height() - 16;
    return { mx, my };
}

bool Magnifier::eventFilter(QObject *obj, QEvent *event)
{
    // FIXME: grab the system-wide mouse event
    if (event->type() == QEvent::MouseMove) {
        update();
        move(position());
    }
    return qApp->eventFilter(obj, event);
}

// TODO: clear the background when close
void Magnifier::showEvent(QShowEvent *) { move(position()); }

void Magnifier::setGrabPixmap(const QPixmap& pixmap) { pixmap_ = pixmap; }

QPixmap Magnifier::grab()
{
    auto rect = grabRect();
    if (pixmap_.isNull()) {
        return QGuiApplication::primaryScreen()->grabWindow(
            probe::graphics::virtual_screen().handle, rect.x(), rect.y(), rect.width(), rect.height());
    }
    else {
        return pixmap_.copy(rect);
    }
}

void Magnifier::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    // 0.
    auto draw = grab().scaled(psize_, Qt::KeepAspectRatioByExpanding);

    // 1.
    painter.fillRect(rect(), QColor(0, 0, 0, 150));
    painter.drawPixmap(0, 0, draw);

    // 2.
    painter.setPen(QPen(QColor(0, 100, 250, 125), 5, Qt::SolidLine, Qt::FlatCap));

    painter.drawLine(QPoint(0, psize_.height() / 2), QPoint(psize_.width(), psize_.height() / 2));
    painter.drawLine(QPoint(psize_.width() / 2, 0), QPoint(psize_.width() / 2, psize_.height()));

    // 3.
    color_ = QColor(draw.toImage().pixel(psize_.width() / 2, psize_.height() / 2));
    painter.setPen(QPen(color_, 5, Qt::SolidLine, Qt::FlatCap));
    painter.drawLine(QPoint(psize_.width() / 2 - 2, psize_.height() / 2),
                     QPoint(psize_.width() / 2 + 3, psize_.height() / 2));

    // 4. border
    painter.setPen(QPen(Qt::black));
    painter.drawRect(0, 0, psize_.width() - 1, psize_.height() - 1);
    painter.setPen(QPen(Qt::white));
    painter.drawRect(1, 1, psize_.width() - 3, psize_.height() - 3);
    painter.end();

    // 5. text
    auto text = QString("(%1, %2)\n").arg(QCursor::pos().x()).arg(QCursor::pos().y()) + colorname();
    label_->setText(text);
}

void Magnifier::closeEvent(QCloseEvent *event)
{
    pixmap_ = {};
    QWidget::closeEvent(event);
}