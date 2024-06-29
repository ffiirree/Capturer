#ifndef CAPTURER_WIDTH_BUTTON_H
#define CAPTURER_WIDTH_BUTTON_H

#include <QCheckBox>

class WidthButton final : public QCheckBox
{
    Q_OBJECT

public:
    explicit WidthButton(bool checkable = false, QWidget *parent = nullptr);

    [[nodiscard]] int value() const { return width_; }

signals:
    void changed(int);

public slots:
    void setValue(int width, bool silence = true);

protected:
    void wheelEvent(QWheelEvent *) override;
    void paintEvent(QPaintEvent *) override;

private:
    [[nodiscard]] int MAX() const { return (width() / 2 - 3) * 3; }

    int width_{ 6 };
};

#endif //! CAPTURER_WIDTH_BUTTON_H