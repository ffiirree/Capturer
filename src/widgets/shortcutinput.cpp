#include "shortcutinput.h"
#include <QKeyEvent>

ShortcutInput::ShortcutInput(QWidget * parent)
    : QLineEdit(parent)
{
    setReadOnly(true);
    setAlignment(Qt::AlignHCenter);
}

ShortcutInput::ShortcutInput(const QKeySequence & ks, QWidget *parent)
    : ShortcutInput(ks.toString(), parent)
{}

ShortcutInput::ShortcutInput(const QString & str, QWidget *parent)
    : QLineEdit(str, parent)
{
    setReadOnly(true);
    setAlignment(Qt::AlignHCenter);
}

void ShortcutInput::keyPressEvent(QKeyEvent *event)
{
    auto key = event->key();
    auto mods = event->modifiers();

    if(key >= Qt::Key_Escape && key < Qt::Key_F1) return;

    auto shortcut = QKeySequence(mods + key).toString();
    setText(shortcut);
    emit changed(shortcut);
}
