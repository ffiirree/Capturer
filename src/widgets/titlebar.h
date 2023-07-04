#ifndef CAPTURER_TITLE_BAR_H
#define CAPTURER_TITLE_BAR_H

#include <QCheckBox>

class TitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit TitleBar(QWidget *);

protected:
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

private:
    QCheckBox *icon_{ nullptr };

    QPoint begin_{ 0, 0 };
    bool moving_{ false };
};
#endif //! CAPTURER_TITLE_BAR_H
