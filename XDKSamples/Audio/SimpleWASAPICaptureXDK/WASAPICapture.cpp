//--------------------------------------------------------------------------------------
// WASAPICapture.cpp
//
// Class responsible for actually inputting samples to WASAPI.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "WASAPICapture.h"

using namespace Microsoft::WRL;
using namespace Windows::Media::Devices;

//
//  WASAPICapture()
//
WASAPICapture::WASAPICapture() :
    m_BufferFrames( 0 ),
    m_CaptureDeviceId ( 0 ),
    m_UseLoopback ( false ),
    m_SampleReadyEvent( INVALID_HANDLE_VALUE ),
    m_CmdReadyEvent( INVALID_HANDLE_VALUE ),
    m_MixFormat( nullptr ),
    m_CaptureDevice( nullptr ),
    m_AudioClient( nullptr ),
    m_AudioCaptureClient( nullptr ),
    m_DeviceStateChanged( nullptr )
{
    // Create events for sample ready or user stop
    m_SampleReadyEvent = CreateEventEx( nullptr, nullptr, 0, EVENT_ALL_ACCESS );
	if (m_SampleReadyEvent)
	{
		if (InitializeCriticalSectionEx(&m_CritSec, 0, 0))
		{
			// Event, queue and Slim RW Lock for sending commands to the sample dispatch thread
			m_CmdReadyEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
		
			if (m_CmdReadyEvent)
			{
				InitializeSRWLock(&m_CmdQueueLock);
				m_DeviceStateChanged = ref new DeviceStateChangedEvent();
			}
		}
	}
}


//
//  ~WASAPICapture()
//
WASAPICapture::~WASAPICapture()
{
    CoTaskMemFree( m_MixFormat );

    if ( INVALID_HANDLE_VALUE != m_CmdReadyEvent )
    {
        CloseHandle( m_CmdReadyEvent );
        m_CmdReadyEvent = INVALID_HANDLE_VALUE;
    }

    if (INVALID_HANDLE_VALUE != m_SampleReadyEvent)
    {
        CloseHandle( m_SampleReadyEvent );
        m_SampleReadyEvent = INVALID_HANDLE_VALUE;
    }

    DeleteCriticalSection( &m_CritSec );
}


