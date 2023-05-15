#ifndef SELECTOR_H
#define SELECTOR_H

#include "json.h"
#include "probe/graphics.h"
#include "resizer.h"
#include "utils.h"

#include <QLabel>
#include <QPainter>
#include <QWidget>

// clang-format off
#define LOCKED()            do{ status_ = SelectorStatus::LOCKED;   emit locked();              } while(0)
#define CAPTURED()          do{ status_ = SelectorStatus::CAPTURED; emit captured(); update();  } while(0)
// clang-format on

class Selector : public QWidget
{
    Q_OBJECT

public:
    enum class SelectorStatus
    {
        INITIAL,
        NORMAL,
        START_SELECTING,
        SELECTING,
        CAPTURED,
        MOVING,
        RESIZING,
        LOCKED,
    };

    enum class mode_t
    {
        rectanle,
        display,
        window,
    };

public:
    explicit Selector(QWidget *parent = nullptr);
    ~Selector() override = default;

public slots:
    virtual void start();
    void setBorderColor(const QColor&);
    void setBorderWidth(int);
    void setBorderStyle(Qt::PenStyle s);
    void setMaskColor(const QColor&);
    void setUseDetectWindow(bool);
    void showRegion()
    {
        info_->hide();
        mask_hidden_ = true;
        repaint();
    }
    void resetSelected()
    {
        box_.reset(use_detect_ ? probe::graphics::virtual_screen_geometry()
                               : probe::graphics::geometry_t{});
    }

    // hiding
    void hide()
    {
        info_->hide();
        resetSelected();
        repaint();
        QWidget::hide();
    }
    virtual void exit();

    void updateTheme(json& setting);

    // minimum size of selected area
    void setMinValidSize(const QSize& size) { min_size_ = size; }
    bool isValid() { return box_.width() >= min_size_.width() && box_.height() >= min_size_.height(); }

signals:
    void captured();
    void moved();
    void resized();
    void locked();

protected:
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void paintEvent(QPaintEvent *) override;

    // selected area
    [[nodiscard]] inline QRect selected() const { return box_.rect(); }

    QPainter painter_;
    SelectorStatus status_             = SelectorStatus::INITIAL;
    Resizer::PointPosition cursor_pos_ = Resizer::OUTSIDE;

    // move
    QPoint mbegin_{ 0, 0 }, mend_{ 0, 0 };

    // selecting
    QPoint sbegin_{ 0, 0 };

    // selected area @{
    void select(const probe::graphics::window_t&);
    void select(const probe::graphics::display_t&);
    void select(const QRect& rect);

    mode_t mode() const { return mode_; }

    Resizer box_;                          // do not use this variable directly
    mode_t mode_ = mode_t::rectanle;       // mode
    probe::graphics::window_t window_{};   // if select a window
    probe::graphics::display_t display_{}; // if select a window
    // @}

    bool prevent_transparent_ = false;

    // windows detection flags
    probe::graphics::window_filter_t windows_detection_flags_{ probe::graphics::window_filter_t::visible };

private:
    void registerShortcuts();
    void moveSelectedBox(int x, int y);

    QLabel *info_{ nullptr };

    QPen pen_{ Qt::cyan, 1, Qt::DashDotLine, Qt::SquareCap, Qt::MiterJoin };
    QColor mask_color_{ 0, 0, 0, 100 };
    bool use_detect_{ true };
    bool mask_hidden_{ false };

    QSize min_size_{ 2, 2 };
};

#endif // !SELECTOR_H
