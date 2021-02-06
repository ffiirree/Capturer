#ifdef __APPLE__
#include "devices.h"

vector<pair<QString, QString>> Devices::video_devices_;
vector<pair<QString, QString>> Devices::audio_devices_;

void Devices::refresh()
{
    // TODO
}
#endif