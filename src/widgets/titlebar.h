#ifndef TITLE_BAR_H
#define TITLE_BAR_H

#include <QWidget>
#include <QLabel>
#include "iconbutton.h"

class TitleBar : public QWidget {
    Q_OBJECT
public:
    explicit TitleBar(QWidget *parent = nullptr);

    void setTitle(const QString& title) { title_label_->setText(title); }

signals:
    void close();
    void maximize();
    void minimize();
    void normal();
    void moved(const QPoint&);

protected:
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

private:
    QIcon icon_;
    QLabel * icon_label_ = nullptr;
    QLabel * title_label_ = nullptr;
    IconButton * close_btn_ = nullptr;

    QPoint begin_{ 0, 0 };
    bool moving = false;
    bool is_maximized_ = false;
};
#endif // TITLE_BAR_H
