#ifndef DEVICES_H
#define DEVICES_H

#include <QString>
extern "C" {
#include <libavutil/avutil.h>
}

struct avdevice_t
{
    enum io_t
    {
        UNKNOWN,
        SOURCE,
        SINK
    };

    std::string name{}; // utf-8
    std::string id{};   // utf-8
    AVMediaType codec_type{ AVMEDIA_TYPE_UNKNOWN };
    io_t io_type{ UNKNOWN };
    uint64_t state{};

    static std::string io_type_name(io_t t)
    {
        // clang-format off
        switch(t) {
        case SINK:      return "sink";
        case SOURCE:    return "source";
        default:        return "unknown";
        }
        // clang-format on
    }
};

class Devices
{
public:
    static QList<QString> cameras();

    static QList<QString> microphones();

    static QList<QString> speakers();

    static QString default_audio_sink();
    static QString default_audio_source();
};

#endif // !DEVICES_H
