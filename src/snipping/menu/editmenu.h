#ifndef EDIT_MENU_H
#define EDIT_MENU_H

#include <QBrush>
#include <QPen>
#include <QWidget>

class EditMenu : public QWidget
{
    Q_OBJECT

public:
    explicit EditMenu(QWidget *parent = nullptr);

    // pen: color, width
    virtual QPen pen() const { return pen_; }

    virtual void pen(const QPen& pen) { pen_ = pen; }

    // brush
    virtual QBrush brush() const { return brush_; }

    virtual void brush(const QBrush& brush) { brush_ = brush; }

    // fill
    virtual bool fill() const { return fill_; }

    virtual void fill(bool f) { fill_ = f; }

    // font
    virtual QFont font() const { return font_; }

    virtual void font(const QFont& font) { font_ = font; }

signals:
    void penChanged(const QPen&);
    void brushChanged(const QBrush&);
    void fontChanged(const QFont&);
    void fillChanged(bool);

    void moved();

protected:
    void moveEvent(QMoveEvent *) override;

    void addSeparator();
    void addWidget(QWidget *);

    QPen pen_{ Qt::red, 3 };
    QBrush brush_{ Qt::NoBrush };
    QFont font_{};
    bool fill_{};
};

#endif // EDIT_MENU_H
