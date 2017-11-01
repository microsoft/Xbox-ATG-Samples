//--------------------------------------------------------------------------------------
// WASAPIRenderer.cpp
//
// Class responsible for actually outputting samples to WASAPI.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "WASAPIRenderer.h"

using namespace Microsoft::WRL;
using namespace Windows::Media::Devices;

//
//  WASAPIRenderer()
//
WASAPIRenderer::WASAPIRenderer() :
    m_BufferFrames( 0 ),
    m_SampleReadyEvent( INVALID_HANDLE_VALUE ),
    m_CmdReadyEvent( INVALID_HANDLE_VALUE ),
    m_MixFormat( nullptr ),
    m_AudioClient( nullptr ),
    m_AudioRenderClient( nullptr ),
    m_DeviceStateChanged( nullptr ),
    m_WaveSource( nullptr ),
    m_bLoopback ( false ),
    m_bPlayback ( false ),
    m_WaveSize( 0 )
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
				if (m_DeviceStateChanged)
				{
					m_MixFormat = new WAVEFORMATEX();
				}
			}
		}
	}
}


//
//  ~WASAPIRenderer()
//
WASAPIRenderer::~WASAPIRenderer()
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

    m_WaveSize = 0;

    DeleteCriticalSection( &m_CritSec );
}


//--------------------------------------------------------------------------------------
//  Name:   InitializeAudioDeviceAsync
//  Desc:   Activates the default audio renderer on a asynchronous callback thread.  
//          This needs to be called from the main UI thread.
//--------------------------------------------------------------------------------------
HRESULT WASAPIRenderer::InitializeAudioDeviceAsync()
{
    ComPtr<IActivateAudioInterfaceAsyncOperation>   asyncOp;
    HRESULT hr = S_OK;

    // Get a string representing the Default Audio Device Renderer
    m_DeviceIdString = MediaDevice::GetDefaultAudioRenderId( Windows::Media::Devices::AudioDeviceRole::Default );

    // This call must be made on the main UI thread.  Async operation will call back to 
    // IActivateAudioInterfaceCompletionHandler::ActivateCompleted, which must be an agile interface implementation
    hr = ActivateAudioInterfaceAsync( m_DeviceIdString->Data(), __uuidof(IAudioClient2), nullptr, this, &asyncOp );
    if (FAILED( hr ))
    {
        m_DeviceStateChanged->SetState( DeviceState::DeviceStateInError, hr, true );
    }

    //SAFE_RELEASE( asyncOp );

    return hr;
}


