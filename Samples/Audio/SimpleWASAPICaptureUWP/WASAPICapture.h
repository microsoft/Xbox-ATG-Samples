//--------------------------------------------------------------------------------------
// WASAPICapture.h
//
// Functions to assist in capturing audio in WASAPI
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <Windows.h>
#include <wrl\implements.h>
#include <mfapi.h>
#include <AudioClient.h>
#include <mmdeviceapi.h>
#include <mftransform.h>
#include "DeviceState.h"
#include "Common.h"
#include "CBuffer.h"
#include <mutex>

#pragma once

// Primary WASAPI Capture Class
class WASAPICapture :
    public Microsoft::WRL::RuntimeClass< Microsoft::WRL::RuntimeClassFlags< Microsoft::WRL::ClassicCom >, Microsoft::WRL::FtmBase, IActivateAudioInterfaceCompletionHandler >
{
public:
    WASAPICapture();

    HRESULT InitializeAudioDeviceAsync(Platform::String^ deviceIdString);
    HRESULT StartCaptureAsync(CBuffer* bufferToUse);
    HRESULT StopCaptureAsync();

    DeviceStateChangedEvent^ GetDeviceStateEvent() { return m_deviceStateChanged; };
    WAVEFORMATEX* GetMixFormat() { return m_mixFormat; }

    METHODASYNCCALLBACK( WASAPICapture, StartCapture, OnStartCapture );
    METHODASYNCCALLBACK( WASAPICapture, StopCapture, OnStopCapture );
    METHODASYNCCALLBACK( WASAPICapture, SampleReady, OnSampleReady );

    // IActivateAudioInterfaceCompletionHandler
    STDMETHOD(ActivateCompleted)( IActivateAudioInterfaceAsyncOperation *operation );

private:
    ~WASAPICapture();

    HRESULT OnStartCapture( IMFAsyncResult* );
    HRESULT OnStopCapture( IMFAsyncResult* );
    HRESULT OnSampleReady( IMFAsyncResult* );

    HRESULT OnAudioSampleRequested( bool isSilence = false );
        
private:
    uint32_t                    m_bufferFrames;
    HANDLE                      m_sampleReadyEvent;
    MFWORKITEM_KEY              m_sampleReadyKey;
    std::mutex                  m_mutex;
    DWORD                       m_queueID;

    CBuffer*                    m_buffer;

    WAVEFORMATEX*               m_mixFormat;
    Microsoft::WRL::ComPtr<IAudioClient>        m_audioClient;
    Microsoft::WRL::ComPtr<IAudioCaptureClient> m_audioCaptureClient;
    Microsoft::WRL::ComPtr<IMFAsyncResult>      m_sampleReadyAsyncResult;
    Microsoft::WRL::ComPtr<IMFTransform>        m_resampler;

    DeviceStateChangedEvent^    m_deviceStateChanged;
};
