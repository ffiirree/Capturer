#ifndef TEXTEDIT_H
#define TEXTEDIT_H

#include <QTextEdit>
#include <QPainter>

class TextEdit : public QTextEdit
{
    Q_OBJECT

public:
    explicit TextEdit(QWidget *parent = nullptr);

    void setFont(const QFont& font);

    QString text() const { return this->toPlainText(); }
    void text(const QString& v) { setText(v); }

signals:
    void focus(bool);

protected:
    virtual void focusInEvent(QFocusEvent *e) override;
    virtual void focusOutEvent(QFocusEvent *e) override;

private:
    QPainter painter_;
};

#endif // TEXTEDIT_H