//--------------------------------------------------------------------------------------
//  Name:   Activate
//  Desc:   Callback implementation of ActivateAudioInterfaceAsync function.  
//          This will be called on MTA thread when results of the activation are available.
//--------------------------------------------------------------------------------------
HRESULT WASAPICapture::Activate(UINT id, bool bUseLoopback, CBuffer* capBuffer)
{
    HRESULT hr = S_OK;

    m_CaptureDeviceId = id;

    if (m_DeviceStateChanged->GetState() != DeviceState::DeviceStateUnInitialized)
    {
        hr = E_NOT_VALID_STATE;
        SetDeviceStateError( hr );
        return hr;
    }

    m_DeviceStateChanged->SetState( DeviceState::DeviceStateActivated, S_OK, false );

    ComPtr<IMMDeviceEnumerator> pEnumerator = NULL;
    ComPtr<IMMDeviceCollection> deviceCollectionInterface;
    ComPtr<IMMDevice> pDevice = NULL;

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL,
        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
        (void**)&pEnumerator);
    if (FAILED( hr ))
    {
        SetDeviceStateError( hr );
		return hr;
	}

    hr = pEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATEMASK_ALL, &deviceCollectionInterface);
    if (FAILED( hr ))
    {
        SetDeviceStateError( hr );
		return hr;
	}

    //Set the capture device based on the index
    hr = deviceCollectionInterface->Item(m_CaptureDeviceId, &pDevice);
    if (FAILED( hr ))
    {
        SetDeviceStateError( hr );
		return hr;
	}

    hr = pDevice->Activate(
        __uuidof(IAudioClient2), CLSCTX_ALL,
            NULL, (void**)&m_AudioClient);
    if (FAILED( hr ))
    {
        SetDeviceStateError( hr );
		return hr;
	}

    //Get the default format of the device
    hr = m_AudioClient->GetMixFormat(&m_MixFormat);
    if (FAILED( hr ))
    {
        SetDeviceStateError( hr );
		return hr;
	}

    // Configure user defined properties
    hr = ConfigureDeviceInternal();
    if (FAILED( hr ))
    {
        SetDeviceStateError( hr );
		return hr;
	}
            
    // Initialize the AudioClient in Shared Mode with the user specified buffer
    hr = m_AudioClient->Initialize( AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
        REFTIMES_PER_SEC,
        0,
        m_MixFormat,
        &AUDIOSESSIONGUID);
    if (FAILED( hr ))
    {
        SetDeviceStateError( hr );
		return hr;
	}

    // Get the maximum size of the AudioClient Buffer
    hr = m_AudioClient->GetBufferSize( &m_BufferFrames );
    if (FAILED( hr ))
    {
        SetDeviceStateError( hr );
		return hr;
	}

    // Get the capture client
    hr = m_AudioClient->GetService( __uuidof(IAudioCaptureClient), (void**)m_AudioCaptureClient.GetAddressOf() );
    if (FAILED( hr ))
    {
        SetDeviceStateError( hr );
		return hr;
	}

    // Sets the event handle that the system signals when an audio buffer is ready to be processed by the client
    hr = m_AudioClient->SetEventHandle( m_SampleReadyEvent );
    if (FAILED( hr ))
    {
        SetDeviceStateError( hr );
		return hr;
	}

    HANDLE hAudioSampleThread = CreateThread( nullptr, 0, AudioSampleThreadProc, this, 0, nullptr );
    if ( NULL == hAudioSampleThread )
    {
        hr = HRESULT_FROM_WIN32( GetLastError() );
        if ( FAILED( hr ) )
            SetDeviceStateError( hr );
		return hr;
	}

    if ( !SetThreadPriority( hAudioSampleThread, THREAD_PRIORITY_TIME_CRITICAL ) )
    {
        hr = HRESULT_FROM_WIN32( GetLastError() );
        if ( FAILED( hr ) )
            SetDeviceStateError( hr );
		return hr;
	}

    // Close thread handle if started successfully so thread can be destroyed when
    // function exits.
    CloseHandle( hAudioSampleThread );

    // Everything succeeded
    m_DeviceStateChanged->SetState( DeviceState::DeviceStateInitialized, S_OK, true );
            
    m_UseLoopback = bUseLoopback;
            
    m_captureBuffer = capBuffer;

    // Need to return S_OK
    return S_OK;
}


//--------------------------------------------------------------------------------------
//  Name:   ActivateCompleted
//  Desc:   Callback implementation of ActivateAudioInterfaceAsync function.  
//          This will be called on MTA thread when results of the activation are available.
//--------------------------------------------------------------------------------------
HRESULT WASAPICapture::ActivateCompleted( IActivateAudioInterfaceAsyncOperation *operation )
{
    UNREFERENCED_PARAMETER(operation);
    return S_OK;
}


void  WASAPICapture::SetDeviceStateError( HRESULT hr )
{
    m_DeviceStateChanged->SetState( DeviceState::DeviceStateInError, hr, true );
    m_AudioClient = nullptr;
    m_AudioCaptureClient = nullptr;
}


//--------------------------------------------------------------------------------------
//  Name:   ConfigureDeviceInternal
//  Desc:   Sets additional playback parameters.
//--------------------------------------------------------------------------------------
HRESULT WASAPICapture::ConfigureDeviceInternal()
{
    if (m_DeviceStateChanged->GetState() != DeviceState::DeviceStateActivated)
    {
        return E_NOT_VALID_STATE;
    }

    HRESULT hr = S_OK;

    // Hardware offload isn't supported
    AudioClientProperties audioProps;
    audioProps.cbSize = sizeof( AudioClientProperties );
    audioProps.bIsOffload = false;
    audioProps.eCategory = AudioCategory_ForegroundOnlyMedia;

    hr = m_AudioClient->SetClientProperties( &audioProps );
    if (FAILED( hr ))
    {
        return hr;
    }

    // This sample opens the device is shared mode so we need to find the supported WAVEFORMATEX mix format
    hr = m_AudioClient->GetMixFormat( &m_MixFormat );
    if (FAILED( hr ))
    {
        return hr;
    }

    return hr;
}


