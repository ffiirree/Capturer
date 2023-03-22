#ifndef CAPTURER_ENUM_WASAPI_H
#define CAPTURER_ENUM_WASAPI_H

#ifdef _WIN32

#include <vector>
#include <optional>
#include <QString>

// all std::string is utf8
std::vector<std::pair<QString, QString>> enum_audio_endpoints(bool is_input);
std::optional<std::pair<QString, QString>> default_audio_endpoint(bool is_input);

#endif // _WIN32

#endif // !CAPTURER_ENUM_WASAPI_H
