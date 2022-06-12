#ifndef TEXTEDIT_H
#define TEXTEDIT_H

#include <QTextEdit>

class TextEdit : public QTextEdit
{
    Q_OBJECT

public:
    explicit TextEdit(QWidget *parent = nullptr);

public slots:
    void setFont(const QFont& font) { QTextEdit::setFont(font); emit fontChanged(); };

signals:
    void focus(bool);
    void resized();
    void fontChanged();

protected:
    void focusInEvent(QFocusEvent *e) override;
    void focusOutEvent(QFocusEvent *e) override;
};

#endif // TEXTEDIT_H
