#ifndef RESIZER_H
#define RESIZER_H

#include<QPoint>
#include<QRect>

#define RESIZE_BORDER_WIDTH 1
#define ANCHOR_WIDTH        5

#define MIN_W   1
#define MIN_H   1

class Resizer
{
public:
    enum PointPosition {
        INSIDE, OUTSIDE,
        BORDER, DIAG,
//        TOP_BORDER, LEFT_BORDER, RIGHT_BORDER, BOTTOM_BORDER,
//        TOP_ANCHOR, LEFT_ANCHOR, RIGHT_ANCHOR, BOTTOM_ANCHOR,
//        TOPLEFT_ANCHOR, TOPRIGHT_ANCHOR, BOTTOMLEFT_ANCHOR, BOTTOMRIGHT_ANCHOR,

        X1_BORDER, X2_BORDER, Y1_BORDER, Y2_BORDER,
        X1Y1_ANCHOR, X1Y2_ANCHOR, X2Y1_ANCHOR, X2Y2_ANCHOR,
        X1_ANCHOR, X2_ANCHOR, Y1_ANCHOR, Y2_ANCHOR
    };
public:
    Resizer(int x1, int y1, int x2, int y2)
        : x1_(x1), y1_(y1), x2_(x2), y2_(y2) { }

    Resizer(const QPoint& p1, const QPoint& p2)
        : x1_(p1.x()), y1_(p1.y()), x2_(p2.x()), y2_(p2.y()) { }

    inline bool isContains(const QPoint& p) const { return QRect(l(), t(), w(), h()).contains(p); }

    QRect Y1Anchor() const
    {
        return { (x1_ + x2_)/2 - ANCHOR_WIDTH/2, y1_ - ANCHOR_WIDTH/2, ANCHOR_WIDTH, ANCHOR_WIDTH };
    }
    QRect X1Anchor() const
    {
        return { x1_ - ANCHOR_WIDTH/2, (y1_ + y2_)/2 - ANCHOR_WIDTH/2, ANCHOR_WIDTH, ANCHOR_WIDTH };
    }
    QRect Y2Anchor() const
    {
        return { (x1_ + x2_)/2 - ANCHOR_WIDTH/2, y2_ - ANCHOR_WIDTH/2, ANCHOR_WIDTH, ANCHOR_WIDTH };
    }
    QRect X2Anchor() const
    {
        return { x2_ - ANCHOR_WIDTH/2, (y1_ + y2_)/2 - ANCHOR_WIDTH/2, ANCHOR_WIDTH, ANCHOR_WIDTH };
    }
    QRect X1Y1Anchor() const
    {
        return { x1_ - ANCHOR_WIDTH/2, y1_ - ANCHOR_WIDTH/2, ANCHOR_WIDTH, ANCHOR_WIDTH };
    }
    QRect X1Y2Anchor() const
    {
        return { x1_ - ANCHOR_WIDTH/2, y2_ - ANCHOR_WIDTH/2, ANCHOR_WIDTH, ANCHOR_WIDTH };
    }
    QRect X2Y1Anchor() const
    {
        return { x2_ - ANCHOR_WIDTH/2, y1_- ANCHOR_WIDTH/2, ANCHOR_WIDTH, ANCHOR_WIDTH };
    }
    QRect X2Y2Anchor() const
    {
        return { x2_ - ANCHOR_WIDTH/2, y2_ - ANCHOR_WIDTH/2, ANCHOR_WIDTH, ANCHOR_WIDTH };
    }

    ////
    bool isX1Border(const QPoint& p) const
    {
        return QRect(x1_ - RESIZE_BORDER_WIDTH/2, t() , RESIZE_BORDER_WIDTH, h()).contains(p);
    }
    bool isX2Border(const QPoint& p) const
    {
        return QRect(x2_ - RESIZE_BORDER_WIDTH/2, t(), RESIZE_BORDER_WIDTH, h()).contains(p);
    }
    bool isY1Border(const QPoint& p) const
    {
        return QRect(l(), y1_ - RESIZE_BORDER_WIDTH/2, w(), RESIZE_BORDER_WIDTH).contains(p);
    }
    bool isY2Border(const QPoint& p) const
    {
        return QRect(l(), y2_ - RESIZE_BORDER_WIDTH/2, w(), RESIZE_BORDER_WIDTH).contains(p);
    }
    bool isBorder(const QPoint& p)
    {
        return QRect(l() - 2, t()- 2, w() + 4, h() + 4).contains(p) && !QRect(l() + 2, t() + 2, w() - 4, h() - 4).contains(p);
    }

    inline bool isY1Anchor(const QPoint& p) const { return Y1Anchor().contains(p); }
    inline bool isY2Anchor(const QPoint& p) const { return Y2Anchor().contains(p); }
    inline bool isX1Anchor(const QPoint& p) const { return X1Anchor().contains(p); }
    inline bool isX2Anchor(const QPoint& p) const { return X2Anchor().contains(p); }
    inline bool isX1Y1Anchor(const QPoint& p) const { return X1Y1Anchor().contains(p); }
    inline bool isX2Y1Anchor(const QPoint& p) const { return X2Y1Anchor().contains(p); }
    inline bool isX1Y2Anchor(const QPoint& p) const { return X1Y2Anchor().contains(p); }
    inline bool isX2Y2Anchor(const QPoint& p) const { return X2Y2Anchor().contains(p); }

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

    PointPosition position(const QPoint& p)
    {
        if(isX1Y1Anchor(p)) return X1Y1_ANCHOR;
        if(isX2Y1Anchor(p)) return X2Y1_ANCHOR;
        if(isX1Y2Anchor(p)) return X1Y2_ANCHOR;
        if(isX2Y2Anchor(p)) return X2Y2_ANCHOR;
        if(isY2Anchor(p)) return Y2_ANCHOR;
        if(isY1Anchor(p)) return Y1_ANCHOR;
        if(isX1Anchor(p)) return X1_ANCHOR;
        if(isX2Anchor(p)) return X2_ANCHOR;

        if(isY1Border(p)) return Y1_BORDER;
        if(isY2Border(p)) return Y2_BORDER;
        if(isX1Border(p)) return X1_BORDER;
        if(isX2Border(p)) return X2_BORDER;

        return isContains(p) ? INSIDE : OUTSIDE;
    }

    int x1_ = 0;
    int x2_ = 0;
    int y1_ = 0;
    int y2_ = 0;
};

#endif // RESIZER_H
