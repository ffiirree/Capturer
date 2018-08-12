#ifndef CAPTURER_GIFCAPTURER_H
#define CAPTURER_GIFCAPTURER_H

#include <QProcess>
#include "selector.h"

class GifCapturer : public Selector
{
    Q_OBJECT

public:
    explicit GifCapturer(QWidget * parent = nullptr);

    inline int framerate() const { return framerate_; }
    inline int fps() const { return fps_; }

public slots:
    virtual void exit() override;

    void record();
    void setFramerate(int fr) { framerate_ = fr; }
    void setFPS(int setFPS) { fps_ = setFPS; }

protected:
    virtual void keyPressEvent(QKeyEvent *) override;

private:
    void setup();

    int framerate_ = 30;
    int fps_ = 6;

    QString filename_;
    QString current_time_str_;
    QString temp_video_path_;
    QString temp_palette_path_;
    QProcess *process_ = nullptr;
};

#endif //! CAPTURER_GIFCAPTURER_H
