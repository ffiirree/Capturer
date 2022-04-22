#include "devices.h"
#include <QCameraInfo>
#include <QAudioDeviceInfo>

QList<QString> Devices::cameras()
{
	QSet<QString> cameras{};
	for (const auto& info : QCameraInfo::availableCameras()) {
		cameras.insert(info.description());
	}

	return cameras.toList();
}

QList<QString> Devices::microphones()
{
	QSet<QString> microphones{};
	auto x = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
	for (const auto& info : QAudioDeviceInfo::availableDevices(QAudio::AudioInput)) {
		microphones.insert(info.deviceName());
	}

	return microphones.toList();
}

QString Devices::defaultMicrophone()
{
	return QAudioDeviceInfo::defaultInputDevice().deviceName();
}

QList<QString> Devices::speakers()
{
	QSet<QString> speakers{};
	for (const auto& info : QAudioDeviceInfo::availableDevices(QAudio::AudioOutput)) {
		speakers.insert(info.deviceName());
	}

	return speakers.toList();
}

QString Devices::defaultSpeaker()
{
	return QAudioDeviceInfo::defaultOutputDevice().deviceName();
}
