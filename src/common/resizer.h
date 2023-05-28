#ifndef RESIZER_H
#define RESIZER_H

#include <QWidget>
#include <QPoint>
#include <QRect>
#include "probe/graphics.h"

#define ANCHOR_W        7

class Resizer
{
public:
    enum PointPosition {
        DEFAULT = 0x0000,
        BORDER  = 0x0001 | 0x0002 | 0x0004 | 0x0008,
        ANCHOR  = 0x0010 | 0x0020 | 0x0040 | 0x0080 | 0x0100 | 0x0200 | 0x0400 | 0x0800,

        X1_BORDER   = 0x00000001,  L_BORDER = 0x10000001,
        X2_BORDER   = 0x00000002,  R_BORDER = 0x10000002,
        Y1_BORDER   = 0x00000004,  T_BORDER = 0x10000003,
        Y2_BORDER   = 0x00000008,  B_BORDER = 0x10000004,

        X1_ANCHOR   = 0x00000010,  L_ANCHOR = 0x10000010,
        X2_ANCHOR   = 0x00000020,  R_ANCHOR = 0x10000020,
        Y1_ANCHOR   = 0x00000040,  T_ANCHOR = 0x10000030,
        Y2_ANCHOR   = 0x00000080,  B_ANCHOR = 0x10000040,

        X1Y1_ANCHOR = 0x00000100,  TL_ANCHOR = 0x10000100,
        X1Y2_ANCHOR = 0x00000200,  BL_ANCHOR = 0x10000200,
        X2Y1_ANCHOR = 0x00000400,  TR_ANCHOR = 0x10000300,
        X2Y2_ANCHOR = 0x00000800,  BR_ANCHOR = 0x10000400,

        ROTATE_ANCHOR = 0x00001000,

        INSIDE      = 0x00010000,
        OUTSIDE     = 0x00020000,
        EDITAREA    = 0x00040000,

        ADJUST_AREA = BORDER | ANCHOR | ROTATE_ANCHOR
    };

public:
    Resizer() : Resizer(0, 0, 0, 0, 5) { }

    Resizer(int x1, int y1, int x2, int y2, int resize_border_width = 5)
        : x1_(x1), y1_(y1), x2_(x2), y2_(y2), ADJUST_BORDER_W_(resize_border_width) 
    {
        range(probe::graphics::virtual_screen_geometry());
    }

    Resizer(const QPoint& p1, const QPoint& p2, int resize_border_width = 5)
        : Resizer(p1.x(), p1.y(), p2.x(), p2.y(), resize_border_width) { }

    Resizer(const QPointF& p1, const QPointF& p2, int resize_border_width = 5)
        : Resizer(static_cast<int>(p1.x()), static_cast<int>(p1.y()), static_cast<int>(p2.x()),
                  static_cast<int>(p2.y()), resize_border_width)
    {}

    Resizer(const QPoint& p1, const QSize& s, int resize_border_width = 5)
        : Resizer(p1.x(), p1.y(), p1.x() + s.width() - 1, p1.y() + s.height() - 1, resize_border_width) { }

    explicit Resizer(const QRect& rect, int resize_border_width = 5)
        : Resizer(rect.topLeft(), rect.bottomRight(), resize_border_width) { }

    // TODO: constrain the box coordinates by range
    inline void range(const QRect& r) { range_ = r; }
    inline void range(const probe::geometry_t& r)
    {
        range_ = QRect{ r.x, r.y, static_cast<int>(r.width), static_cast<int>(r.height) };
    }
    inline QRect range() const { return range_; }

    inline void enableRotate(bool val) { rotate_f_ = val; }

    void coords(int x1, int y1, int x2, int y2)
    {
        x1_ = clamp_x(x1);
        y1_ = clamp_y(y1);
        x2_ = clamp_x(x2);
        y2_ = clamp_y(y2);
    }

    void coords(const QPoint& p1, const QPoint& p2) { coords(p1.x(), p1.y(), p2.x(), p2.y()); }
    void coords(const QRect& rect) { coords(rect.left(), rect.top(), rect.right(), rect.bottom()); }
    void coords(const probe::geometry_t& rect)
    {
        coords(rect.left(), rect.top(), rect.right(), rect.bottom());
    }

    // clang-format off
    inline int clamp_x(int v)   { return std::clamp(v, range_.left(), range_.right()); }
    inline int clamp_y(int v)   { return std::clamp(v, range_.top(), range_.bottom()); }
    
    inline int x1()     const   { return x1_; }
    inline int x2()     const   { return x2_; }
    inline int y1()     const   { return y1_; }
    inline int y2()     const   { return y2_; }

    inline void x1(int v)       { x1_ = clamp_x(v); }
    inline void x2(int v)       { x2_ = clamp_x(v); }
    inline void y1(int v)       { y1_ = clamp_y(v); }
    inline void y2(int v)       { y2_ = clamp_y(v); }

