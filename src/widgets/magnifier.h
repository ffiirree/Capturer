#ifndef CAPTURER_MAGNIFIER_H
#define CAPTURER_MAGNIFIER_H

#include <QLabel>

class Magnifier : public QWidget
{
public:
    enum class ColorFormat
    {
        HEX,
        DEC
    };

public:
    explicit Magnifier(QWidget *parent = nullptr);

    QColor getColor() const { return center_color_; }

    QString getColorStringValue();

    void colorFormat(ColorFormat format) { color_format_ = format; }

    ColorFormat colorFormat() const { return color_format_; }

    void toggleColorFormat()
    {
        color_format_ = (color_format_ == ColorFormat::HEX) ? ColorFormat::DEC : ColorFormat::HEX;
        update();
    }

    // 
    void setGrabPixmap(const QPixmap&);

protected:
    bool eventFilter(QObject *, QEvent *) override;

    void showEvent(QShowEvent *) override;
    void paintEvent(QPaintEvent *) override;

    void closeEvent(QCloseEvent *) override;

private:
    QRect grabRect();
    QPixmap grab();
    QPoint position();

    QPixmap pixmap_{};

    QLabel *label_{};

    int alpha_{ 5 };
    QSize msize_{ 29, 29 };
    QSize psize_{ 0, 0 };
    QColor center_color_;

    ColorFormat color_format_{ ColorFormat::HEX };
};

#endif //! CAPTURER_MAGNIFIER_H
