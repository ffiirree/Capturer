#include "textedit.h"
#include <QKeyEvent>
#include "logging.h"

TextEdit::TextEdit(QWidget * parent)
    : QTextEdit(parent)
{
    setWindowFlags(Qt::Widget | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setFrameShape(QFrame::NoFrame);
    setAttribute(Qt::WA_TranslucentBackground);
    ensureCursorVisible();
    QPalette p = palette();
    p.setBrush(QPalette::Base, QColor(0, 0, 0, 1));
    setPalette(p);

    document()->setDocumentMargin(0);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    setLineWrapMode(QTextEdit::NoWrap);
    setAlignment(Qt::AlignVCenter);

    auto resize_fuctor = [this](){
        setFixedSize(document()->size().toSize());
        emit resized();
    };

    connect(this, &TextEdit::textChanged, resize_fuctor);
    connect(this, &TextEdit::fontChanged, resize_fuctor);
    connect(document(), &QTextDocument::contentsChanged, resize_fuctor );
}

void TextEdit::focusInEvent(QFocusEvent *e)
{
    emit focus(true);
    QTextEdit::focusInEvent(e);
}

void TextEdit::focusOutEvent(QFocusEvent *e)
{
    emit focus(false);
    hide();
    QTextEdit::focusOutEvent(e);
}
