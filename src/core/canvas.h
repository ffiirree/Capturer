#ifndef CANVAS_H
#define CANVAS_H

#include <cstdint>
#include <QPixmap>
#include "utils.h"
#include "imageeditmenu.h"
#include "command.h"
#include "circlecursor.h"

#define HIDE_AND_COPY_CMD(CMD)                           \
        st(                                              \
            auto pre_cmd = CMD;                          \
            pre_cmd->visible(false);                     \
            CMD = make_shared<PaintCommand>(*pre_cmd);   \
            CMD->previous(pre_cmd);                      \
        )

class Canvas : public QObject
{
    Q_OBJECT

public:
    enum EditStatus : std::uint32_t {
        NONE            = 0x00000000,
        READY           = 0x00010000,
        GRAPH_CREATING  = 0x00100000,
        GRAPH_MOVING    = 0x00200000,
        GRAPH_RESIZING  = 0x00400000,
        GRAPH_ROTATING  = 0x00800000,

        READY_MASK      = 0x000f0000,
        OPERATION_MASK  = 0xfff00000,
        GRAPH_MASK      = 0x0000ffff
    };
public:
    Canvas(QWidget*parent = nullptr);

    ImageEditMenu* menu_ = nullptr;


signals:
    void close();
    void update();

public slots:
    void device(QWidget* device) { device_ = device; }
    void canvas(QPixmap* canvas) { canvas_ = canvas; captured_screen_ = (*canvas).copy(); }
    void start();
    //void exit();

    //void save();
    //void copy();
    //void pin();
    //QPixmap snipped();

    void undo();
    void redo();

protected:
    bool eventFilter(QObject*, QEvent*) override;

private:
    void updateCanvas();
    void focusOn(shared_ptr<PaintCommand>);
    void modified(PaintType type) { modified_ = (type > modified_) ? type : modified_; update(); }

    void mousePressEvent(QMouseEvent*);
    void mouseMoveEvent(QMouseEvent*);
    void mouseReleaseEvent(QMouseEvent*);
    void paintEvent(QPaintEvent *);

    void updateHoverPos(const QPoint&);
    void setCursorByHoverPos(Resizer::PointPosition, const QCursor & = Qt::CrossCursor);
    QImage mosaic(const QImage& );
    void drawSelector(QPainter*, const Resizer&);

    Resizer::PointPosition hover_position_ = Resizer::OUTSIDE;
    QPoint move_begin_{ 0, 0 };
    QPoint resize_begin_{ 0, 0 };

    CommandStack commands_;
    CommandStack redo_stack_;

    shared_ptr<PaintCommand> hover_cmd_ = nullptr;    // hover
    shared_ptr<PaintCommand> focus_cmd_ = nullptr;    // focus
    shared_ptr<PaintCommand> copied_cmd_ = nullptr;   // copied

    PaintType modified_ = PaintType::UNMODIFIED;

    CircleCursor circle_cursor_{ 20 };
    QWidget* device_;
    QPixmap *canvas_ = nullptr;
    QPixmap captured_screen_;
    uint32_t edit_status_ = EditStatus::NONE;
};

#endif // CANVAS_H
