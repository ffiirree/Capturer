#ifndef CAPTURER_SELECTOR_H
#define CAPTURER_SELECTOR_H

#include <QWidget>
#include <QPainter>

#define POSITION_BORDER_WIDTH 10

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
        TOP_BORDER, LEFT_BORDER, RIGHT_BORDER, BOTTOM_BORDER,
        LT_DIAG, RB_DIAG, LB_DIAG, RT_DIAG
    };
public:
    explicit Selector(QWidget *parent = nullptr);
    virtual ~Selector() = default;

public slots:
    virtual void start();

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
    inline bool isContains(const QPoint& p) { return selected().contains(p); }
    bool isTopBorder(const QPoint&);
    bool isLeftBorder(const QPoint&);
    bool isRightBorder(const QPoint&);
    bool isBottomBorder(const QPoint&);
    inline bool isVerBorder(const QPoint& p) { return isLeftBorder(p) || isRightBorder(p); }
    inline bool isHorBorder(const QPoint& p) { return isTopBorder(p) || isBottomBorder(p); }
    inline bool isLTDiag(const QPoint& p) { return isLeftBorder(p) && isTopBorder(p); }
    inline bool isRBDiag(const QPoint& p) { return isRightBorder(p) && isBottomBorder(p); }
    inline bool isLBDiag(const QPoint& p) { return isLeftBorder(p) && isBottomBorder(p); }
    inline bool isRTDiag(const QPoint& p) { return isRightBorder(p) && isTopBorder(p); }
    inline bool isDiag(const QPoint& p) { return isLTDiag(p) || isRBDiag(p); }

    QRect selected();
    QPoint lt(); // left-top point
    QPoint rb(); // right-bottom point

    void updateSelected();

    QPainter painter_;
    Status status_ = INITIAL;
    PointPosition cursor_pos_ = OUTSIDE;

    QPoint begin_{0, 0}, end_{0, 0};
    QPoint mbegin_{0, 0}, mend_{0, 0};
    QPoint rbegin_{0, 0}, rend_{0, 0};

private:
    void registerShortcuts();
};

#endif //! CAPTURER_SELECTOR_H
