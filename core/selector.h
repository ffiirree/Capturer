#ifndef CAPTURER_SELECTOR_H
#define CAPTURER_SELECTOR_H

#include <QWidget>
#include <QPainter>
#include "info.h"

#define RESIZE_BORDER_WIDTH 1
#define ANCHOR_WIDTH        5

#define MIN_W   1
#define MIN_H   1

class Selector : public QWidget
{
    Q_OBJECT

public:
    enum Status {
        INITIAL,
        NORMAL,
        SELECTING,
        CAPTURED,
        MOVING,
        RESIZING, /*TOP_RESIZING, BOTTOM_RESIZING, LEFT_RESIZING, RIGHT_RESIZING,*/
        LOCKED,
    };

    enum PointPosition {
        INSIDE, OUTSIDE,
        BORDER, DIAG,
        Y1_BORDER, X1_BORDER, X2_BORDER, Y2_BORDER,
        X1Y1_DIAG, X1Y2_DIAG, X2Y1_DIAG, X2Y2_DIAG
    };
public:
    explicit Selector(QWidget *parent = nullptr);
    virtual ~Selector() = default;

public slots:
    virtual void start();
    void setBorderColor(const QColor&);
    void setBorderWidth(int);
    void setBorderStyle(Qt::PenStyle s);

signals:
    void moved();
    void resized();

protected:
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent* event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void paintEvent(QPaintEvent *event);

    // The cursor's position
    PointPosition position(const QPoint&);
    inline bool isContains(const QPoint& p) const { return selected().contains(p); }

    QRect Y1Anchor() const;
    QRect X1Anchor() const;
    QRect Y2Anchor() const;
    QRect X2Anchor() const;
    QRect X1Y1Anchor() const;
    QRect X1Y2Anchor() const;
    QRect X2Y1Anchor() const;
    QRect X2Y2Anchor() const;

    bool isX1Border(const QPoint&) const;
    bool isX2Border(const QPoint&) const;
    bool isY1Border(const QPoint&) const;
    bool isY2Border(const QPoint&) const;
    inline bool isVBorder(const QPoint& p) const { return isX1Border(p) || isX2Border(p); }
    inline bool isHBorder(const QPoint& p) const { return isY1Border(p) || isY2Border(p); }

    inline bool isX1Y1Anchor(const QPoint& p) const { return X1Y1Anchor().contains(p); }
    inline bool isX1Y2Anchor(const QPoint& p) const { return X1Y2Anchor().contains(p); }
    inline bool isX2Y1Anchor(const QPoint& p) const { return X2Y1Anchor().contains(p); }
    inline bool isX2Y2Anchor(const QPoint& p) const { return X2Y2Anchor().contains(p); }

    // selected area
    inline QRect selected() const { return {l(), t(), w(), h()}; }

    inline int l() const { return x1_ < x2_ ? x1_ : x2_; }
    inline int r() const { return x1_ > x2_ ? x1_ : x2_; }
    inline int t() const { return y1_ < y2_ ? y1_ : y2_; }
    inline int b() const { return y1_ > y2_ ? y1_ : y2_; }

    inline int w() const { auto w = std::abs(x1_ - x2_); return w > MIN_W ? w : MIN_W; }
    inline int h() const { auto h = std::abs(y1_ - y2_); return h > MIN_H ? h : MIN_H; }

    inline QPoint topLeft() const { return {l(), t()}; }
    inline QPoint bottomRight() const { return {r(), b()}; }
    inline QPoint topRight() const { return {r(), t()}; }
    inline QPoint bottomLeft() const { return {l(), b()}; }

    void updateSelected();

    QPainter painter_;
    Status status_ = INITIAL;
    PointPosition cursor_pos_ = OUTSIDE;

    // selected area's border
    int x1_ = 0;
    int x2_ = 0;
    int y1_ = 0;
    int y2_ = 0;

    // move
    QPoint mbegin_{0, 0}, mend_{0, 0};
    // resize
    QPoint rbegin_{0, 0}, rend_{0, 0};

    Info * info_ = nullptr;

private:
    void registerShortcuts();

    QColor border_color_ = Qt::cyan;
    int border_width_ = 1;
    Qt::PenStyle border_style_ = Qt::DashDotLine;
};

#endif //! CAPTURER_SELECTOR_H
