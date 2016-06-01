//--------------------------------------------------------------------------------------
// WASAPIRenderer.cpp
//
// Functions to assist in rendering audio in WASAPI
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "WASAPIRenderer.h"

using namespace Windows::System::Threading;
using namespace Windows::Media::Devices;

//
//  WASAPIRenderer()
//
WASAPIRenderer::WASAPIRenderer() :
    m_bufferFrames( 0 ),
    m_deviceStateChanged( nullptr ),
    m_audioClient( nullptr ),
    m_audioRenderClient( nullptr ),
    m_sampleReadyAsyncResult( nullptr )
{
    // Create events for sample ready or user stop
    m_sampleReadyEvent = CreateEventEx( nullptr, nullptr, 0, EVENT_ALL_ACCESS );
    if (nullptr == m_sampleReadyEvent)
    {
        DX::ThrowIfFailed( HRESULT_FROM_WIN32( GetLastError() ) );
    }

    m_deviceStateChanged = ref new DeviceStateChangedEvent();
    if (nullptr == m_deviceStateChanged)
    {
        DX::ThrowIfFailed( E_OUTOFMEMORY );
    }
}

//
//  ~WASAPIRenderer()
//
WASAPIRenderer::~WASAPIRenderer()
{
    if (INVALID_HANDLE_VALUE != m_sampleReadyEvent)
    {
        CloseHandle( m_sampleReadyEvent );
        m_sampleReadyEvent = INVALID_HANDLE_VALUE;
    }

    m_deviceStateChanged = nullptr;
}

//
//  InitializeAudioDeviceAsync()
//
//  Activates the default audio renderer on a asynchronous callback thread.  This needs
//  to be called from the main UI thread.
//
HRESULT WASAPIRenderer::InitializeAudioDeviceAsync()
{
    IActivateAudioInterfaceAsyncOperation *asyncOp;
    HRESULT hr = S_OK;

    // Get a string representing the Default Audio Device Renderer
    Platform::String^ deviceIdString = MediaDevice::GetDefaultAudioRenderId( Windows::Media::Devices::AudioDeviceRole::Default );

	// This call must be made on the main UI thread.  Async operation will call back to 
    // IActivateAudioInterfaceCompletionHandler::ActivateCompleted, which must be an agile interface implementation
    hr = ActivateAudioInterfaceAsync(deviceIdString->Data(), __uuidof(IAudioClient2), nullptr, this, &asyncOp );
    if (FAILED( hr ))
    {
        m_deviceStateChanged->SetState( DeviceState::DeviceStateInError, hr, true );
    }

    SAFE_RELEASE( asyncOp );

    return hr;
}

//
//  ActivateCompleted()
//
//  Callback implementation of ActivateAudioInterfaceAsync function.  This will be called on MTA thread
//  when results of the activation are available.
//
HRESULT WASAPIRenderer::ActivateCompleted( IActivateAudioInterfaceAsyncOperation *operation )
{
    HRESULT hr = S_OK;
    HRESULT hrActivateResult = S_OK;
    Microsoft::WRL::ComPtr<IUnknown> punkAudioInterface = nullptr;

    if (m_deviceStateChanged->GetState() != DeviceState::DeviceStateUnInitialized)
    {
        hr = E_NOT_VALID_STATE;
        goto exit;
    }

    // Check for a successful activation result
    hr = operation->GetActivateResult( &hrActivateResult, &punkAudioInterface );
    if (SUCCEEDED( hr ) && SUCCEEDED( hrActivateResult ))
    {
        m_deviceStateChanged->SetState( DeviceState::DeviceStateActivated, S_OK, false );

        // Get the pointer for the Audio Client
        punkAudioInterface.Get()->QueryInterface( IID_PPV_ARGS(&m_audioClient) );
        if( nullptr == m_audioClient )
        {
            hr = E_FAIL;
            goto exit;
        }

        // Configure user defined properties
        hr = ConfigureDeviceInternal();
        if (FAILED( hr ))
        {
            goto exit;
        }

        // Initialize the AudioClient in Shared Mode with the user specified buffer
        hr = m_audioClient->Initialize( AUDCLNT_SHAREMODE_SHARED,
                                        AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
                                        200000,
                                        0,
                                        m_mixFormat,
                                        &AUDIOSESSIONGUID);

        if (FAILED( hr ))
        {
            goto exit;
        }

        // Get the maximum size of the AudioClient Buffer
        hr = m_audioClient->GetBufferSize( &m_bufferFrames );
        if (FAILED( hr ))
        {
            goto exit;
        }

        // Get the render client
        hr = m_audioClient->GetService( __uuidof(IAudioRenderClient), (void**) &m_audioRenderClient );
        if (FAILED( hr ))
        {
            goto exit;
        }

        // Create Async callback for sample events
        hr = MFCreateAsyncResult( nullptr, &m_xSampleReady, nullptr, &m_sampleReadyAsyncResult );
        if (FAILED( hr ))
        {
            goto exit;
        }

        // Sets the event handle that the system signals when an audio buffer is ready to be processed by the client
        hr = m_audioClient->SetEventHandle( m_sampleReadyEvent );
        if (FAILED( hr ))
        {
            goto exit;
        }

        // Everything succeeded
        m_deviceStateChanged->SetState( DeviceState::DeviceStateInitialized, S_OK, true );

    }

exit:

    if (FAILED( hr ))
    {
        m_deviceStateChanged->SetState( DeviceState::DeviceStateInError, hr, true );
    }
    
    // Need to return S_OK
    return S_OK;
}