//--------------------------------------------------------------------------------------
//  Name:   ActivateCompleted
//  Desc:   Callback implementation of ActivateAudioInterfaceAsync function.  
//          This will be called on MTA thread when results of the activation are available.
//--------------------------------------------------------------------------------------
HRESULT WASAPIRenderer::ActivateCompleted( IActivateAudioInterfaceAsyncOperation *operation )
{
    HRESULT hr = S_OK;
    HRESULT hrActivateResult = S_OK;
    ComPtr<IUnknown>    punkAudioInterface;

    if (m_DeviceStateChanged->GetState() != DeviceState::DeviceStateUnInitialized)
    {
        hr = E_NOT_VALID_STATE;
        SetDeviceStateError( hr );

        // Need to return S_OK
        return S_OK;
    }

    // Check for a successful activation result
    hr = operation->GetActivateResult( &hrActivateResult, &punkAudioInterface );
    if (SUCCEEDED( hr ) && SUCCEEDED( hrActivateResult ))
    {
        m_DeviceStateChanged->SetState( DeviceState::DeviceStateActivated, S_OK, false );

        // Get the pointer for the Audio Client
        punkAudioInterface.As<IAudioClient2>( &m_AudioClient );
        if( nullptr == m_AudioClient )
        {
            hr = E_FAIL;
            SetDeviceStateError( hr );

            // Need to return S_OK
            return S_OK;
        }

        // Configure user defined properties
        hr = ConfigureDeviceInternal();
        if (FAILED( hr ))
        {
            SetDeviceStateError( hr );

            // Need to return S_OK
            return S_OK;
        }

        // For this sample we will force stereo output (default is 7.1)
        m_MixFormat->nChannels = 2;
        m_MixFormat->nBlockAlign = 8;
        m_MixFormat->nAvgBytesPerSec = 384000;

        // Initialize the AudioClient in Shared Mode with the user specified buffer
        hr = m_AudioClient->Initialize( AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
            (REFTIMES_PER_SEC / 1000) * 20,
            0,
            m_MixFormat,
            &AUDIOSESSIONGUID);
        if (FAILED( hr ))
        {
            SetDeviceStateError( hr );

            // Need to return S_OK
            return S_OK;
        }

        // Get the maximum size of the AudioClient Buffer
        hr = m_AudioClient->GetBufferSize( &m_BufferFrames );
        if (FAILED( hr ))
        {
            SetDeviceStateError( hr );

            // Need to return S_OK
            return S_OK;
        }

        // Get the render client
        hr = m_AudioClient->GetService( __uuidof(IAudioRenderClient), (void**)m_AudioRenderClient.GetAddressOf() );
        if (FAILED( hr ))
        {
            SetDeviceStateError( hr );

            // Need to return S_OK
            return S_OK;
        }

        // Sets the event handle that the system signals when an audio buffer is ready to be processed by the client
        hr = m_AudioClient->SetEventHandle( m_SampleReadyEvent );
        if (FAILED( hr ))
        {
            SetDeviceStateError( hr );

            // Need to return S_OK
            return S_OK;
        }

        HANDLE hAudioSampleThread = CreateThread( nullptr, 0, AudioSampleThreadProc, this, 0, nullptr );
        if ( NULL == hAudioSampleThread )
        {
            hr = HRESULT_FROM_WIN32( GetLastError() );
            if ( FAILED( hr ) )
                SetDeviceStateError( hr );

            // Need to return S_OK
            return S_OK;
        }

        if ( !SetThreadPriority( hAudioSampleThread, THREAD_PRIORITY_TIME_CRITICAL ) )
        {
            hr = HRESULT_FROM_WIN32( GetLastError() );
            if ( FAILED( hr ) )
                SetDeviceStateError( hr );

            // Need to return S_OK
            return S_OK;
        }

        // Close thread handle if started successfully so thread can be destroyed when
        // function exits.
        CloseHandle( hAudioSampleThread );

        // Everything succeeded
        m_DeviceStateChanged->SetState( DeviceState::DeviceStateInitialized, S_OK, true );
    }
            
    if ( FAILED( hr ) )
        SetDeviceStateError( hr );
            
    // Need to return S_OK
    return S_OK;
}


void  WASAPIRenderer::SetDeviceStateError( HRESULT hr )
{
    m_DeviceStateChanged->SetState( DeviceState::DeviceStateInError, hr, true );
    m_AudioClient = nullptr;
    m_AudioRenderClient = nullptr;
}



//--------------------------------------------------------------------------------------
//  Name:   GetBufferFramesPerPeriod
//  Desc:   Get the time in seconds between passes of the audio device
//--------------------------------------------------------------------------------------
UINT32 WASAPIRenderer::GetBufferFramesPerPeriod()
{
    REFERENCE_TIME defaultDevicePeriod = 0;
    REFERENCE_TIME minimumDevicePeriod = 0;

    // Get the audio device period
    HRESULT hr = m_AudioClient->GetDevicePeriod( &defaultDevicePeriod, &minimumDevicePeriod);
    if (FAILED( hr ))
    {
        return 0;
    }

    double devicePeriodInSeconds = defaultDevicePeriod / (double)REFTIMES_PER_SEC;

    return static_cast<UINT32>( m_MixFormat->nSamplesPerSec * devicePeriodInSeconds + 0.5 );
}


//--------------------------------------------------------------------------------------
//  Name:   ConfigureDeviceInternal
//  Desc:   Sets additional playback parameters.
//--------------------------------------------------------------------------------------
HRESULT WASAPIRenderer::ConfigureDeviceInternal()
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

    return hr;
}


