#ifndef TITLE_BAR_H
#define TITLE_BAR_H

#include <QCheckBox>

class TitleBar : public QWidget {
    Q_OBJECT
public:
    explicit TitleBar(QWidget* = nullptr);

    void setTitle(const QString& title) { logo_->setText(title); }

signals:
    void close();
    void normal();
    void moved(const QPoint&);

protected:
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    QCheckBox* logo_{ nullptr };

    QPoint begin_{ 0, 0 };
    bool moving_{ false };
    bool is_maximized_{ false };
};
#endif // TITLE_BAR_H
