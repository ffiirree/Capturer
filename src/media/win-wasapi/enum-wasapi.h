#ifndef CAPTURER_ENUM_WASAPI_H
#define CAPTURER_ENUM_WASAPI_H

#ifdef _WIN32

#include <vector>
#include <optional>
#include <string>

// all std::string is utf8
std::vector<std::pair<std::wstring, std::wstring>> enum_audio_endpoints(bool is_input);
std::optional<std::pair<std::wstring, std::wstring>> default_audio_endpoint(bool is_input);

#endif // _WIN32

#endif // !CAPTURER_ENUM_WASAPI_H