//--------------------------------------------------------------------------------------
//  Name:   ConfigureSource
//  Desc:   Configures WAVE playback
//--------------------------------------------------------------------------------------
HRESULT WASAPIRenderer::ConfigureSource(const wchar_t* filename )
{
    HRESULT hr = S_OK;
    UINT32 FramesPerPeriod = GetBufferFramesPerPeriod();
    WAVEFORMATEXTENSIBLE wfx;

    LoadPCM(filename, &wfx);

    m_WaveSource = new WaveSampleGenerator();
    hr = m_WaveSource->GenerateSampleBuffer( &m_waveFile[0], m_WaveSize, &wfx, FramesPerPeriod, m_MixFormat );

    return hr;
}


//--------------------------------------------------------------------------------------
//  Name:   StartPlaybackAsync
//  Desc:   Starts asynchronous playback on a separate thread
//--------------------------------------------------------------------------------------
HRESULT WASAPIRenderer::StartPlaybackAsync(bool bLoopback)
{
    HRESULT hr = S_OK;
    m_bLoopback = bLoopback;

    // We should be stopped if the user stopped playback, or we should be
    // initialized if this is the first time through getting ready to playback.
    if ( (m_DeviceStateChanged->GetState() == DeviceState::DeviceStateStopped) ||
        (m_DeviceStateChanged->GetState() == DeviceState::DeviceStateInitialized) )
    {
        // Setup File Playback
        //hr = ConfigureSource();
        if (FAILED( hr ))
        {
            m_DeviceStateChanged->SetState( DeviceState::DeviceStateInError, hr, true );
            return hr;
        }

        m_DeviceStateChanged->SetState( DeviceState::DeviceStateStarting, S_OK, true );

        EnqueueCommand( CMD_START );
        return hr;
    }
    else if (m_DeviceStateChanged->GetState() == DeviceState::DeviceStatePaused)
    {
        EnqueueCommand( CMD_START );
        return hr;
    }

    // Otherwise something else happened
    return E_FAIL;
}


//--------------------------------------------------------------------------------------
//  Name:   OnStartPlayback
//  Desc:   Method to start playback called from the high priority thread
//--------------------------------------------------------------------------------------
HRESULT WASAPIRenderer::OnStartPlayback()
{
    HRESULT hr = S_OK;

    // Pre-Roll the buffer with silence
    hr = OnAudioSampleRequested( true );
    if ( SUCCEEDED( hr ) )
    {
        // Actually start the playback
        hr = m_AudioClient->Start();
        if ( SUCCEEDED( hr ) )
        {
            m_DeviceStateChanged->SetState( DeviceState::DeviceStatePlaying, S_OK, true );
        }

        if ( FAILED( hr ) )
        {
            m_DeviceStateChanged->SetState( DeviceState::DeviceStateInError, hr, true );
        }
    }
    return S_OK;
}


//--------------------------------------------------------------------------------------
//  Name:   StopPlaybackAsync
//  Desc:   Stop playback asynchronously
//--------------------------------------------------------------------------------------
HRESULT WASAPIRenderer::StopPlaybackAsync()
{
    if ( (m_DeviceStateChanged->GetState() != DeviceState::DeviceStatePlaying) &&
        (m_DeviceStateChanged->GetState() != DeviceState::DeviceStatePaused) &&
        (m_DeviceStateChanged->GetState() != DeviceState::DeviceStateInError) )
    {
        return E_NOT_VALID_STATE;
    }

    m_DeviceStateChanged->SetState( DeviceState::DeviceStateStopping, S_OK, true );
    EnqueueCommand( CMD_STOP );
    return S_OK;
}


//--------------------------------------------------------------------------------------
//  Name:   OnStopPlayback
//  Desc:   Method to stop playback called from the high priority thread
//--------------------------------------------------------------------------------------
HRESULT WASAPIRenderer::OnStopPlayback()
{
    // Flush anything left in buffer with silence
    OnAudioSampleRequested( true );

    m_AudioClient->Stop();

    // Flush remaining buffers
    if(!m_bLoopback)
    {
        m_WaveSource->Flush();
    }

    m_DeviceStateChanged->SetState( DeviceState::DeviceStateStopped, S_OK, true );
    return S_OK;
}


