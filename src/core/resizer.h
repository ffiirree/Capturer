#ifndef RESIZER_H
#define RESIZER_H

#include<QWidget>
#include<QPoint>
#include<QRect>
#include "displayinfo.h"

#define ANCHOR_W        5

#define MIN_W   1
#define MIN_H   1

class Resizer
{
public:
    enum PointPosition {
        DEFAULT = 0x0000,
        BORDER = 0x0001 | 0x0002 | 0x0004 | 0x0008,
        ANCHOR = 0x0010 | 0x0020 | 0x0040 | 0x0080 | 0x0100 | 0x0200 | 0x0400 | 0x0800,

        X1_BORDER   = 0x0000'0001,  L_BORDER = 0x1000'0001,
        X2_BORDER   = 0x0000'0002,  R_BORDER = 0x1000'0002,
        Y1_BORDER   = 0x0000'0004,  T_BORDER = 0x1000'0003,
        Y2_BORDER   = 0x0000'0008,  B_BORDER = 0x1000'0004,

        X1_ANCHOR   = 0x0000'0010,  L_ANCHOR = 0x1000'0010,
        X2_ANCHOR   = 0x0000'0020,  R_ANCHOR = 0x1000'0020,
        Y1_ANCHOR   = 0x0000'0040,  T_ANCHOR = 0x1000'0030,
        Y2_ANCHOR   = 0x0000'0080,  B_ANCHOR = 0x1000'0040,

        X1Y1_ANCHOR = 0x0000'0100,  TL_ANCHOR = 0x1000'0100,
        X1Y2_ANCHOR = 0x0000'0200,  BL_ANCHOR = 0x1000'0200,
        X2Y1_ANCHOR = 0x0000'0400,  TR_ANCHOR = 0x1000'0300,
        X2Y2_ANCHOR = 0x0000'0800,  BR_ANCHOR = 0x1000'0400,

        ROTATE_ANCHOR = 0x0000'1000,

        INSIDE      = 0x0001'0000,
        OUTSIDE     = 0x0002'0000,
        ADJUST_AREA = BORDER | ANCHOR | ROTATE_ANCHOR
    };

public:
    Resizer() : Resizer(0, 0, 0, 0, 5) { }

    Resizer(int x1, int y1, int x2, int y2, int resize_border_width = 5)
        : x1_(x1), y1_(y1), x2_(x2), y2_(y2), ADJUST_BORDER_W_(resize_border_width) {
        range({{0, 0}, DisplayInfo::maxSize()});
    }

    Resizer(const QPoint& p1, const QPoint& p2, int resize_border_width = 5)
        : Resizer(p1.x(), p1.y(), p2.x(), p2.y(), resize_border_width) { }

    explicit Resizer(const QRect& rect, int resize_border_width = 5)
        : Resizer(rect.topLeft(), rect.bottomRight(), resize_border_width) { }

    inline void range(const QRect& r) { range_ = r; }
    inline QRect range() const { return range_; }

    inline void enableRotate(bool val) { rotate_f_ = val; }

    void reset(int x1, int y1, int x2, int y2) { x1_ = x1; y1_ = y1; x2_ = x2; y2_ = y2; }
    void reset(const QPoint& p1, const QPoint& p2) { x1_ = p1.x(); y1_ = p1.y(); x2_ = p2.x(); y2_ = p2.y(); }
    void reset(const QRect& rect) { x1_ = rect.left(); y1_ = rect.top(); x2_ = rect.right(); y2_ = rect.bottom(); }

    int& rx1() { return x1_; }  void x1(int val) { x1_ = val; } int x1() const { return x1_; }
    int& rx2() { return x2_; }  void x2(int val) { x2_ = val; } int x2() const { return x2_; }
    int& ry1() { return y1_; }  void y1(int val) { y1_ = val; } int y1() const { return y1_; }
    int& ry2() { return y2_; }  void y2(int val) { y2_ = val; } int y2() const { return y2_; }

    void ajust(int x1, int y1, int x2, int y2) { x1_ += x1; y1_ += y1; x2_ += x2; y2_ += y2; }
    void move(int x, int y)
    {
        x = (x < 0) ? std::max(x, -left()) : std::min(x, range_.right() - right());
        y = (y < 0) ? std::max(y, -top())  : std::min(y, range_.bottom() - bottom());

        x1_ += x; x2_ += x; y1_ += y; y2_ += y;
    }

    inline bool isContains(const QPoint& p) const { return QRect(left(), top(), width(), height()).contains(p); }

