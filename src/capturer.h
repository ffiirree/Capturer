#ifndef CAPTURER_H
#define CAPTURER_H

#include "imagewindow.h"
#include "qhotkey.h"
#include "screenrecorder.h"
#include "screenshoter.h"
#include "settingdialog.h"

#include <any>
#include <QSystemTrayIcon>

template<typename T, int MAX_SIZE = 32> class LimitSizeVector : public std::vector<T>
{
public:
    void append(const T& value)
    {
        this->push_back(value);

        if (this->size() > MAX_SIZE) {
            this->erase(this->begin());
        }
    }
};

enum class DataFormat
{
    UNKNOWN,
    PIXMAP,
    HTML,
    TEXT,
    COLOR,
    URLS,
    SNIPPED
};

class Capturer : public QWidget
{
    Q_OBJECT

public:
    explicit Capturer(QWidget *parent = nullptr);
    ~Capturer() override = default;

private slots:
    void pin();
    void pinPixmap(const QPixmap&, const QPoint&);
    void showImages();

    void updateConfig();

    void showMessage(const QString& title, const QString& msg,
                     QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information, int msecs = 10'000);

private:
    void setupSystemTray();

    static std::pair<DataFormat, std::any> clipboard_data();
    static std::pair<bool, QPixmap> to_pixmap(const std::pair<DataFormat, std::any>&);

    ScreenShoter *sniper_{ nullptr };
    ScreenRecorder *recorder_{ nullptr };
    ScreenRecorder *gifcptr_{ nullptr };

    size_t pin_idx_{ 0 };
    bool clipboard_changed_{ false };
    LimitSizeVector<std::tuple<DataFormat, std::any, std::shared_ptr<ImageWindow>>> history_;

    QSystemTrayIcon *sys_tray_icon_{ nullptr };

    SettingWindow *setting_dialog_{ nullptr };

    // hotkey
    QHotkey *snip_sc_{ nullptr };
    QHotkey *show_pin_sc_{ nullptr };
    QHotkey *pin_sc_{ nullptr };
    QHotkey *gif_sc_{ nullptr };
    QHotkey *video_sc_{ nullptr };

    bool images_visible_{ true };
};

#endif // CAPTURER_H
