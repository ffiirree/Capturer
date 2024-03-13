#ifndef CAPTURER_COLOR_PANEL_H
#define CAPTURER_COLOR_PANEL_H

#include <QPushButton>

class ColorButton : public QPushButton
{
    Q_OBJECT

public:
    explicit ColorButton(QWidget * = nullptr);
    explicit ColorButton(const QColor&, QWidget * = nullptr);

    void setColor(const QColor& c, bool silence = true);

    [[nodiscard]] QColor color() const { return color_; }

signals:
    void clicked(const QColor&);
    void changed(const QColor&);

protected:
    void paintEvent(QPaintEvent *) override;

protected:
    QColor color_{ Qt::blue };
};

///////////////////////////////////////////////////////////////////////////
class ColorDialogButton : public ColorButton
{
    Q_OBJECT

public:
    explicit ColorDialogButton(QWidget * = nullptr);
    explicit ColorDialogButton(const QColor&, QWidget * = nullptr);

protected:
    void wheelEvent(QWheelEvent *) override;
};

///////////////////////////////////////////////////////////////////////////
class ColorPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ColorPanel(QWidget * = nullptr);

    [[nodiscard]] QColor color() const { return color_dialog_btn_->color(); }

signals:
    void changed(const QColor&);

public slots:
    void setColor(const QColor& color, bool silence = true) { color_dialog_btn_->setColor(color, silence); }

private:
    ColorDialogButton *color_dialog_btn_{};
};

#endif //! CAPTURER_COLOR_PANEL_H
