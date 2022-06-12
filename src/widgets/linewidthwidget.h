#ifndef LINE_WIDTH_WIDGET_H
#define LINE_WIDTH_WIDGET_H

#include "custombutton.h"

class WidthButton : public CustomButton
{
    Q_OBJECT
public:
    explicit WidthButton(const QSize& size, int width = 1, bool checkable = false, QWidget* parent = nullptr)
        : CustomButton(size, checkable, parent), width_(width) {
        checkValue();
    }

    int value() const { return width_; }

    void setMaxValue(int max) { max_ = max; }
    void setMinValue(int min) { min_ = min; }

    virtual void paint(QPainter*) override;

signals:
    void changed(int);

public slots:
    // Don't emit changed signal
    void setValue(int width)
    {
        width_ = width;
        checkValue();
        update();
    }

protected:
    void wheelEvent(QWheelEvent*) override;

private:
    void checkValue()
    {
        if (width_ > max_) width_ = max_;
        if (width_ < min_) width_ = min_;
    }

    int width_{ 2 };
    int max_{ 25 };
    int min_{ 1 };
};

#endif // LINE_WIDTH_WIDGET_H
