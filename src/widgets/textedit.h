#ifndef TEXTEDIT_H
#define TEXTEDIT_H

#include <QTextEdit>

class TextEdit : public QTextEdit
{
    Q_OBJECT

public:
    explicit TextEdit(QWidget *parent = nullptr);

public slots:

    void setFont(const QFont& font)
    {
        QTextEdit::setFont(font);
        emit fontChanged();
    };

    void setColor(const QColor& color)
    {
        color_ = color;
        setTextColor(color);
    }

signals:
    void focus(bool);
    void resized();
    void fontChanged();

protected:
    void focusInEvent(QFocusEvent *e) override;
    void focusOutEvent(QFocusEvent *e) override;

    QColor color_{};
};

#endif // TEXTEDIT_H
