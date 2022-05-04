#ifndef SCREEN_RECORDER_H
#define SCREEN_RECORDER_H

#include <QSystemTrayIcon>
#include "selector.h"
#include "record_menu.h"
#include "config.h"
#include "mediadecoder.h"
#include "mediaencoder.h"
#include "videoplayer.h"

class ScreenRecorder : public Selector
{
    Q_OBJECT

public:
    explicit ScreenRecorder(QWidget *parent = nullptr);
    ~ScreenRecorder() override {
        delete menu_;
        delete decoder_thread_; decoder_thread_ = nullptr;
        delete encoder_thread_; encoder_thread_ = nullptr;
    }

    inline int framerate() const { return framerate_; }

signals:
    void SHOW_MESSAGE(const QString &title, const QString &msg,
                      QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information, int msecs = 10000);
public slots:
    virtual void exit() override;

    void start() override;
    void record();
    void setFramerate(int fr) { framerate_ = fr; }

    void mute(int type, bool v) { type ? m_mute_ = v : s_mute_ = v; }
    void updateTheme()
    {
        Selector::updateTheme(Config::instance()["record"]["selector"]);
    }
private:
    void setup();

    void keyPressEvent(QKeyEvent *event) override;

    int framerate_{ 30 };

    QString filename_;

    VideoPlayer* player_{ nullptr };

    RecordMenu *menu_ = nullptr;
    bool m_mute_ = true;
    bool s_mute_ = true;

    MediaDecoder* decoder_{ nullptr };
    MediaEncoder* encoder_{ nullptr };
    QThread* decoder_thread_{ nullptr };
    QThread* encoder_thread_{ nullptr };
};

#endif // SCREEN_RECORDER_H
