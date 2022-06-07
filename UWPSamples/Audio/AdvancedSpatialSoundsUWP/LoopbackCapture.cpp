#include "pch.h"

#include "LoopbackCapture.h"

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                ((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) |       \
                ((uint32_t)(uint8_t)(ch2) << 16) | ((uint32_t)(uint8_t)(ch3) << 24 ))
#endif

#define NUM_100NS_IN_SECOND 10000000

#pragma pack(push, 1) 
typedef struct _RIFFHeader {
    DWORD        dwRiff;
    DWORD        dwRiffSize;
    DWORD        dwWave;
    DWORD        dwFmt;
    DWORD        dwFmtSize;
    WAVEFORMATEX wfx;
} RIFFHeader;

typedef struct _DATAHeader {
    DWORD       dwData;
    DWORD       dwDataSize;
} DATAHeader;
#pragma pack(pop) 

VOID CALLBACK LoopBackCaptureCallback(_Inout_ PTP_CALLBACK_INSTANCE, _Inout_opt_ PVOID Context, _Inout_ PTP_WORK)
{
    HRESULT hr = S_OK;
    auto lpInstance = reinterpret_cast<CLoopbackCapture*>(Context);

    // Calculate iterations needed to capture all audio requested
    REFERENCE_TIME devicePeriod;
    hr = lpInstance->_audioClient->GetDevicePeriod(&devicePeriod, NULL);

    hr = lpInstance->_audioClient->Start();

    while (lpInstance->_isCaptureActive)
    {
        if (WaitForSingleObject(lpInstance->_bufferCompleteEvent, 10000) != WAIT_OBJECT_0)
        {
            hr = E_FAIL;
        }

        uint32_t packetLength;
        hr = lpInstance->_loopbackClient->GetNextPacketSize(&packetLength);

        if (packetLength > 0)
        {
            BYTE* data;
            DWORD dwFlags;
            uint32_t framesAvailable;

            hr = lpInstance->_loopbackClient->GetBuffer(&data, &framesAvailable, &dwFlags, NULL, NULL);

            uint32_t writeBytes = framesAvailable * lpInstance->_mixFormat->nBlockAlign;

            memcpy_s(lpInstance->_buffer.get() + lpInstance->_curOffset, lpInstance->_bufferSize - lpInstance->_curOffset, data, writeBytes);
            lpInstance->_curOffset += writeBytes;

            hr = lpInstance->_loopbackClient->ReleaseBuffer(framesAvailable);

            if (lpInstance->_bufferSize - lpInstance->_curOffset <= writeBytes)
            {
                // Write the buffer to disk
                // TODO: This could be done async but for now we do it between buffer iterations
                WriteFile(lpInstance->_hFile, lpInstance->_buffer.get(), lpInstance->_curOffset, NULL, NULL);
                lpInstance->_curOffset = 0;
            }
        }
    }

    hr = lpInstance->_audioClient->Stop();

    // Write any final data to disk
    WriteFile(lpInstance->_hFile, lpInstance->_buffer.get(), lpInstance->_curOffset, NULL, NULL);
    lpInstance->_curOffset = 0;

}

CLoopbackCapture::~CLoopbackCapture()
{
    if (_bufferCompleteEvent != INVALID_HANDLE_VALUE)
    {
        CloseHandle(_bufferCompleteEvent);
    }
    //CoTaskMemFree(_mixFormat);
    CloseHandle(_hFile);
}

HRESULT CLoopbackCapture::Initialize(uint32_t numSecsCapture)
{
    _bufferCompleteEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    if (_bufferCompleteEvent == INVALID_HANDLE_VALUE)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    _numSecsCapture = numSecsCapture;

    return S_OK;
}


