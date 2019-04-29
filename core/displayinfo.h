#ifndef DISPLAYINFO_H
#define DISPLAYINFO_H

#include <QScreen>

class DisplayInfo : public QObject
{
    Q_OBJECT
public:
    static DisplayInfo& instance(QObject *parent = nullptr)
    {
        static DisplayInfo instance(parent);
        return instance;
    }

    static QSize maxSize();

    static QList<QScreen*> screens();

signals:
    void changed();
    void added(QScreen *);
    void removed(QScreen *);

public slots:
    void update();

private:
    explicit DisplayInfo(QObject *parent = nullptr);
    static QSize max_size_;
};

#endif // DISPLAYINFO_H
