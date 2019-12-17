#include "devices.h"
#include <Windows.h>
#include <dshow.h>
#include "logging.h"

vector<pair<QString, QString>> Devices::video_devices_;
vector<pair<QString, QString>> Devices::audio_devices_;

HRESULT EnumerateDevices(REFGUID category, IEnumMoniker **ppEnum)
{
    // Create the System Device Enumerator.
    ICreateDevEnum *pDevEnum;
    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));

    if (SUCCEEDED(hr)) {
        // Create an enumerator for the category.
        hr = pDevEnum->CreateClassEnumerator(category, ppEnum, 0);
        if (hr == S_FALSE) {
            hr = VFW_E_NOT_FOUND;  // The category is empty. Treat as an error.
        }
        pDevEnum->Release();
    }
    return hr;
}


void DisplayDeviceInformation(IEnumMoniker *pEnum, vector<pair<QString, QString>>& devices)
{
    IMoniker *pMoniker = nullptr;

    while (pEnum->Next(1, &pMoniker, nullptr) == S_OK) {
        IPropertyBag *pPropBag = nullptr;
        pair<QString, QString> device;

        HRESULT hr = pMoniker->BindToStorage(nullptr, nullptr, IID_PPV_ARGS(&pPropBag));
        if (FAILED(hr)) {
            pMoniker->Release();
            continue;
        }

        VARIANT var;
        VariantInit(&var);

        // Get description or friendly name.
        hr = pPropBag->Read(L"Description", &var, nullptr);
        if (FAILED(hr)) {
            hr = pPropBag->Read(L"FriendlyName", &var, nullptr);
        }
        if (SUCCEEDED(hr)) {
            device.first = QString::fromUtf16(reinterpret_cast<uint16_t *>(var.bstrVal));
            VariantClear(&var);
        }

        hr = pPropBag->Write(L"FriendlyName", &var);

        // WaveInID applies only to audio capture devices.
        hr = pPropBag->Read(L"WaveInID", &var, nullptr);
        if (SUCCEEDED(hr)) {
            printf("WaveIn ID: %d\n", var.lVal);
            VariantClear(&var);
        }

        hr = pPropBag->Read(L"DevicePath", &var, nullptr);
        if (SUCCEEDED(hr)) {
            // The device path is not intended for display.
            device.second = QString::fromUtf16(reinterpret_cast<uint16_t *>(var.bstrVal));
            VariantClear(&var);
        }

        pPropBag->Release();
        pMoniker->Release();

         LOG(INFO) << device.first << "@[" << device.second << "]";
        devices.push_back(device);
    }
}

void Devices::refresh()
{
    if (SUCCEEDED( CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)))
    {
        IEnumMoniker *pEnum;
        // video input devices
        if (SUCCEEDED(EnumerateDevices(CLSID_VideoInputDeviceCategory, &pEnum))) {
            DisplayDeviceInformation(pEnum, video_devices_);
            pEnum->Release();
        }

        // audio input devices
        if (SUCCEEDED(EnumerateDevices(CLSID_AudioInputDeviceCategory, &pEnum))) {
            DisplayDeviceInformation(pEnum, audio_devices_);
            pEnum->Release();
        }
        CoUninitialize();
    }
}
