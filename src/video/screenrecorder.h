#ifndef SCREEN_RECORDER_H
#define SCREEN_RECORDER_H

#include <QProcess>
#include <QSystemTrayIcon>
#include "selector.h"
#include "record_menu.h"
#include "config.h"

class ScreenRecorder : public Selector
{
    Q_OBJECT

public:
    explicit ScreenRecorder(QWidget *parent = nullptr);
    ~ScreenRecorder() override {
        delete menu_;
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

    void mute(bool v) { mute_ = v; }
    void updateTheme()
    {
        Selector::updateTheme(Config::instance()["record"]["selector"]);
    }
private:
    void setup();

    void keyPressEvent(QKeyEvent *event) override;

    int framerate_ = 30;

    QProcess *process_ = nullptr;

    QString filename_;

    RecordMenu *menu_ = nullptr;
    bool mute_ = true;
};

#endif // SCREEN_RECORDER_H
