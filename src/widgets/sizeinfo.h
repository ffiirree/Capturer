#ifndef SIZE_INFO_H
#define SIZE_INFO_H

#include <QWidget>
#include <QLabel>
#include <QPainter>

class SizeInfoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SizeInfoWidget(QWidget *parent = nullptr);

    void size(const QSize& size);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QLabel * label_ = nullptr;
    QPainter painter_;
};

#endif // SIZE_INFO_H
