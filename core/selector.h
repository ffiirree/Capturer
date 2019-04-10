#ifndef CAPTURER_SELECTOR_H
#define CAPTURER_SELECTOR_H

#include <QWidget>
#include <QPainter>
#include "info.h"
#include "resizer.h"

class Selector : public QWidget, public Resizer
{
    Q_OBJECT

public:
    enum Status {
        INITIAL,
        NORMAL,
        SELECTING,
        CAPTURED,
        MOVING,
        RESIZING,
        LOCKED,
    };

public:
    explicit Selector(QWidget *parent = nullptr);
    virtual ~Selector() override = default;

public slots:
    virtual void start();
    void setMaxSize();
    void setBorderColor(const QColor&);
    void setBorderWidth(int);
    void setBorderStyle(Qt::PenStyle s);
    void setMaskColor(const QColor&);
    void setUseDetectWindow(bool);

    virtual void exit();

signals:
    void moved();
    void resized();

protected:
    virtual void mousePressEvent(QMouseEvent *) override;
    virtual void mouseMoveEvent(QMouseEvent * ) override;
    virtual void mouseReleaseEvent(QMouseEvent *) override;
    virtual void keyPressEvent(QKeyEvent *) override;
    virtual void paintEvent(QPaintEvent *) override;

    // selected area
    inline QRect selected() const { return {l(), t(), w(), h()}; }

    void updateSelected();

    QPainter painter_;
    Status status_ = INITIAL;
    PointPosition cursor_pos_ = OUTSIDE;

    // move
    QPoint mbegin_{0, 0}, mend_{0, 0};
    // resize
    QPoint rbegin_{0, 0}, rend_{0, 0};

    Info * info_ = nullptr;

    QColor mask_color_{0, 0, 0, 100};

private:
    void registerShortcuts();

    QColor border_color_ = Qt::cyan;
    int border_width_ = 1;
    Qt::PenStyle border_style_ = Qt::DashDotLine;

    bool use_detect_ = true;
};

#endif //! CAPTURER_SELECTOR_H
