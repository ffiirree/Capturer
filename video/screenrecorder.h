#ifndef CAPTURER_VIDEOCAPTURER_H
#define CAPTURER_VIDEOCAPTURER_H

#include "selector.h"
#include <QProcess>

class ScreenRecorder : public Selector
{
    Q_OBJECT

public:
    explicit ScreenRecorder(QWidget *parent = nullptr);

    inline int framerate() { return framerate_; }

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
};

#endif //! CAPTURER_VIDEOCAPTURER_H
