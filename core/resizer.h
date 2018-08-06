#ifndef RESIZER_H
#define RESIZER_H

#include<QWidget>
#include<QPoint>
#include<QRect>

#define ANCHOR_WIDTH        7

#define MIN_W   1
#define MIN_H   1

class Resizer
{
public:
    enum PointPosition {
        DEFAULT = 0x0000,
        BORDER = 0x0001 | 0x0002 | 0x0004 | 0x0008,
        ANCHOR = 0x0010 | 0x0020 | 0x0040 | 0x0080 | 0x0100 | 0x0200 | 0x0400 | 0x0800,

        X1_BORDER   = 0x0001,
        X2_BORDER   = 0x0002,
        Y1_BORDER   = 0x0004,
        Y2_BORDER   = 0x0008,

        X1_ANCHOR   = 0x0010,
        X2_ANCHOR   = 0x0020,
        Y1_ANCHOR   = 0x0040,
        Y2_ANCHOR   = 0x0080,
        X1Y1_ANCHOR = 0x0100,
        X1Y2_ANCHOR = 0x0200,
        X2Y1_ANCHOR = 0x0400,
        X2Y2_ANCHOR = 0x0800,

        INSIDE      = 0x1000,
        OUTSIDE     = 0x2000,
    };

public:
    Resizer() = default;

    Resizer(int x1, int y1, int x2, int y2, int resize_border_width = 5)
        : x1_(x1), y1_(y1), x2_(x2), y2_(y2), RESIZE_BORDER_WIDTH_(resize_border_width) { }

    Resizer(const QPoint& p1, const QPoint& p2, int resize_border_width = 5)
        : x1_(p1.x()), y1_(p1.y()), x2_(p2.x()), y2_(p2.y()), RESIZE_BORDER_WIDTH_(resize_border_width) { }

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
        return QRect(x1_ - RESIZE_BORDER_WIDTH_/2, t() , RESIZE_BORDER_WIDTH_, h()).contains(p);
    }
    bool isX2Border(const QPoint& p) const
    {
        return QRect(x2_ - RESIZE_BORDER_WIDTH_/2, t(), RESIZE_BORDER_WIDTH_, h()).contains(p);
    }
    bool isY1Border(const QPoint& p) const
    {
        return QRect(l(), y1_ - RESIZE_BORDER_WIDTH_/2, w(), RESIZE_BORDER_WIDTH_).contains(p);
    }
    bool isY2Border(const QPoint& p) const
    {
        return QRect(l(), y2_ - RESIZE_BORDER_WIDTH_/2, w(), RESIZE_BORDER_WIDTH_).contains(p);
    }
    bool isBorder(const QPoint& p) const
    {
        return isX1Border(p) || isX2Border(p) || isY1Border(p) || isY2Border(p);
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

    inline QPoint topLeft() const       { return {l(), t()}; }
    inline QPoint bottomRight() const   { return {r(), b()}; }
    inline QPoint topRight() const      { return {r(), t()}; }
    inline QPoint bottomLeft() const    { return {l(), b()}; }

    PointPosition position(const QPoint& p)
    {
        if(isX1Anchor(p)) return X1_ANCHOR;
        if(isX2Anchor(p)) return X2_ANCHOR;
        if(isY2Anchor(p)) return Y2_ANCHOR;
        if(isY1Anchor(p)) return Y1_ANCHOR;

        if(isX1Y1Anchor(p)) return X1Y1_ANCHOR;
        if(isX2Y1Anchor(p)) return X2Y1_ANCHOR;
        if(isX1Y2Anchor(p)) return X1Y2_ANCHOR;
        if(isX2Y2Anchor(p)) return X2Y2_ANCHOR;

        if(isY1Border(p)) return Y1_BORDER;
        if(isY2Border(p)) return Y2_BORDER;
        if(isX1Border(p)) return X1_BORDER;
        if(isX2Border(p)) return X2_BORDER;

        return isContains(p) ? INSIDE : OUTSIDE;
    }

protected:
    int x1_ = 0;
    int y1_ = 0;
    int x2_ = 0;
    int y2_ = 0;

    int RESIZE_BORDER_WIDTH_ = 1;
};

#endif // RESIZER_H
