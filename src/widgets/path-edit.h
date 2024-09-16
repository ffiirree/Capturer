#ifndef CAPTURER_PATH_EDIT_H
#define CAPTURER_PATH_EDIT_H

#include <QWidget>

class QLineEdit;
class QAbstractButton;

class PathEdit final : public QWidget
{
    Q_OBJECT
public:
    explicit PathEdit(QWidget *parent = nullptr);

    explicit PathEdit(const QString&, bool file = false, QWidget *parent = nullptr);

    [[nodiscard]] QString path() const;

signals:
    void changed(const QString& path);

private:
    QLineEdit       *path_{};
    QAbstractButton *browse_{};
};

#endif //! CAPTURER_PATH_EDIT_H