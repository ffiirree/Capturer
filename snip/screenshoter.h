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
#include "config.h"

#define  DO(COMMAND)    do {                                    \
                            auto __command__ = COMMAND;         \
                            focusOn(__command__);               \
                            commands_.push(__command__);        \
                            redo_stack_.clear();                \
                        } while(0)

class ScreenShoter : public Selector
{
    Q_OBJECT

    using super = Selector;

public:
    enum EditStatus: std::uint32_t {
        NONE            = 0x0000'0000,
        READY           = 0x0001'0000,
        GRAPH_PAINTING  = 0x0010'0000,
        GRAPH_MOVING    = 0x0020'0000,
        GRAPH_RESIZING  = 0x0040'0000,
        GRAPH_ROTATING  = 0x0080'0000,

        READY_MASK      = 0x000f'0000,
        OPERATION_MASK  = 0xfff0'0000,
        GRAPH_MASK      = 0x0000'ffff
    };

public:
    explicit ScreenShoter(QWidget *parent = nullptr);

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

    void updateTheme()
    {
        Selector::updateTheme(Config::instance()["snip"]["selector"]);
    }

public slots:
    void modified(PaintType type) { modified_ = type; update(); }

private slots:
    void moveMenu();

protected:
    virtual void mousePressEvent(QMouseEvent *) override;
    virtual void mouseMoveEvent(QMouseEvent *) override;
    virtual void mouseReleaseEvent(QMouseEvent *) override;
    virtual void keyPressEvent(QKeyEvent *) override;
    virtual void paintEvent(QPaintEvent *) override;
    virtual void wheelEvent(QWheelEvent *) override;

private:
    void updateCanvas();

    QImage mosaic(const QImage&);
    void registerShortcuts();

    void updateHoverPos(const QPoint&);
    void setCursorByHoverPos(Resizer::PointPosition, const QCursor & default_cursor = Qt::CrossCursor);

    void moveMagnifier();

    QPixmap snippedImage();

    void focusOn(shared_ptr<PaintCommand> cmd);

    QPixmap captured_screen_;
    QImage mosaic_background_;

    Resizer::PointPosition hover_position_ = Resizer::OUTSIDE;

    shared_ptr<PaintCommand> hover_cmd_ = nullptr;    // hover
    shared_ptr<PaintCommand> focus_cmd_ = nullptr;    // focus

    std::uint32_t edit_status_ = EditStatus::NONE;

    TextEdit * text_edit_ = nullptr;

    ImageEditMenu * menu_ = nullptr;
    Magnifier * magnifier_ = nullptr;

    QPoint move_begin_{0, 0};
    QPoint resize_begin_{0, 0};
    std::vector<QPoint> curves_;

    CommandStack commands_;
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
