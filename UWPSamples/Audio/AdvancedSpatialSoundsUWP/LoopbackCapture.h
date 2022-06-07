#pragma once

#include <windows.h>
#include <strsafe.h>
#include <cstdint>
#include <stdio.h>
#include <windows.h>
#include <cstdint>
#include <stdio.h>
#include <Mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <memory>
#include <fileapi.h>

#define RETURN_IF_FAILED( exp ) { HRESULT _hr_ = (exp); if( FAILED( _hr_ ) ) return _hr_; }
#define RETURN_HR_IF(hr, x) { if((x)) { return hr; } }

class CLoopbackCapture
{
public:
    HRESULT ReinitializeDevice();
    HRESULT StartCapture();

    HANDLE _hFile;
    HANDLE _bufferCompleteEvent;

    Microsoft::WRL::ComPtr<IAudioClient> _audioClient;
    Microsoft::WRL::ComPtr<IAudioCaptureClient> _loopbackClient;
    WAVEFORMATEX* _mixFormat;
    uint32_t _audioBufferSize;
    uint32_t _bufferSize;
    std::unique_ptr<uint8_t[]> _buffer;
    uint32_t _curOffset;
    uint32_t _numSecsCapture;
    bool _isCaptureActive;
public:
    HRESULT Initialize(uint32_t numSecsCapture);
    HRESULT Start();
    HRESULT Stop();
    ~CLoopbackCapture();
private:
    //worker thread for capturing loopback data
    PTP_WORK m_workThread;
};