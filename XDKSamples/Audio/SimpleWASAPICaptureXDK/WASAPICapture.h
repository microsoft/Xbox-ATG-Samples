//--------------------------------------------------------------------------------------
// WASAPICapture.h
//
// Class responsible for actually inputting samples to WASAPI.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#ifndef WASAPI_CAPTURE_H_INCLUDED
#define WASAPI_CAPTURE_H_INCLUDED

#include "pch.h"
#include "DeviceState.h"
#include "WaveFileWriter.h"
#include "CBuffer.h"

#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <wrl\implements.h>

// Primary WASAPI Capture Class
class WASAPICapture :
	public Microsoft::WRL::RuntimeClass< Microsoft::WRL::RuntimeClassFlags< Microsoft::WRL::ClassicCom >, Microsoft::WRL::FtmBase, IActivateAudioInterfaceCompletionHandler >
{
public:
    WASAPICapture();
    ~WASAPICapture();

    HRESULT Activate(UINT id, bool bUseLoopback, CBuffer* capBuffer);

    HRESULT StartCaptureAsync();
    HRESULT StopCaptureAsync();
    HRESULT SetCaptureDevice(UINT id);

    void LoopbackToggle() { m_UseLoopback = !m_UseLoopback; };
            
    DeviceStateChangedEvent^ GetDeviceStateEvent() { return m_DeviceStateChanged; };
    WAVEFORMATEX* GetFormat() { return m_MixFormat; };
    void SetCaptureFilename(LPCWSTR inFile) { m_Filename = inFile; };

    // IActivateAudioInterfaceCompletionHandler
    STDMETHOD(ActivateCompleted)( IActivateAudioInterfaceAsyncOperation *operation );

private:
    HRESULT OnSampleReady();

    HRESULT OnStartCapture();
    HRESULT OnStopCapture();

    HRESULT ConfigureDeviceInternal();
    HRESULT OnAudioSampleRequested();

    static DWORD WINAPI AudioSampleThreadProc( LPVOID lpParameter );

    DWORD AudioSampleThread();

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
        CMD_START_CAPTURE,
        CMD_STOP_CAPTURE
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
	Microsoft::WRL::ComPtr<IAudioCaptureClient> m_AudioCaptureClient;
	Microsoft::WRL::ComPtr<IMMDevice>			m_CaptureDevice;
            
    UINT32				m_CaptureDeviceId;
    bool				m_UseLoopback;

    DeviceStateChangedEvent^       m_DeviceStateChanged;
    CWaveFileWriter*		m_pWaveFile;
    LPCWSTR					m_Filename;

    CBuffer*			m_captureBuffer;
};

#endif