//--------------------------------------------------------------------------------------
//  Name:   StartCaptureAsync
//  Desc:   Starts asynchronous capture on a separate thread
//--------------------------------------------------------------------------------------
HRESULT WASAPICapture::StartCaptureAsync()
{
    HRESULT hr = S_OK;

    // We should be stopped if the user stopped playback, or we should be
    // initialized if this is the first time through getting ready to playback.
    if ( (m_DeviceStateChanged->GetState() == DeviceState::DeviceStateStopped) ||
        (m_DeviceStateChanged->GetState() == DeviceState::DeviceStateInitialized) )
    {
        m_DeviceStateChanged->SetState( DeviceState::DeviceStateStarting, S_OK, true );

        EnqueueCommand( CMD_START_CAPTURE );
        return hr;
    }

    // Otherwise something else happened
    return E_FAIL;
}


//--------------------------------------------------------------------------------------
//  Name:   StopCaptureAsync
//  Desc:   Stop playback asynchronously
//--------------------------------------------------------------------------------------
HRESULT WASAPICapture::StopCaptureAsync()
{
    if ((m_DeviceStateChanged->GetState() != DeviceState::DeviceStateCapturing) &&
        (m_DeviceStateChanged->GetState() != DeviceState::DeviceStateInError) )
    {
        return E_NOT_VALID_STATE;
    }

    m_DeviceStateChanged->SetState( DeviceState::DeviceStateStopping, S_OK, true );
    EnqueueCommand( CMD_STOP_CAPTURE );
    return S_OK;
}


//--------------------------------------------------------------------------------------
//  Name:   OnStartCapture
//  Desc:   Method to start capture called from the high priority thread
//--------------------------------------------------------------------------------------
HRESULT WASAPICapture::OnStartCapture()
{
    HRESULT hr = S_OK;

    //Create the WAV file
    m_pWaveFile = new CWaveFileWriter();
    m_pWaveFile->Open(m_Filename, m_MixFormat);
            
    // Actually start recording
    hr = m_AudioClient->Start();
    if ( SUCCEEDED( hr ) )
    {
        m_DeviceStateChanged->SetState( DeviceState::DeviceStateCapturing, S_OK, true );
    }
    else
    {
        m_DeviceStateChanged->SetState( DeviceState::DeviceStateInError, hr, true );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
//  Name:   OnStopCapture
//  Desc:   Method to stop capture called from the high priority thread
//--------------------------------------------------------------------------------------
HRESULT WASAPICapture::OnStopCapture()
{
    if(m_DeviceStateChanged->GetState() != DeviceState::DeviceStateCapturing)
    {
        // Flush anything left in buffer
        OnAudioSampleRequested();
    }

    m_AudioClient->Stop();

    m_pWaveFile->Close();

    m_DeviceStateChanged->SetState( DeviceState::DeviceStateStopped, S_OK, true );
    return S_OK;
}


//--------------------------------------------------------------------------------------
//  Name:   SetCaptureDevice
//  Desc:   Sets the capture device
//--------------------------------------------------------------------------------------
HRESULT WASAPICapture::SetCaptureDevice(UINT id)
{
    HRESULT hr = S_OK;
    UINT iDeviceCount = 0;
    ComPtr<IMMDeviceEnumerator> pEnumerator = NULL;
    ComPtr<IMMDeviceCollection> deviceCollectionInterface;

    if(id == m_CaptureDeviceId)
    {
        return S_OK;
    }

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL,
        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
        (void**)&pEnumerator);

    //Check for valid ID
    if ( SUCCEEDED( hr ) )
    {
        hr = pEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATEMASK_ALL, &deviceCollectionInterface);
    }
            
    if ( SUCCEEDED( hr ) )
    {
        hr = deviceCollectionInterface->GetCount(&iDeviceCount);

        if ( FAILED( hr ) || iDeviceCount<=id)
        {
            return E_FAIL;
        }
    }

    //Deactivate running device
    m_CaptureDevice = nullptr;

    //Activate new device
    if ( SUCCEEDED( hr ) )
    {
        hr = Activate(id, m_UseLoopback, m_captureBuffer);
    }

    return hr;
}


