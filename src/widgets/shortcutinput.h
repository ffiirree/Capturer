#ifndef CAPTURER_SHORTCUT_INPUT_H
#define CAPTURER_SHORTCUT_INPUT_H

#include <QLineEdit>

class ShortcutInput final : public QLineEdit
{
    Q_OBJECT

public:
    explicit ShortcutInput(QWidget *parent = nullptr);
    explicit ShortcutInput(const QKeySequence&, QWidget *parent = nullptr);
    explicit ShortcutInput(const QString&, QWidget *parent = nullptr);

    void set(const QString& str)
    {
        setText(str);
        emit changed(QKeySequence(str));
    }

    void set(const QKeySequence& ks)
    {
        setText(ks.toString());
        emit changed(ks);
    }

    [[nodiscard]] QKeySequence get() const { return text(); }

signals:
    void changed(const QKeySequence&);

private:
    void keyPressEvent(QKeyEvent *event) override;
};

#endif //! CAPTURER_SHORTCUT_INPUT_H
