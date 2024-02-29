#ifndef CAPTURER_SELECTOR_H
#define CAPTURER_SELECTOR_H

#include "hunter.h"
#include "json.h"
#include "resizer.h"

#include <probe/graphics.h>
#include <QPainter>
#include <QWidget>

class QLabel;

enum class SelectorStatus
{
    INVALID        = 0x00,
    READY          = 0x01,
    PREY_SELECTING = 0x02,
    FREE_SELECTING = 0x04,
    __RESERVERD__  = PREY_SELECTING | FREE_SELECTING,
    CAPTURED       = 0x08,
    MOVING         = 0x10,
    RESIZING       = 0x20,
    LOCKED         = 0x40,

    ENABLE_BITMASK_OPERATORS()
};

struct SelectorStyle
{
    int          border_width{ 2 };
    QColor       border_color{ "#409EFF" };
    Qt::PenStyle border_style{ Qt::SolidLine };
    QColor       mask_color{ "#88000000" };
};

class Selector : public QWidget
{
    Q_OBJECT

public:
    enum class scope_t
    {
        desktop, // virtual screen
        display,
    };

public:
    explicit Selector(QWidget *parent = nullptr);
    ~Selector() override = default;

    [[nodiscard]] SelectorStatus status() const { return status_; }

    [[nodiscard]] hunter::prey_t prey() const { return prey_; }

    // painter window
    void coordinate(const QRect& window) { coordinate_ = window; }

    // scope: display or desktop
    void scope(scope_t scope) { scope_ = scope; }

    [[nodiscard]] scope_t scope() const { return scope_; }

    // selected area
    [[nodiscard]] QRect selected(bool relative = false) const
    {
        return relative ? box_.rect().translated(-box_.range().topLeft()) : box_.rect();
    }

    // set minium valid size
    void setMinValidSize(int x, int y) { min_size_ = QSize{ std::max(2, x), std::max(2, y) }; }

    [[nodiscard]] bool invalid() const
    {
        return box_.width() < min_size_.width() || box_.height() < min_size_.height();
    }

    void showCrossHair(bool v = true)
    {
        crosshair_ = v;
        update();
    }

    // region selection
    void select(const hunter::prey_t&);
    void select(const probe::graphics::display_t&);
    void select(const QRect& rect);

public slots:
    virtual void start(probe::graphics::window_filter_t);

    void status(SelectorStatus);

    void setBorderStyle(const QPen&);
    void setMaskStyle(const QColor&);

    void showRegion();

signals:
    void captured();
    void moved();
    void resized();
    void locked();
    void statusChanged(SelectorStatus);

protected:
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void paintEvent(QPaintEvent *) override;
    void closeEvent(QCloseEvent *event) override;

    void update_info_label();

    // selected area @{
    void translate(int32_t dx, int32_t dy);
    void adjust(int32_t dx1, int32_t dy1, int32_t dx2, int32_t dy2);
    void margins(int32_t dt, int32_t dr, int32_t db, int32_t dl);

    QRect coordinate_{};

    Resizer box_;                              // TODO: do not use this variable directly

    scope_t        scope_{ scope_t::desktop }; // selection scope
    hunter::prey_t prey_{};                    // capture object
    // @}

    QPainter        painter_{};
    SelectorStatus  status_     = SelectorStatus::INVALID;
    ResizerLocation cursor_pos_ = ResizerLocation::OUTSIDE;

    // move
    QPoint move_spos_{ 0, 0 };
    QPoint move_epos_{ 0, 0 };

    // selecting
    QPoint sbegin_{ 0, 0 };

private:
    void registerShortcuts();

    QLabel *info_{ nullptr };

    QPen   pen_{ Qt::cyan, 1, Qt::DashDotLine, Qt::SquareCap, Qt::MiterJoin };
    QColor mask_color_{ 0, 0, 0, 100 };
    bool   mask_hidden_{ false };

    // minimum valid size for selection
    QSize min_size_{ 2, 2 };

    bool crosshair_{};
};

#endif //! CAPTURER_SELECTOR_H
