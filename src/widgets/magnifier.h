#ifndef MAGNIFIER_H
#define MAGNIFIER_H

#include <QWidget>
#include <QLabel>
#include <QPixmap>

class Magnifier : public QWidget
{
public:
    enum class ColorFormat {
        HEX, DEC
    };
public:
    explicit Magnifier(QWidget *parent = nullptr);

    inline void pixmap(const QPixmap& p) { pixmap_ = p; }
    QRect mrect();

    QColor getColor() const { return center_color_; }
    QString getColorStringValue();
    void colorFormat(ColorFormat format) { color_format_ = format; }
    ColorFormat colorFormat() const { return color_format_; }
    void toggleColorFormat() 
    { 
        color_format_ = (color_format_ == ColorFormat::HEX) ? ColorFormat::DEC : ColorFormat::HEX; 
        update(); 
    }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QLabel * label_{ nullptr };
    QPixmap pixmap_;

    int alpha_{ 5 };
    QSize msize_{ 31, 25 };
    QSize psize_{ 0, 0 };
    QColor center_color_;

    ColorFormat color_format_{ ColorFormat::HEX };
};

#endif // MAGNIFIER_H
