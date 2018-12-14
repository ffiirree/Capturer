#ifndef CAPTURER_VIDEOCAPTURER_H
#define CAPTURER_VIDEOCAPTURER_H

#include <QProcess>
#include <QSystemTrayIcon>
#include "selector.h"
#include "record_menu.h"

class ScreenRecorder : public Selector
{
    Q_OBJECT

public:
    explicit ScreenRecorder(QWidget *parent = nullptr);
    virtual ~ScreenRecorder() override {
        delete menu_;
    }

    inline int framerate() { return framerate_; }

signals:
    void SHOW_MESSAGE(const QString &title, const QString &msg,
                      QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information, int msecs = 10000);
public slots:
    virtual void exit() override;

    void record();
    void setFramerate(int fr) { framerate_ = fr; }

private:
    void setup();

    void keyPressEvent(QKeyEvent *event) override;

    int framerate_ = 30;

    QProcess *process_ = nullptr;

    QString filename_;

    RecordMenu *menu_ = nullptr;
};

#endif //! CAPTURER_VIDEOCAPTURER_H
