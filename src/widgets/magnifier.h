#ifndef CAPTURER_MAGNIFIER_H
#define CAPTURER_MAGNIFIER_H

#include <QLabel>

class Magnifier final : public QWidget
{
public:
    enum class ColorFormat : int
    {
        HEX,
        INT,
        FLT,
        AUTO,
    };

public:
    explicit Magnifier(QWidget *parent = nullptr);

    [[nodiscard]] QColor color() const { return color_; }

    [[nodiscard]] QString colorname(ColorFormat = ColorFormat::AUTO) const;

    void format(ColorFormat format) { cfmt_ = format; }

    [[nodiscard]] ColorFormat format() const { return cfmt_; }

    void toggleFormat()
    {
        cfmt_ = static_cast<ColorFormat>((static_cast<int>(cfmt_) + 1) % 3);
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
    [[nodiscard]] QRect   grabRect() const;
    [[nodiscard]] QPixmap grab() const;
    [[nodiscard]] QPoint  position() const;

    QPixmap desktop_{};

    QLabel *label_{};

    int    alpha_{ 5 };
    QSize  msize_{ 29, 29 };
    QSize  psize_{ 0, 0 };
    QColor color_;

    ColorFormat cfmt_{ ColorFormat::HEX };
};

#endif //! CAPTURER_MAGNIFIER_H
