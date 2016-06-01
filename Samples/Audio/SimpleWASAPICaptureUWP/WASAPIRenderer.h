//--------------------------------------------------------------------------------------
// WASAPIRenderer.h
//
// Functions to assist in rendering audio in WASAPI
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <Windows.h>
#include <wrl\implements.h>
#include <mfapi.h>
#include <AudioClient.h>
#include <mmdeviceapi.h>
#include "DeviceState.h"
#include "Common.h"
#include "CBuffer.h"
#include <mutex>

#pragma once

// Primary WASAPI Renderering Class
class WASAPIRenderer :
    public Microsoft::WRL::RuntimeClass< Microsoft::WRL::RuntimeClassFlags< Microsoft::WRL::ClassicCom >, Microsoft::WRL::FtmBase, IActivateAudioInterfaceCompletionHandler >
{
public:
    WASAPIRenderer();

    HRESULT InitializeAudioDeviceAsync();
    HRESULT StartPlaybackAsync(CBuffer* bufferToUse);
    HRESULT StopPlaybackAsync();

    DeviceStateChangedEvent^ GetDeviceStateEvent() { return m_deviceStateChanged; };
    WAVEFORMATEX* GetMixFormat() { return m_mixFormat; }

    METHODASYNCCALLBACK( WASAPIRenderer, StartPlayback, OnStartPlayback );
    METHODASYNCCALLBACK( WASAPIRenderer, StopPlayback, OnStopPlayback );
    METHODASYNCCALLBACK( WASAPIRenderer, SampleReady, OnSampleReady );

    // IActivateAudioInterfaceCompletionHandler
    STDMETHOD(ActivateCompleted)( IActivateAudioInterfaceAsyncOperation *operation );

private:
    ~WASAPIRenderer();

    HRESULT OnStartPlayback( IMFAsyncResult* );
    HRESULT OnStopPlayback( IMFAsyncResult* );
    HRESULT OnSampleReady( IMFAsyncResult* );

    HRESULT ConfigureDeviceInternal();
    HRESULT OnAudioSampleRequested( bool isSilence = false );

private:
    uint32_t                    m_bufferFrames;
    HANDLE                      m_sampleReadyEvent;
    MFWORKITEM_KEY              m_sampleReadyKey;
    std::mutex                  m_mutex;

    CBuffer*                    m_buffer;

    WAVEFORMATEX*               m_mixFormat;
    Microsoft::WRL::ComPtr<IAudioClient2>       m_audioClient;
    Microsoft::WRL::ComPtr<IAudioRenderClient>  m_audioRenderClient;
    Microsoft::WRL::ComPtr<IMFAsyncResult>      m_sampleReadyAsyncResult;

    DeviceStateChangedEvent^    m_deviceStateChanged;
};