    QRect Y1Anchor() const
    {
        return { (x1_ + x2_)/2 - ANCHOR_W/2, y1_ - ANCHOR_W/2, ANCHOR_W, ANCHOR_W };
    }
    QRect X1Anchor() const
    {
        return { x1_ - ANCHOR_W/2, (y1_ + y2_)/2 - ANCHOR_W/2, ANCHOR_W, ANCHOR_W };
    }
    QRect Y2Anchor() const
    {
        return { (x1_ + x2_)/2 - ANCHOR_W/2, y2_ - ANCHOR_W/2, ANCHOR_W, ANCHOR_W };
    }
    QRect X2Anchor() const
    {
        return { x2_ - ANCHOR_W/2, (y1_ + y2_)/2 - ANCHOR_W/2, ANCHOR_W, ANCHOR_W };
    }
    QRect X1Y1Anchor() const
    {
        return { x1_ - ANCHOR_W/2, y1_ - ANCHOR_W/2, ANCHOR_W, ANCHOR_W };
    }
    QRect X1Y2Anchor() const
    {
        return { x1_ - ANCHOR_W/2, y2_ - ANCHOR_W/2, ANCHOR_W, ANCHOR_W };
    }
    QRect X2Y1Anchor() const
    {
        return { x2_ - ANCHOR_W/2, y1_- ANCHOR_W/2, ANCHOR_W, ANCHOR_W };
    }
    QRect X2Y2Anchor() const
    {
        return { x2_ - ANCHOR_W/2, y2_ - ANCHOR_W/2, ANCHOR_W, ANCHOR_W };
    }

    QRect topAnchor() const
    {
        return { (x1_ + x2_)/2 - ANCHOR_W/2, top() - ANCHOR_W/2, ANCHOR_W, ANCHOR_W };
    }
    QRect leftAnchor() const
    {
        return { left() - ANCHOR_W/2, (y1_ + y2_)/2 - ANCHOR_W/2, ANCHOR_W, ANCHOR_W };
    }
    QRect bottomAnchor() const
    {
        return { (x1_ + x2_)/2 - ANCHOR_W/2, bottom() - ANCHOR_W/2, ANCHOR_W, ANCHOR_W };
    }
    QRect rightAnchor() const
    {
        return { right() - ANCHOR_W/2, (y1_ + y2_)/2 - ANCHOR_W/2, ANCHOR_W, ANCHOR_W };
    }
    QRect topLeftAnchor() const
    {
        return { left() - ANCHOR_W/2, top() - ANCHOR_W/2, ANCHOR_W, ANCHOR_W };
    }
    QRect topRightAnchor() const
    {
        return { right() - ANCHOR_W/2, top() - ANCHOR_W/2, ANCHOR_W, ANCHOR_W };
    }
    QRect bottomLeftAnchor() const
    {
        return { left() - ANCHOR_W/2, bottom() - ANCHOR_W/2, ANCHOR_W, ANCHOR_W };
    }
    QRect bottomRightAnchor() const
    {
        return { right() - ANCHOR_W/2, bottom() - ANCHOR_W/2, ANCHOR_W, ANCHOR_W };
    }

    QVector<QRect> anchors() const {
        return QVector<QRect>{
            rightAnchor(),topAnchor(), bottomAnchor(), leftAnchor(),
            topLeftAnchor(), topRightAnchor(), bottomLeftAnchor(), bottomRightAnchor()
        };
    }

    QVector<QRect> cornerAnchors() const {
        return QVector<QRect>{
            topLeftAnchor(), topRightAnchor(), bottomLeftAnchor(), bottomRightAnchor()
        };
    }

    QRect rotateAnchor() const
    {
        QRect anchor{0, 0, 9, 9};
        anchor.moveCenter({(x1_ + x2_)/2, top() - 10});
        return anchor;
    }

    ////
    bool isX1Border(const QPoint& p) const
    {
        return QRect(x1_ - ADJUST_BORDER_W_/2, top() , ADJUST_BORDER_W_, height()).contains(p);
    }
    bool isX2Border(const QPoint& p) const
    {
        return QRect(x2_ - ADJUST_BORDER_W_/2, top(), ADJUST_BORDER_W_, height()).contains(p);
    }
    bool isY1Border(const QPoint& p) const
    {
        return QRect(left(), y1_ - ADJUST_BORDER_W_/2, width(), ADJUST_BORDER_W_).contains(p);
    }
    bool isY2Border(const QPoint& p) const
    {
        return QRect(left(), y2_ - ADJUST_BORDER_W_/2, width(), ADJUST_BORDER_W_).contains(p);
    }
    bool isBorder(const QPoint& p) const
    {
        return isX1Border(p) || isX2Border(p) || isY1Border(p) || isY2Border(p);
    }

    bool isLeftBorder(const QPoint& p) const
    {
        return QRect(left() - ADJUST_BORDER_W_/2, top(), ADJUST_BORDER_W_, height()).contains(p);
    }
    bool isRightBorder(const QPoint& p) const
    {
        return QRect(right() - ADJUST_BORDER_W_/2, top(), ADJUST_BORDER_W_, height()).contains(p);
    }
    bool isTopBorder(const QPoint& p) const
    {
        return QRect(left(), top() - ADJUST_BORDER_W_/2, width(), ADJUST_BORDER_W_).contains(p);
    }
    bool isBottomBorder(const QPoint& p) const
    {
        return QRect(left(), bottom() - ADJUST_BORDER_W_/2, width(), ADJUST_BORDER_W_).contains(p);
    }