//
//  ConfigureDeviceInternal()
//
//  Sets additional playback parameters and opts into hardware offload
//
HRESULT WASAPIRenderer::ConfigureDeviceInternal()
{
    if (m_deviceStateChanged->GetState() != DeviceState::DeviceStateActivated)
    {
        return E_NOT_VALID_STATE;
    }

    HRESULT hr = S_OK;

    AudioClientProperties audioProps = {0};
    audioProps.cbSize = sizeof( AudioClientProperties );
    audioProps.eCategory = AudioCategory_ForegroundOnlyMedia;
	
    hr = m_audioClient->SetClientProperties( &audioProps );
    if (FAILED( hr ))
    {
        return hr;
    }

    // This sample opens the device is shared mode so we need to find the supported WAVEFORMATEX mix format
    hr = m_audioClient->GetMixFormat( &m_mixFormat );

    return hr;
}

//
//  StartPlaybackAsync()
//
//  Starts asynchronous playback on a separate thread via MF Work Item
//
HRESULT WASAPIRenderer::StartPlaybackAsync(CBuffer* bufferToUse)
{
    m_buffer = bufferToUse;

    // We should be stopped if the user stopped playback, or we should be
    // initialzied if this is the first time through getting ready to playback.
    if ( (m_deviceStateChanged->GetState() == DeviceState::DeviceStateStopped) ||
         (m_deviceStateChanged->GetState() == DeviceState::DeviceStateInitialized) )
    {
        m_deviceStateChanged->SetState( DeviceState::DeviceStateStarting, S_OK, true );
        return MFPutWorkItem2( MFASYNC_CALLBACK_QUEUE_MULTITHREADED, 0, &m_xStartPlayback, nullptr );
    }
    else if (m_deviceStateChanged->GetState() == DeviceState::DeviceStatePaused)
    {
        return MFPutWorkItem2( MFASYNC_CALLBACK_QUEUE_MULTITHREADED, 0, &m_xStartPlayback, nullptr );
    }

    // Otherwise something else happened
    return E_FAIL;
}

//
//  OnStartPlayback()
//
//  Callback method to start playback
//
HRESULT WASAPIRenderer::OnStartPlayback( IMFAsyncResult* )
{
    HRESULT hr = S_OK;

    // Pre-Roll the buffer with silence
    hr = OnAudioSampleRequested( true );
    if (FAILED( hr ))
    {
        goto exit;
    }

    // Actually start the playback
    hr = m_audioClient->Start();
    if (SUCCEEDED( hr ))
    {
        m_deviceStateChanged->SetState( DeviceState::DeviceStatePlaying, S_OK, true );
        hr = MFPutWaitingWorkItem( m_sampleReadyEvent, 0, m_sampleReadyAsyncResult.Get(), &m_sampleReadyKey );
    }

exit:
    if (FAILED( hr ))
    {
        m_deviceStateChanged->SetState( DeviceState::DeviceStateInError, hr, true );
    }

    return S_OK;
}

//
//  StopPlaybackAsync()
//
//  Stop playback asynchronously via MF Work Item
//
HRESULT WASAPIRenderer::StopPlaybackAsync()
{
    if ( (m_deviceStateChanged->GetState() != DeviceState::DeviceStatePlaying) &&
         (m_deviceStateChanged->GetState() != DeviceState::DeviceStatePaused) &&
         (m_deviceStateChanged->GetState() != DeviceState::DeviceStateInError) )
    {
        return E_NOT_VALID_STATE;
    }

    m_deviceStateChanged->SetState( DeviceState::DeviceStateStopping, S_OK, true );

    return MFPutWorkItem2( MFASYNC_CALLBACK_QUEUE_MULTITHREADED, 0, &m_xStopPlayback, nullptr );
}

