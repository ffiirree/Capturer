#ifndef COLOR_PANEL_H
#define COLOR_PANEL_H

#include <QPushButton>
#include <QPainter>
#include <QColorDialog>
#include <QGridLayout>
#include <QDebug>

class ColorButton : public QPushButton
{
    Q_OBJECT

public:
    explicit ColorButton(QWidget *parent = nullptr);
    explicit ColorButton(const QColor& c, QWidget *parent = nullptr);

    inline void color(const QColor& c) { color_ = c; update(); emit changed(color_); }
    inline QColor color() const { return color_; }

signals:
    void clicked(const QColor&);
    void changed(const QColor&);

protected:
    void paintEvent(QPaintEvent *e) override;
    void enterEvent(QEvent *) override;
    void leaveEvent(QEvent *) override;

protected:
    QColor color_ = Qt::blue;
    QColor default_color_{"#bfbfbf"};
    QPen border_pen_{default_color_, 2};
    QColor hover_color_ = QColor("#2080f0");
};

///////////////////////////////////////////////////////////////////////////
class ColorDialogButton : public ColorButton
{
    Q_OBJECT

public:
    explicit ColorDialogButton(QWidget *parent = nullptr);
    explicit ColorDialogButton(const QColor&, QWidget *parent = nullptr);
    ~ColorDialogButton() override;

private:
    QColorDialog *color_dialog_;
};

///////////////////////////////////////////////////////////////////////////
class ColorPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ColorPanel(QWidget * parent = nullptr);
    QColor color() const { return color_dialog_btn_->color(); }

signals:
    void changed(const QColor&);

public slots:
    void setColor(const QColor& color) {
        color_dialog_btn_->color(color);
    }

private:
    ColorDialogButton * color_dialog_btn_ = nullptr;
};

#endif // COLOR_PANEL_H
