#ifndef CANVAS_H
#define CANVAS_H

#include "circlecursor.h"
#include "command.h"
#include "imageeditmenu.h"
#include "utils.h"

#include <cstdint>
#include <QPixmap>

#define HIDE_AND_COPY_CMD(CMD)                                                                             \
    {                                                                                                      \
        auto __cmd = CMD;                                                                                  \
        __cmd->visible(false);                                                                             \
        CMD = std::make_shared<PaintCommand>(*__cmd);                                                      \
        CMD->previous(__cmd);                                                                              \
        CMD->visible(true);                                                                                \
    }

class Canvas : public QObject
{
    Q_OBJECT

public:
    enum EditStatus : uint32_t
    {
        NONE           = 0x0000'0000,
        READY          = 0x0001'0000,
        GRAPH_CREATING = 0x0010'0000,
        GRAPH_MOVING   = 0x0020'0000,
        GRAPH_RESIZING = 0x0040'0000,
        GRAPH_ROTATING = 0x0080'0000,

        READY_MASK     = 0x000f'0000,
        OPERATION_MASK = 0xfff0'0000,
        GRAPH_MASK     = 0x0000'ffff
    };

public:
    explicit Canvas(ImageEditMenu *, QWidget * = nullptr);

    ImageEditMenu *menu() const { return menu_; }

    QCursor getCursorShape();
    void mousePressEvent(QMouseEvent *);
    void mouseMoveEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);
    void keyPressEvent(QKeyEvent *);
    void keyReleaseEvent(QKeyEvent *);
    void wheelEvent(QWheelEvent *);

    void updateCanvas();
    void drawModifying(QPainter *);

    QPixmap pixmap() { return canvas_; }

    void offset(const QPoint& offset) { offset_ = offset; }

    void globalOffset(const QPoint& offset) { global_offset_ = offset; }

    bool editing();

    void clear();
    void reset();

    void commands(CommandStack cmds)
    {
        commands_ = cmds;
        redo_stack_.clear();

        if (!commands_.empty()) {
            focusOn(commands_.back());
        }
        modified(PaintType::REPAINT_ALL);
    }

    CommandStack commands() { return commands_; }

signals:
    void focusOnGraph(Graph);
    void closed();
    void changed();
    void cursorChanged();

public slots:

    void enable() { enabled_ = true; }

    void disable() { enabled_ = false; }

    void changeGraph(Graph);
    void pixmap(const QPixmap& canvas);
    void copy();
    void paste();
    void remove();

    void undo();
    void redo();

    void modified(PaintType type)
    {
        modified_ = ((type > modified_) || (type == PaintType::UNMODIFIED)) ? type : modified_;

        if (modified_) changed();
    }

private:
    bool eventFilter(QObject *, QEvent *) override;
    void focusOn(const std::shared_ptr<PaintCommand>&);

    void updateHoverPos(const QPoint&);
    QImage mosaic(const QImage&);

    ImageEditMenu *menu_{ nullptr };

    Resizer::PointPosition hover_position_{ Resizer::OUTSIDE };
    QPoint move_begin_{ 0, 0 };

    CommandStack commands_;
    CommandStack redo_stack_;

    std::shared_ptr<PaintCommand> hover_cmd_{};  // hover
    std::shared_ptr<PaintCommand> focus_cmd_{};  // focus
    std::shared_ptr<PaintCommand> copied_cmd_{}; // copied

    PaintType modified_{ PaintType::UNMODIFIED };

    CircleCursor circle_cursor_{ 20 };
    QPixmap canvas_;
    QPixmap backup_;
    uint32_t edit_status_{ EditStatus::NONE };

    QPoint offset_{ 0, 0 };
    QPoint global_offset_{ 0, 0 };

    bool enabled_{ false };
};

#endif // CANVAS_H
