#ifndef WIDTH_BUTTON_H
#define WIDTH_BUTTON_H

#include <QCheckBox>

class WidthButton : public QCheckBox
{
    Q_OBJECT

    Q_PROPERTY(int width READ __r_attr_width WRITE __w_attr_width)

public:
    explicit WidthButton(bool checkable = false, QWidget* parent = nullptr)
        : QCheckBox(parent) 
    {
        setCheckable(checkable);
    }

    [[nodiscard]] int value() const { return width_; }

    void setMaxValue(int max) { max_ = max; }
    void setMinValue(int min) { min_ = min; }

signals:
    void changed(int);

public slots:
    // Don't emit changed signal
    void setValue(int width);

protected:
    void wheelEvent(QWheelEvent*) override;

    int __r_attr_width() const { return __attr_width; };
    void __w_attr_width(int w) { __attr_width = w; }

private:
    int width_{ 3 };
    
    int max_{ 16 };
    int min_{ 1 };

    int __attr_width{ 2 };
};

#endif // !WIDTH_BUTTON_H