    inline int left()   const   { return x1_ < x2_ ? x1_ : x2_; }
    inline int right()  const   { return x1_ > x2_ ? x1_ : x2_; }
    inline int top()    const   { return y1_ < y2_ ? y1_ : y2_; }
    inline int bottom() const   { return y1_ > y2_ ? y1_ : y2_; }

    inline void left(int v)     { x1_ < x2_ ? (x1_ = clamp_x(v)) : (x2_ = clamp_x(v)); }
    inline void right(int v)    { x1_ > x2_ ? (x1_ = clamp_x(v)) : (x2_ = clamp_x(v)); }
    inline void top(int v)      { y1_ < y2_ ? (y1_ = clamp_y(v)) : (y2_ = clamp_y(v)); }
    inline void bottom(int v)   { y1_ > y2_ ? (y1_ = clamp_y(v)) : (y2_ = clamp_y(v)); }
    // clang-format on

    void adjust(int dx1, int dy1, int dx2, int dy2)
    {
        x1_ = clamp_x(x1_ + dx1);
        x2_ = clamp_x(x2_ + dx2);
        y1_ = clamp_y(y1_ + dy1);
        y2_ = clamp_y(y2_ + dy2);
    }

    void margins(int dt, int dr, int db, int dl)
    {
        top(top() + dt);
        right(right() + dr);
        bottom(bottom() + db);
        left(left() + dl);
    }

    void resize(int w, int h)
    {
        right(left() + w);
        bottom(top() + h);
    }

    void resize(const QSize& size)
    {
        right(left() + size.width());
        bottom(top() + size.height());
    }

    void translate(int dx, int dy)
    {
        dx  = std::clamp(dx, range_.left() - left(), range_.right() - right());
        dy  = std::clamp(dy, range_.top() - top(), range_.bottom() - bottom());

        x1_ += dx;
        x2_ += dx;
        y1_ += dy;
        y2_ += dy;
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
        QRect anchor{0, 0, 11, 11};
        anchor.moveCenter({(x1_ + x2_)/2, top() - 20});
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

    inline bool isY1Anchor(const QPoint& p)             const { return Y1Anchor().contains(p); }
    inline bool isY2Anchor(const QPoint& p)             const { return Y2Anchor().contains(p); }
    inline bool isX1Anchor(const QPoint& p)             const { return X1Anchor().contains(p); }
    inline bool isX2Anchor(const QPoint& p)             const { return X2Anchor().contains(p); }
    inline bool isX1Y1Anchor(const QPoint& p)           const { return X1Y1Anchor().contains(p); }
    inline bool isX2Y1Anchor(const QPoint& p)           const { return X2Y1Anchor().contains(p); }
    inline bool isX1Y2Anchor(const QPoint& p)           const { return X1Y2Anchor().contains(p); }
    inline bool isX2Y2Anchor(const QPoint& p)           const { return X2Y2Anchor().contains(p); }

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

    bool isCornerAnchor(const QPoint& p) const
    {
        return isX1Y1Anchor(p) || isX2Y1Anchor(p) || isX1Y2Anchor(p) || isX2Y2Anchor(p);
    }

    inline int width()  const { return std::abs(x1_ - x2_) + 1; }   // like QRect class
    inline int height() const { return std::abs(y1_ - y2_) + 1; }

    inline QSize size() const { return { width(), height() }; }

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
    // point 1
    int x1_ = 0;
    int y1_ = 0;
    // point 2
    int x2_ = 0;
    int y2_ = 0;

    QRect range_{ std::numeric_limits<int>::min(), std::numeric_limits<int>::min(),
                  std::numeric_limits<int>::max(), std::numeric_limits<int>::max() };

    bool rotate_f_ = false;

    int ADJUST_BORDER_W_ = 1;
};


inline QCursor getCursorByLocation(Resizer::PointPosition pos, const QCursor& default_cursor = Qt::CrossCursor)
{
    switch (pos) {
    // clang-format off
    case Resizer::EDITAREA:      return Qt::IBeamCursor;
    case Resizer::X1_ANCHOR:
    case Resizer::X2_ANCHOR:     return Qt::SizeHorCursor;
    case Resizer::Y1_ANCHOR:
    case Resizer::Y2_ANCHOR:     return Qt::SizeVerCursor;
    case Resizer::X1Y1_ANCHOR:
    case Resizer::X2Y2_ANCHOR:   return Qt::SizeFDiagCursor;
    case Resizer::X1Y2_ANCHOR:
    case Resizer::X2Y1_ANCHOR:   return Qt::SizeBDiagCursor;

    case Resizer::BORDER:
    case Resizer::X1_BORDER:
    case Resizer::X2_BORDER:
    case Resizer::Y1_BORDER:
    case Resizer::Y2_BORDER:     return Qt::SizeAllCursor;
    case Resizer::ROTATE_ANCHOR: return QCursor{ QPixmap(":/icons/rotate").scaled(21, 21, Qt::IgnoreAspectRatio, Qt::SmoothTransformation) };

    default:                     return default_cursor;
    // clang-format on
    }
}

#endif // RESIZER_H
