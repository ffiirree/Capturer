#ifndef CAPTURER_CUSTOM_BUTTON_H
#define CAPTURER_CUSTOM_BUTTON_H

#include <QAbstractButton>
#include <QPainter>

class CustomButton : public QAbstractButton
{
public:
    CustomButton(QWidget *parent = nullptr);
    CustomButton(const QSize&, bool checkable = false, QWidget *parent = nullptr);

    virtual void paint(QPainter* painter) = 0;

    inline void setIconColor(const QColor& color)           { icon_color_ = icon_normal_color_ = color; update(); }
    inline void setIconHoverColor(const QColor& color)      { icon_hover_color_ = color; update(); }
    inline void setIconCheckedColor(const QColor& color)    { icon_checked_color_ = color; update(); }

    inline void setBackgroundColor(const QColor& color)         { bg_color_ = bg_normal_color_ = color; update(); }
    inline void setBackgroundHoverColor(const QColor& color)    { bg_hover_color_ = color; update(); }
    inline void setBackgroundCheckedColor(const QColor& color)  { bg_checked_color_ = color; update(); }

protected:
    virtual void paintEvent(QPaintEvent *) override;
    virtual void enterEvent(QEvent *event) override;
    virtual void leaveEvent(QEvent *event) override;

protected:
    QPainter painter_;

    // icon
    QColor icon_color_ = Qt::black;
    QColor icon_normal_color_ = icon_color_;
    QColor icon_hover_color_ = icon_color_;
    QColor icon_checked_color_ = icon_color_;

    // background
    QColor bg_color_ = Qt::transparent;
    QColor bg_normal_color_ = bg_color_;
    QColor bg_hover_color_ = bg_color_;
    QColor bg_checked_color_ = bg_color_;
};
#endif // CAPTURER_CUSTOM_BUTTON_H
