#ifndef CAPTURER_SELECTOR_H
#define CAPTURER_SELECTOR_H

#include <QWidget>
#include <QPainter>
#include "json.h"
#include "utils.h"
#include "sizeinfo.h"
#include "resizer.h"
#include "displayinfo.h"

#define LOCKED()            do{ status_ = LOCKED; emit locked(); } while(0)
#define CAPTURED()          do{ status_ = CAPTURED; emit captured(); } while(0)

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
        RESIZING,
        LOCKED,
    };

public:
    explicit Selector(QWidget *parent = nullptr);
    virtual ~Selector() override = default;

public slots:
    virtual void start();
    void setBorderColor(const QColor&);
    void setBorderWidth(int);
    void setBorderStyle(Qt::PenStyle s);
    void setMaskColor(const QColor&);
    void setUseDetectWindow(bool);

    virtual void exit();

    static void drawSelector(QPainter *, const Resizer&);

    void updateTheme(json& setting);

signals:
    void captured();
    void moved();
    void resized();
    void locked();

protected:
    virtual void mousePressEvent(QMouseEvent *) override;
    virtual void mouseMoveEvent(QMouseEvent * ) override;
    virtual void mouseReleaseEvent(QMouseEvent *) override;
    virtual void paintEvent(QPaintEvent *) override;

    // selected area
    inline QRect selected() const { return box_.rect(); }

    void updateSelected();

    QPainter painter_;
    Status status_ = INITIAL;
    Resizer::PointPosition cursor_pos_ = Resizer::OUTSIDE;

    // move
    QPoint mbegin_{0, 0}, mend_{0, 0};
    // resize
    QPoint rbegin_{0, 0}, rend_{0, 0};

    PaintType modified_ = PaintType::UNMODIFIED;
    Resizer box_;

private:
    void registerShortcuts();

    SizeInfoWidget * info_ = nullptr;

    QPen pen_{Qt::cyan, 1, Qt::DashDotLine, Qt::SquareCap, Qt::MiterJoin};
    QColor mask_color_{0, 0, 0, 100};
    bool use_detect_ = true;
};

#endif //! CAPTURER_SELECTOR_H