//--------------------------------------------------------------------------------------
//  Name:   PausePlaybackAsync
//  Desc:   Pause playback asynchronously
//--------------------------------------------------------------------------------------
HRESULT WASAPIRenderer::PausePlaybackAsync()
{
    if ( (m_DeviceStateChanged->GetState() !=  DeviceState::DeviceStatePlaying) &&
        (m_DeviceStateChanged->GetState() != DeviceState::DeviceStateInError) )
    {
        return E_NOT_VALID_STATE;
    }

    // Change state to stop automatic queuing of samples
    m_DeviceStateChanged->SetState( DeviceState::DeviceStatePausing, S_OK, false );
    EnqueueCommand( CMD_PAUSE );
    return S_OK;
}


//--------------------------------------------------------------------------------------
//  Name:   OnPausePlayback
//  Desc:   Method to pause playback called from the high priority thread
//--------------------------------------------------------------------------------------
HRESULT WASAPIRenderer::OnPausePlayback()
{
    m_AudioClient->Stop();
    m_DeviceStateChanged->SetState( DeviceState::DeviceStatePaused, S_OK, true );
    return S_OK;
}


//--------------------------------------------------------------------------------------
//  Name:   OnSampleReady
//  Desc:   Method called on high priority thread when ready to fill sample buffer
//--------------------------------------------------------------------------------------
HRESULT WASAPIRenderer::OnSampleReady()
{
    HRESULT hr = S_OK;

    hr = OnAudioSampleRequested( false );

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
HRESULT WASAPIRenderer::OnAudioSampleRequested( Platform::Boolean bIsSilence )
{
    HRESULT hr = S_OK;
    UINT32  paddingFrames = 0;
    UINT32  framesAvailable = 0;

    // Prevent multiple concurrent submissions of samples
    EnterCriticalSection( &m_CritSec );

    // Get padding in existing buffer
    hr = m_AudioClient->GetCurrentPadding( &paddingFrames );

    if ( SUCCEEDED( hr ) )
    {
        // In non-HW shared mode, GetCurrentPadding represents the number of queued frames
        // so we can subtract that from the overall number of frames we have
        framesAvailable = m_BufferFrames - paddingFrames;

        // Only continue if we have a buffer to write data
        if (framesAvailable > 0)
        {
            if ( bIsSilence )
            {
                hr = WriteSilence( framesAvailable );
            }
            else
            {
                if (m_DeviceStateChanged->GetState() == DeviceState::DeviceStatePlaying)
                {
                    if(m_bLoopback)
                    {
                        // Fill the buffer with capture samples
                        hr = GetCaptureSample( framesAvailable );
                    }
                    else
                    {
                        // Fill the buffer with a playback sample
                        hr = GetWaveSample( framesAvailable );
                    }
                }
            }
        }
    }

    LeaveCriticalSection( &m_CritSec );

    //	If anything says that the resources have been invalidated then reinitialize
    //	the device.
    if (AUDCLNT_E_RESOURCES_INVALIDATED == hr)
    {
        m_DeviceStateChanged->SetState( DeviceState::DeviceStateUnInitialized, hr, false );
        m_AudioClient = nullptr;
        m_AudioRenderClient = nullptr;

        hr = InitializeAudioDeviceAsync();
    }

    return hr;
}

        
//--------------------------------------------------------------------------------------
//  Name:   WriteSilence
//  Desc:   Fills buffer with silence
//--------------------------------------------------------------------------------------
HRESULT WASAPIRenderer::WriteSilence( UINT32 FramesAvailable )
{
    HRESULT hr = S_OK;
    BYTE *pData;

    // Fill the buffer with silence
    hr = m_AudioRenderClient->GetBuffer( FramesAvailable, &pData );

    if ( SUCCEEDED( hr ) )
    {
        hr = m_AudioRenderClient->ReleaseBuffer( FramesAvailable, AUDCLNT_BUFFERFLAGS_SILENT );
    }

    return hr;
}


//--------------------------------------------------------------------------------------
//  Name:   GetWaveSample
//  Desc:   Fills buffer with a wave sample
//--------------------------------------------------------------------------------------
HRESULT WASAPIRenderer::GetWaveSample( UINT32 FramesAvailable )
{
    HRESULT hr = S_OK;
    BYTE *Data;

    // Post-Roll Silence
    if (m_WaveSource->IsEOF())
    {
        hr = m_AudioRenderClient->GetBuffer( FramesAvailable, &Data );
        if (SUCCEEDED( hr ))
        {
            // Ignore the return
            hr = m_AudioRenderClient->ReleaseBuffer( FramesAvailable, AUDCLNT_BUFFERFLAGS_SILENT );
        }

        StopPlaybackAsync();
    }
    else if (m_WaveSource->GetBufferLength() <= ( FramesAvailable * m_MixFormat->nBlockAlign ))
    {
        UINT32 ActualFramesToRead = m_WaveSource->GetBufferLength() / m_MixFormat->nBlockAlign;
        UINT32 ActualBytesToRead = ActualFramesToRead * m_MixFormat->nBlockAlign;

        hr = m_AudioRenderClient->GetBuffer( ActualFramesToRead, &Data );
        if (SUCCEEDED( hr ))
        {
            hr = m_WaveSource->FillSampleBuffer( ActualBytesToRead, Data );
            if (SUCCEEDED( hr ))
            {
                hr = m_AudioRenderClient->ReleaseBuffer( ActualFramesToRead, 0 );
            }
        }
    }

    return hr;
}


//--------------------------------------------------------------------------------------
//  Name:   GetCaptureSample
//  Desc:   Fills buffer with a capture sample
//--------------------------------------------------------------------------------------
HRESULT WASAPIRenderer::GetCaptureSample( UINT32 FramesAvailable )
{
    HRESULT hr = S_OK;
    BYTE *pRenderData;
    UINT32 ActualFramesToRead = m_captureBuffer->GetCurrentUsage() / m_MixFormat->nBlockAlign;
    UINT32 ActualBytesToRead = ActualFramesToRead * m_MixFormat->nBlockAlign;

	if(FramesAvailable < ActualFramesToRead)
	{
		ActualFramesToRead = FramesAvailable;
		ActualBytesToRead = ActualFramesToRead * m_MixFormat->nBlockAlign;
	}

    if(ActualFramesToRead > 0)
    {
		hr = m_AudioRenderClient->GetBuffer( ActualFramesToRead, &pRenderData );

        if( SUCCEEDED( hr ))
        {
            m_captureBuffer->GetCaptureBuffer( ActualBytesToRead, pRenderData);
            hr = m_AudioRenderClient->ReleaseBuffer( ActualFramesToRead, 0 );
        }
    }
    else
    {
        //No capture data, just write silence
        WriteSilence(FramesAvailable);
    }

    return hr;
}


//--------------------------------------------------------------------------------------
//  Name: LoadPCM
//  Desc: Read a PCM WAV file into memory and track its key information ready to
//        be transferred to the WASAPIRenderer
//--------------------------------------------------------------------------------------
void WASAPIRenderer::LoadPCM(const wchar_t* filename, WAVEFORMATEXTENSIBLE* inWfx )
{
    m_WaveSize = 0;
    ZeroMemory( inWfx, sizeof( WAVEFORMATEXTENSIBLE) );

    // Read the wave file
	DX::WAVData waveData;
	DX::ThrowIfFailed(DX::LoadWAVAudioFromFileEx(filename, m_waveFile, waveData));

    // Read the format header
	memcpy(inWfx, waveData.wfx, sizeof(WAVEFORMATEXTENSIBLE));

    // Calculate how many bytes and samples are in the wave
    m_WaveSize = (DWORD)waveData.GetSampleDurationMS();
}


//--------------------------------------------------------------------------------------
//  Name:   AudioSampleThreadProc
//  Desc:   Shim for the AudioSampleThread method
//--------------------------------------------------------------------------------------
DWORD WINAPI WASAPIRenderer::AudioSampleThreadProc( LPVOID lpParameter )
{
    DWORD	result = S_OK;

    result = ((WASAPIRenderer*)lpParameter)->AudioSampleThread();
            
    return result;
}

//--------------------------------------------------------------------------------------
//  Name:   AudioSampleThread
//  Desc:   High priority thread that services samples and commands
//--------------------------------------------------------------------------------------
DWORD WASAPIRenderer::AudioSampleThread()
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
                case CMD_START:
                    OnStartPlayback();
                    break;

                case CMD_STOP:
                    OnStopPlayback();
                    break;

                case CMD_PAUSE:
                    OnPausePlayback();
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
