#ifndef COLORBUTTON_H
#define COLORBUTTON_H

#include <QPushButton>
#include <QPainter>
#include <QColorDialog>

class ColorButton : public QPushButton
{
    Q_OBJECT

public:
    explicit ColorButton(QWidget *parent = nullptr);
    ColorButton(const QColor&, QWidget *parent = nullptr);
    ~ColorButton();

    inline QColor color() const  { return color_; }
    inline void color(const QColor& color) { color_ = color; }

signals:
    void changed(const QColor&);

protected:
    void paintEvent(QPaintEvent *);

private:
    QPainter painter_;

    QColorDialog *color_dialog_;
    QColor color_;
};

#endif // COLORBUTTON_H