HRESULT CLoopbackCapture::ReinitializeDevice()
{
    WCHAR   filename[MAX_PATH];
    RETURN_IF_FAILED(StringCchPrintfW(filename, ARRAYSIZE(filename), L"%s-%d-%d.%s", L"d:\\loopback", GetTickCount(), GetCurrentProcessId(), L"wav"));

    _hFile = CreateFileW(filename, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (_hFile == INVALID_HANDLE_VALUE)
    {
        printf("unable to create %ls! error = 0x%x\n", filename, GetLastError());
        exit(GetLastError());
    }

    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;

    RETURN_IF_FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC, __uuidof(IMMDeviceEnumerator), (void**)&enumerator));

    Microsoft::WRL::ComPtr<IMMDevice> device;
    RETURN_IF_FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device));

    Microsoft::WRL::ComPtr<IAudioClient> client;
    RETURN_IF_FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&client));


    WAVEFORMATEX* mixFormat = (WAVEFORMATEX*)CoTaskMemAlloc(sizeof(WAVEFORMATEX));
    RETURN_IF_FAILED(client->GetMixFormat(&mixFormat));

    // Allocate a one second buffer
    _bufferSize = mixFormat->nAvgBytesPerSec;
    _curOffset = 0;

    std::unique_ptr<uint8_t[]> buffer(new (std::nothrow) uint8_t[_bufferSize]);

    RETURN_IF_FAILED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 200000, 0, mixFormat, nullptr));

    // For some reason unique_event cannot be found
    RETURN_IF_FAILED(client->SetEventHandle(_bufferCompleteEvent));

    uint32_t bufferSize;
    RETURN_IF_FAILED(client->GetBufferSize(&bufferSize));

    Microsoft::WRL::ComPtr<IAudioCaptureClient> captureClient;
    RETURN_IF_FAILED(client->GetService(__uuidof(IAudioCaptureClient), (void**)&captureClient));

    RIFFHeader riff;

    // Fill RIFF and fmt chunks
    riff.dwRiff = MAKEFOURCC('R', 'I', 'F', 'F');
    riff.dwWave = MAKEFOURCC('W', 'A', 'V', 'E');
    riff.dwFmt = MAKEFOURCC('f', 'm', 't', ' ');
    riff.dwFmtSize = sizeof(WAVEFORMATEX) + mixFormat->cbSize;
    memcpy_s(&riff.wfx, sizeof(WAVEFORMATEX), mixFormat, sizeof(WAVEFORMATEX));

    // Build the data section
    // TODO: Write duration and size at file close
    uint32_t duration = _numSecsCapture;
    DATAHeader data;
    data.dwData = MAKEFOURCC('d', 'a', 't', 'a');
    data.dwDataSize = duration * mixFormat->nAvgBytesPerSec;

    // do this last now that the length calculation has been performed
    riff.dwRiffSize = sizeof(riff) + data.dwDataSize;

    // Write the initial format data
    WriteFile(_hFile, &riff, sizeof(RIFFHeader), NULL, NULL);

    // Now copy the extended fmt data if present
    if (mixFormat->cbSize > 0)
    {
        WAVEFORMATEXTENSIBLE* extFmt = (WAVEFORMATEXTENSIBLE*)mixFormat;
        WriteFile(_hFile, &extFmt->Samples, mixFormat->cbSize, NULL, NULL);
    }

    // Finally write the data section
    WriteFile(_hFile, &data, sizeof(DATAHeader), NULL, NULL);

    _audioClient = client;
    _loopbackClient = captureClient;
    _mixFormat = mixFormat;
    _audioBufferSize = bufferSize;
    _buffer = std::move(buffer);

    return S_OK;
}

HRESULT CLoopbackCapture::Start()
{
    HRESULT hrCapture = AUDCLNT_E_DEVICE_INVALIDATED;

    while (hrCapture == AUDCLNT_E_DEVICE_INVALIDATED)
    {
        if (_audioClient)
        {
            //printf("Reinitializing on audio client invalidation\n");
            // Write the rest of the data to the file before closing the handle
            WriteFile(_hFile, _buffer.get(), _curOffset, NULL, NULL);
            _curOffset = 0;

            // Null out the client before reinitializing, so that we clean up
            // our resources first
            _audioClient = nullptr;
        }

        hrCapture = ReinitializeDevice();
        if (SUCCEEDED(hrCapture))
        {
            StartCapture();
        }
    }

    return hrCapture;
}


HRESULT CLoopbackCapture::StartCapture()
{
    _isCaptureActive = true;
    m_workThread = CreateThreadpoolWork(LoopBackCaptureCallback, this, nullptr);
    SubmitThreadpoolWork(m_workThread);

    return S_OK;
}