    bool isRotateAnchor(const QPoint& p) const
    {
        return rotate_f_ && rotateAnchor().contains(p);
    }

    inline bool isY1Anchor(const QPoint& p)     const { return Y1Anchor().contains(p); }
    inline bool isY2Anchor(const QPoint& p)     const { return Y2Anchor().contains(p); }
    inline bool isX1Anchor(const QPoint& p)     const { return X1Anchor().contains(p); }
    inline bool isX2Anchor(const QPoint& p)     const { return X2Anchor().contains(p); }
    inline bool isX1Y1Anchor(const QPoint& p)   const { return X1Y1Anchor().contains(p); }
    inline bool isX2Y1Anchor(const QPoint& p)   const { return X2Y1Anchor().contains(p); }
    inline bool isX1Y2Anchor(const QPoint& p)   const { return X1Y2Anchor().contains(p); }
    inline bool isX2Y2Anchor(const QPoint& p)   const { return X2Y2Anchor().contains(p); }

    inline bool isTopAnchor(const QPoint& p)            const { return topAnchor().contains(p); }
    inline bool isBottomAnchor(const QPoint& p)         const { return bottomAnchor().contains(p); }
    inline bool isLeftAnchor(const QPoint& p)           const { return leftAnchor().contains(p); }
    inline bool isRightAnchor(const QPoint& p)          const { return rightAnchor().contains(p); }
    inline bool isTopLeftAnchor(const QPoint& p)        const { return topLeftAnchor().contains(p); }
    inline bool isTopRightAnchor(const QPoint& p)       const { return topRightAnchor().contains(p); }
    inline bool isBottomLeftAnchor(const QPoint& p)     const { return bottomLeftAnchor().contains(p); }
    inline bool isBottomRightAnchor(const QPoint& p)    const { return bottomRightAnchor().contains(p); }

    bool isAnchor(const QPoint& p) const
    {
        return  isX1Anchor(p) || isX2Anchor(p) || isY2Anchor(p) || isY1Anchor(p)
                || isX1Y1Anchor(p) || isX2Y1Anchor(p)||isX1Y2Anchor(p) || isX2Y2Anchor(p);
    }

    inline int left()   const { return x1_ < x2_ ? x1_ : x2_; }
    inline int right()  const { return x1_ > x2_ ? x1_ : x2_; }
    inline int top()    const { return y1_ < y2_ ? y1_ : y2_; }
    inline int bottom() const { return y1_ > y2_ ? y1_ : y2_; }

    inline int& rleft()   { return x1_ < x2_ ? x1_ : x2_; }
    inline int& rright()  { return x1_ > x2_ ? x1_ : x2_; }
    inline int& rtop()    { return y1_ < y2_ ? y1_ : y2_; }
    inline int& rbottom() { return y1_ > y2_ ? y1_ : y2_; }

    inline int width()  const { return std::abs(x1_ - x2_) + 1; }   // like QRect class
    inline int height() const { return std::abs(y1_ - y2_) + 1; }

    inline QPoint topLeft()     const { return {left(), top()}; }
    inline QPoint bottomRight() const { return {right(), bottom()}; }
    inline QPoint topRight()    const { return {right(), top()}; }
    inline QPoint bottomLeft()  const { return {left(), bottom()}; }

    inline QPoint point1()      const { return {x1_, y1_}; }
    inline QPoint point2()      const { return {x2_, y2_}; }

    inline QRect rect() const { return {QPoint{left(), top()}, QPoint{right(), bottom()}}; }

    PointPosition absolutePos(const QPoint& p) const
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

        if(isRotateAnchor(p)) return ROTATE_ANCHOR;

        return isContains(p) ? INSIDE : OUTSIDE;
    }

    PointPosition relativePos(const QPoint& p) const
    {
        if(isLeftAnchor(p))     return L_ANCHOR;
        if(isRightAnchor(p))    return R_ANCHOR;
        if(isTopAnchor(p))      return T_ANCHOR;
        if(isBottomAnchor(p))   return B_ANCHOR;

        if(isTopLeftAnchor(p))      return TL_ANCHOR;
        if(isTopRightAnchor(p))     return TR_ANCHOR;
        if(isBottomRightAnchor(p))  return BR_ANCHOR;
        if(isBottomLeftAnchor(p))   return BL_ANCHOR;

        if(isLeftBorder(p))     return L_BORDER;
        if(isRightBorder(p))    return R_BORDER;
        if(isTopBorder(p))      return T_BORDER;
        if(isBottomBorder(p))   return B_BORDER;

        if(isRotateAnchor(p)) return ROTATE_ANCHOR;

        return isContains(p) ? INSIDE : OUTSIDE;
    }

protected:
    int x1_ = 0;
    int y1_ = 0;
    int x2_ = 0;
    int y2_ = 0;

    QRect range_{0, 0, 0, 0};

    bool rotate_f_ = false;

    int ADJUST_BORDER_W_ = 1;
};

#endif // RESIZER_H
