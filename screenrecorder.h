#ifndef CAPTURER_VIDEOCAPTURER_H
#define CAPTURER_VIDEOCAPTURER_H

#include "selector.h"
#include <QProcess>

class ScreenRecorder : public Selector
{
    Q_OBJECT

public:
    explicit ScreenRecorder(QWidget *parent = nullptr);

public slots:
    void record();

private:
    void setup();
    void end();

    void keyPressEvent(QKeyEvent *event);
    void paintEvent(QPaintEvent *event);

    QProcess *process_;

    QString filename_;

    bool is_recording = false;
};

#endif //! CAPTURER_VIDEOCAPTURER_H
