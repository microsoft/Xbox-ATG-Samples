//--------------------------------------------------------------------------------------
// WASAPIRenderer.h
//
// Class responsible for actually outputting samples to WASAPI.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#ifndef WASAPI_RENDERER_H_INCLUDED
#define WASAPI_RENDERER_H_INCLUDED

#include "pch.h"
#include "WaveSampleGenerator.h"
#include "DeviceState.h"
#include "CBuffer.h"

#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <wrl\implements.h>

// Primary WASAPI Rendering Class
class WASAPIRenderer :
    public Microsoft::WRL::RuntimeClass< Microsoft::WRL::RuntimeClassFlags< Microsoft::WRL::ClassicCom >, Microsoft::WRL::FtmBase, IActivateAudioInterfaceCompletionHandler >
{
public:
    WASAPIRenderer();
    ~WASAPIRenderer();

    HRESULT InitializeAudioDeviceAsync();
    HRESULT StartPlaybackAsync(bool bLoopback);
    HRESULT StopPlaybackAsync();
    HRESULT PausePlaybackAsync();

    DeviceStateChangedEvent^ GetDeviceStateEvent() { return m_DeviceStateChanged; };
    WAVEFORMATEX* GetFormat() { return m_MixFormat; };
    void SetCaptureBuffer(	CBuffer* inBuffer ) { m_captureBuffer = inBuffer;};
    HRESULT ConfigureSource(const wchar_t* filename );

    // IActivateAudioInterfaceCompletionHandler
    STDMETHOD(ActivateCompleted)( IActivateAudioInterfaceAsyncOperation *operation );

private:
    HRESULT OnStartPlayback();
    HRESULT OnStopPlayback();
    HRESULT OnPausePlayback();
    HRESULT OnSampleReady();

    HRESULT ConfigureDeviceInternal();
    HRESULT OnAudioSampleRequested( Platform::Boolean bIsSilence = false );
    UINT32  GetBufferFramesPerPeriod();

    HRESULT GetWaveSample( UINT32 FramesAvailable );
    HRESULT WriteSilence( UINT32 FramesAvailable );
    HRESULT GetCaptureSample( UINT32 FramesAvailable );

    static DWORD WINAPI AudioSampleThreadProc( LPVOID lpParameter );

    DWORD AudioSampleThread();

    void LoadPCM(const wchar_t* filename, WAVEFORMATEXTENSIBLE* inWfx );

    void  SetDeviceStateError(  HRESULT hr );

    //--------------------------------------------------------------------------------------
    //  Name:   EnqueueCommand
    //  Desc:   Acquire a write lock on the command queue and submit the supplied command.
    //			Then raise the CmdReady event so the processing thread wakes.
    //--------------------------------------------------------------------------------------
    HRESULT EnqueueCommand( UINT32 cmd )
    {
        HRESULT	hr = S_OK;
        AcquireSRWLockExclusive( &m_CmdQueueLock );
        m_CmdQueue.push_back( cmd );
        ReleaseSRWLockExclusive( &m_CmdQueueLock );
        if ( !SetEvent( m_CmdReadyEvent ) )
        {
            hr = HRESULT_FROM_WIN32( GetLastError() );  
        }
        return hr;
    }

private:

    static const REFERENCE_TIME	REFTIMES_PER_SEC = 10000000LL;

    enum
    {
        CMD_START,
        CMD_STOP,
        CMD_PAUSE
    };

    Platform::String^   m_DeviceIdString;
    UINT32              m_BufferFrames;
    HANDLE              m_SampleReadyEvent;

    CRITICAL_SECTION    m_CritSec;

    HANDLE				m_CmdReadyEvent;
    std::deque<UINT32>	m_CmdQueue;
    SRWLOCK				m_CmdQueueLock;

    WAVEFORMATEX*       m_MixFormat;
    Microsoft::WRL::ComPtr<IAudioClient2>       m_AudioClient;
    Microsoft::WRL::ComPtr<IAudioRenderClient>  m_AudioRenderClient;

    DeviceStateChangedEvent^       m_DeviceStateChanged;

    bool					m_bLoopback;
    bool					m_bPlayback;

	std::unique_ptr<uint8_t[]>	m_waveFile;
	DWORD						m_WaveSize;
    WaveSampleGenerator*		m_WaveSource;
    CBuffer*					m_captureBuffer;
};

#endif
