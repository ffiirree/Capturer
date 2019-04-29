#ifndef TESTCAPTURER_H
#define TESTCAPTURER_H

#include <selector.h>
#include <QTextEdit>
#include <QPixmap>
#include <QSystemTrayIcon>
#include "imageeditmenu.h"
#include "magnifier.h"
#include "textedit.h"
#include "command.h"
#include "resizer.h"
#include "circlecursor.h"

#define  DO(COMMAND) do{ auto __command__ = COMMAND; focus_ = __command__; undo_stack_.push(__command__); redo_stack_.clear(); }while(0)

#define DEFINE_PAINTING_STATES(X) START_PAINTING_##X    = START_PAINTING | X,   PAINTING_##X    = PAINTING | X, END_PAINTING_##X    = END_PAINTING | X
class ScreenShoter : public Selector
{
    Q_OBJECT

    using super = Selector;

public:
    enum EditStatus: unsigned int {
        NONE = 0x0000, START_PAINTING = 0x0001'0000, PAINTING = 0x0002'0000, END_PAINTING = 0x0004'0000, END = 0x0006'0000,
        DEFINE_PAINTING_STATES(RECTANGLE),
        DEFINE_PAINTING_STATES(CIRCLE),
        DEFINE_PAINTING_STATES(ARROW),
        DEFINE_PAINTING_STATES(LINE),
        DEFINE_PAINTING_STATES(CURVES),
        DEFINE_PAINTING_STATES(MOSAIC),
        DEFINE_PAINTING_STATES(TEXT),
        START_ERASING = START_PAINTING | ERASER, ERASING = PAINTING | ERASER, END_ERASING = END_PAINTING | ERASER,
        GRAPH_MOVING = 0x0010'0000, GRAPH_RESIZING = 0x0020'0000, GRAPH_ROTATING = 0x0030'0000,
        GRAPH_MASK = 0x0000'ffff
    };

public:
    explicit ScreenShoter(QWidget *parent = nullptr);

    static double distance(const QPoint& p, const QPoint& p1, const QPoint&p2);

signals:
    void SNIPPED(const QPixmap image, const QPoint& pos);
    void FIX_IMAGE(const QPixmap image, const QPoint& pos);
    void SHOW_MESSAGE(const QString &title, const QString &msg,
                      QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information, int msecs = 10000);

public slots:
    virtual void start() override;
    virtual void exit() override;

    void save();
    void copy();
    void pin();
    void snipped();

    void undo();
    void redo();

private slots:
    void moveMenu();
    void modified() { modified_ = true; update(); }

protected:
    virtual void mousePressEvent(QMouseEvent *) override;
    virtual void mouseMoveEvent(QMouseEvent *) override;
    virtual void mouseReleaseEvent(QMouseEvent *) override;
    virtual void keyPressEvent(QKeyEvent *) override;
    virtual void paintEvent(QPaintEvent *) override;
    virtual void wheelEvent(QWheelEvent *) override;

private:
    void paintOnCanvas();

    QImage mosaic(const QImage&);
    void registerShortcuts();

    shared_ptr<PaintCommand> getCursorPos(const QPoint&);
    void setCursorByPos(Resizer::PointPosition, const QCursor & default_cursor = Qt::CrossCursor);

    void moveMagnifier();

    QPixmap snippedImage();

    QPixmap captured_screen_;
    QImage mosaic_background_;

    Resizer::PointPosition cursor_graph_pos_ = Resizer::OUTSIDE;

    shared_ptr<PaintCommand> command_ = nullptr;
    shared_ptr<PaintCommand> focus_ = nullptr;

    uint32_t edit_status_ = NONE, last_edit_status_ = NONE;

    TextEdit * text_edit_ = nullptr;

    ImageEditMenu * menu_ = nullptr;
    Magnifier * magnifier_ = nullptr;

    QPoint move_begin_{0, 0}, move_end_{0, 0};
    QPoint resize_begin_{0, 0}, resize_end_{0, 0};
    std::vector<QPoint> curves_;

    CommandStack undo_stack_;
    CommandStack redo_stack_;

    CircleCursor circle_cursor_{20};
    QPixmap canvas_;

    struct History{
        QPixmap image_;
        QRect rect_;
        CommandStack commands_;
    };

    std::vector<History> history_;
    size_t history_idx_ = 0;
};

#endif // TESTCAPTURER_H
