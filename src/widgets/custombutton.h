#ifndef CUSTOM_BUTTON_H
#define CUSTOM_BUTTON_H

#include <QAbstractButton>

class CustomButton : public QAbstractButton
{
public:
    explicit CustomButton(QWidget* = nullptr);
    explicit CustomButton(const QSize&, bool = false, QWidget* = nullptr);

    virtual void paint(QPainter*) = 0;

    inline void normal(const QColor& icon, const QColor& bg = Qt::transparent) { icon_normal_color_ = icon, bg_normal_color_ = bg; update(); }
    inline void hover(const QColor& icon, const QColor& bg = Qt::transparent) { icon_hover_color_ = icon, bg_hover_color_ = bg; update(); }
    inline void checked(const QColor& icon, const QColor& bg = Qt::transparent) { icon_checked_color_ = icon, bg_checked_color_ = bg; update(); }

    inline QColor backgroundColor() const { return isChecked() ? bg_checked_color_ : ((hover_ && isEnabled()) ? bg_hover_color_ : bg_normal_color_); }
    inline QColor iconColor() const { return isChecked() ? icon_checked_color_ : ((hover_ && isEnabled()) ? icon_hover_color_ : icon_normal_color_); }

protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEvent*) override;
    void leaveEvent(QEvent*) override;

protected:
    bool hover_{ false };

    // icon
    QColor icon_normal_color_{ 0x38, 0x38, 0x38 };
    QColor icon_hover_color_{ 0x38, 0x38, 0x38 };
    QColor icon_checked_color_{ 0x38, 0x38, 0x38 };

    // background
    QColor bg_normal_color_{ Qt::transparent };
    QColor bg_hover_color_{ Qt::transparent };
    QColor bg_checked_color_{ Qt::transparent };
};

#endif // CUSTOM_BUTTON_H
