#ifndef FONTMENU_H
#define FONTMENU_H

#include <QFrame>
#include <QHBoxLayout>
#include <vector>
#include <map>
#include <QPushButton>

class FontMenu : public QFrame
{
    Q_OBJECT

public:
    explicit FontMenu(QWidget* parent = nullptr);
    ~FontMenu() = default;

signals:
    void familyChanged(const QString&);
    void sizeChanged(int);
    void styleChanged(const QString&);
    void colorChanged(const QColor&);

private:
    QHBoxLayout * layout_ = nullptr;
};

#endif // FONTMENU_H