//--------------------------------------------------------------------------------------
//  Name:   OnSampleReady
//  Desc:   Method called on high priority thread when ready to get a sample buffer
//--------------------------------------------------------------------------------------
HRESULT WASAPICapture::OnSampleReady()
{
    HRESULT hr = S_OK;

    hr = OnAudioSampleRequested();

    if (FAILED( hr ))
    {
        m_DeviceStateChanged->SetState( DeviceState::DeviceStateInError, hr, true );
    }

    return hr;
}


//--------------------------------------------------------------------------------------
//  Name:   OnAudioSampleRequested
//  Desc:   Called when audio device fires m_SampleReadyEvent
//--------------------------------------------------------------------------------------
HRESULT WASAPICapture::OnAudioSampleRequested()
{
    HRESULT hr = S_OK;
    BYTE *captureData;
    DWORD flags;
    UINT32 numFramesAvailable;
            
    // Prevent multiple concurrent submissions of samples
    EnterCriticalSection( &m_CritSec );
        
    hr = m_AudioCaptureClient->GetBuffer(&captureData, &numFramesAvailable, &flags, NULL, NULL);
    if(SUCCEEDED( hr ))
    {
        //Write to WAV file
        m_pWaveFile->WriteSample(captureData, numFramesAvailable * m_MixFormat->nBlockAlign * m_MixFormat->nChannels, NULL);
                
        if(m_UseLoopback)
        {
            //Copy data to CBuffer for the renderer
            m_captureBuffer->SetCaptureBuffer(numFramesAvailable * m_MixFormat->nBlockAlign, captureData);
        }
                
        hr = m_AudioCaptureClient->ReleaseBuffer(numFramesAvailable);
    }

    LeaveCriticalSection( &m_CritSec );

    return hr;
}


//--------------------------------------------------------------------------------------
//  Name:   AudioSampleThreadProc
//  Desc:   Shim for the AudioSampleThread method
//--------------------------------------------------------------------------------------
DWORD WINAPI WASAPICapture::AudioSampleThreadProc( LPVOID lpParameter )
{
    DWORD	result = S_OK;

    result = ((WASAPICapture*)lpParameter)->AudioSampleThread();
            
    return result;
}


//--------------------------------------------------------------------------------------
//  Name:   AudioSampleThread
//  Desc:   High priority thread that services samples and commands
//--------------------------------------------------------------------------------------
DWORD WASAPICapture::AudioSampleThread()
{
    DWORD result = S_OK;
    bool bQuit = false;

    HANDLE eventHandles[] = { m_SampleReadyEvent, m_CmdReadyEvent };

    while ( !bQuit )
    {
        DWORD reason = WaitForMultipleObjectsEx( _countof( eventHandles ), eventHandles, FALSE, INFINITE, TRUE );

        if ( WAIT_OBJECT_0 == reason )
        {
            //	Process a new set of samples
            OnSampleReady();
        }
        else if ( ( WAIT_OBJECT_0 + 1 ) == reason )
        {
            //	Commands are available in queue. Acquire a lock on the queue and
            //	copy commands into a local buffer so that we can process them
            //	without having to hold onto the lock.
            AcquireSRWLockShared( &m_CmdQueueLock );
            auto availCmds = m_CmdQueue.size();
            std::vector<UINT32> cmds( availCmds );
            for ( std::vector<UINT32>::size_type i = 0; i < availCmds; ++i )
            {
                cmds[ i ] = m_CmdQueue.front();
                m_CmdQueue.pop_front();
            }
            ReleaseSRWLockShared( &m_CmdQueueLock );

            //	Now process the commands
            for ( std::vector<UINT32>::size_type i = 0; i < availCmds; ++i )
            {
                switch ( cmds[ i ] )
                {
                case CMD_START_CAPTURE:
                    OnStartCapture();
                    break;
                case CMD_STOP_CAPTURE:
                    OnStopCapture();
                    break;
                }
            }
        }
        else if ( WAIT_FAILED == reason )
        {
            bQuit = true;
        }
    }
    return result;
}