//
//  OnStopPlayback()
//
//  Callback method to stop playback
//
HRESULT WASAPIRenderer::OnStopPlayback( IMFAsyncResult* )
{
    // Stop playback by cancelling Work Item
    // Cancel the queued work item (if any)
    if (0 != m_sampleReadyKey)
    {
        MFCancelWorkItem( m_sampleReadyKey );
        m_sampleReadyKey = 0;
    }

    // Flush anything left in buffer with silence
    OnAudioSampleRequested( true );

    m_audioClient->Stop();

    m_deviceStateChanged->SetState( DeviceState::DeviceStateStopped, S_OK, true );
    return S_OK;
}

//
//  OnSampleReady()
//
//  Callback method when ready to fill sample buffer
//
HRESULT WASAPIRenderer::OnSampleReady( IMFAsyncResult* )
{
    HRESULT hr = S_OK;

    hr = OnAudioSampleRequested( false );

    if (SUCCEEDED( hr ))
    {
        // Re-queue work item for next sample
        if (m_deviceStateChanged->GetState() ==  DeviceState::DeviceStatePlaying)
        {
            hr = MFPutWaitingWorkItem( m_sampleReadyEvent, 0, m_sampleReadyAsyncResult.Get(), &m_sampleReadyKey );
        }
    }
    else
    {
        m_deviceStateChanged->SetState( DeviceState::DeviceStateInError, hr, true );
    }

    return hr;
}

//
//  OnAudioSampleRequested()
//
//  Called when audio device fires m_sampleReadyEvent
//
HRESULT WASAPIRenderer::OnAudioSampleRequested( bool isSilence )
{
    HRESULT hr = S_OK;
    uint32_t paddingFrames = 0;
    uint32_t framesAvailable = 0;

    std::lock_guard<std::mutex> lck(m_mutex);

    // Get padding in existing buffer
    hr = m_audioClient->GetCurrentPadding( &paddingFrames);
    if (FAILED( hr ))
    {
        goto exit;
    }

    framesAvailable = m_bufferFrames - paddingFrames;

    // Only continue if we have buffer to write data
    if (framesAvailable > 0)
    {
        if (isSilence)
        {
            BYTE *Data;

            // Fill the buffer with silence
            hr = m_audioRenderClient->GetBuffer( framesAvailable, &Data );
            if (FAILED( hr ))
            {
                goto exit;
            }

            hr = m_audioRenderClient->ReleaseBuffer( framesAvailable, AUDCLNT_BUFFERFLAGS_SILENT );
            goto exit;
        }

        // Even if we cancel a work item, this may still fire due to the async
        // nature of things.  There should be a queued work item already to handle
        // the process of stopping or stopped
        if (m_deviceStateChanged->GetState() == DeviceState::DeviceStatePlaying)
        {
            BYTE *pRenderData;
            uint32_t actualFramesToRead = m_buffer->GetCurrentUsage() / m_mixFormat->nBlockAlign;
            uint32_t actualBytesToRead = actualFramesToRead * m_mixFormat->nBlockAlign;

            if (framesAvailable < actualFramesToRead)
            {
                actualFramesToRead = framesAvailable;
                actualBytesToRead = actualFramesToRead * m_mixFormat->nBlockAlign;
            }

            if (actualFramesToRead > 0)
            {
                hr = m_audioRenderClient->GetBuffer(actualFramesToRead, &pRenderData);

                if (SUCCEEDED(hr))
                {
                    m_buffer->GetCaptureBuffer(actualBytesToRead, pRenderData);
                    hr = m_audioRenderClient->ReleaseBuffer(actualFramesToRead, 0);
                }
            }
            else
            {
                //No capture data, just write silence
                hr = m_audioRenderClient->GetBuffer(framesAvailable, &pRenderData);
                if (FAILED(hr))
                {
                    goto exit;
                }

                hr = m_audioRenderClient->ReleaseBuffer(framesAvailable, AUDCLNT_BUFFERFLAGS_SILENT);
            }
        }
    }

exit:
    if (AUDCLNT_E_RESOURCES_INVALIDATED == hr)
    {
        m_deviceStateChanged->SetState( DeviceState::DeviceStateUnInitialized, hr, false );
        hr = InitializeAudioDeviceAsync();
    }

    return hr;
}
