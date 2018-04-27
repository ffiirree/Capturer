#ifndef CAPTURER_GIFCAPTURER_H
#define CAPTURER_GIFCAPTURER_H

#include <QProcess>
#include "selector.h"

class GifCapturer : public Selector
{
    Q_OBJECT

public:
    explicit GifCapturer(QWidget * parent = nullptr);

public slots:
    void record();


protected:
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void paintEvent(QPaintEvent *event);

private:
    void setup();
    void end();

    QString filename_;
    QString current_time_str_;
    QString temp_video_path_;
    QString temp_palette_path_;
    QProcess *process_ = nullptr;
};

#endif //! CAPTURER_GIFCAPTURER_H
