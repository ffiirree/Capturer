#ifndef WIDTH_BUTTON_H
#define WIDTH_BUTTON_H

#include <QCheckBox>

class WidthButton : public QCheckBox
{
    Q_OBJECT
public:
    explicit WidthButton(int width = 1, bool checkable = false, QWidget* parent = nullptr)
        : QCheckBox(parent) {
        setValue(width);
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

private:
    int width_{ 2 };
    int max_{ 25 };
    int min_{ 1 };
};

#endif // !WIDTH_BUTTON_H