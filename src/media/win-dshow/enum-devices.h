#ifndef CAPTURER_WIN_DSHOW_H
#define CAPTURER_WIN_DSHOW_H

#ifdef _WIN32

#include <vector>
#include <optional>
#include <string>

std::vector<std::pair<std::wstring, std::wstring>> enum_video_devices();
std::vector<std::pair<std::wstring, std::wstring>> enum_audio_devices();

#endif // !_WIN32

#endif // !CAPTURER_WIN_DSHOW_H