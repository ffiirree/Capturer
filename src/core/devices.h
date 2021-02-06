#ifndef DEVICES_H
#define DEVICES_H

#include "utils.h"
#include <QString>

enum class DeviceType {
    VIDEO_INPUT_DEVICE,
    AUDIO_INPUT_DEVICE,
};

class Devices {
public:
    static void refresh();

    static vector<pair<QString, QString>> videoDevices()
    {
        return video_devices_;
    }

    static vector<pair<QString, QString>> audioDevices()
    {
        return audio_devices_;
    }

private:
    static vector<pair<QString, QString>> video_devices_;
    static vector<pair<QString, QString>> audio_devices_;
};

#endif // DEVICES_H
