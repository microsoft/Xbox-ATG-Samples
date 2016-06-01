//--------------------------------------------------------------------------------------
// WASAPICapture.cpp
//
// Functions to assist in capturing audio in WASAPI
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "WASAPICapture.h"

using namespace Windows::System::Threading;

//
//  WASAPICapture()
//
WASAPICapture::WASAPICapture() :
    m_bufferFrames( 0 ),
    m_queueID( 0 ),
    m_deviceStateChanged( nullptr ),
    m_audioClient( nullptr ),
    m_audioCaptureClient( nullptr ),
    m_sampleReadyAsyncResult( nullptr ),
    m_resampler( nullptr ),
    m_buffer( nullptr )
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

    // Register MMCSS work queue
    HRESULT hr = S_OK;
    DWORD taskID = 0;

    hr = MFLockSharedWorkQueue( L"Capture", 0, &taskID, &m_queueID );
    if (FAILED( hr ))
    {
        DX::ThrowIfFailed( hr );
    }

    // Set the capture event work queue to use the MMCSS queue
    m_xSampleReady.SetQueueID(m_queueID);
}

//
//  ~WASAPICapture()
//
WASAPICapture::~WASAPICapture()
{
    if (INVALID_HANDLE_VALUE != m_sampleReadyEvent)
    {
        CloseHandle( m_sampleReadyEvent );
        m_sampleReadyEvent = INVALID_HANDLE_VALUE;
    }

    MFUnlockWorkQueue( m_queueID );

    m_deviceStateChanged = nullptr;
}

//
//  InitializeAudioDeviceAsync()
//
//  Activates the default audio capture on a asynchronous callback thread.  This needs
//  to be called from the main UI thread.
//
HRESULT WASAPICapture::InitializeAudioDeviceAsync(Platform::String^ deviceIdString)
{
    IActivateAudioInterfaceAsyncOperation *asyncOp;
    HRESULT hr = S_OK;

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
HRESULT WASAPICapture::ActivateCompleted( IActivateAudioInterfaceAsyncOperation *operation )
{
    HRESULT hr = S_OK;
    HRESULT hrActivateResult = S_OK;
    Microsoft::WRL::ComPtr<IUnknown> punkAudioInterface = nullptr;

    // Check for a successful activation result
    hr = operation->GetActivateResult( &hrActivateResult, &punkAudioInterface );
    if (SUCCEEDED( hr ) && SUCCEEDED( hrActivateResult ))
    {
        // Get the pointer for the Audio Client
        punkAudioInterface.Get()->QueryInterface( IID_PPV_ARGS(&m_audioClient) );
        if (nullptr == m_audioClient)
        {
            hr = E_FAIL;
            goto exit;
        }

        hr = m_audioClient->GetMixFormat( &m_mixFormat );
        if (FAILED( hr ))
        {
            goto exit;
        }

        // Initialize the AudioClient in Shared Mode with the user specified buffer
        hr = m_audioClient->Initialize( AUDCLNT_SHAREMODE_SHARED,
                                        AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
                                        10000000LL,
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

        // Get the capture client
        hr = m_audioClient->GetService( __uuidof(IAudioCaptureClient), (void**) &m_audioCaptureClient );
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

        m_deviceStateChanged->SetState(DeviceState::DeviceStateInitialized, S_OK, true);
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
//  StartCaptureAsync()
//
//  Starts asynchronous capture on a separate thread via MF Work Item
//
HRESULT WASAPICapture::StartCaptureAsync(CBuffer* bufferToUse)
{
    m_buffer = bufferToUse;

    // We should be in the initialzied state if this is the first time through getting ready to capture.
    if (m_deviceStateChanged->GetState() == DeviceState::DeviceStateInitialized)
    {
        m_deviceStateChanged->SetState( DeviceState::DeviceStateStarting, S_OK, true );
        return MFPutWorkItem2( MFASYNC_CALLBACK_QUEUE_MULTITHREADED, 0, &m_xStartCapture, nullptr );
    }

    // We are in the wrong state
    return E_NOT_VALID_STATE;
}

//
//  OnStartCapture()
//
//  Callback method to start capture
//
HRESULT WASAPICapture::OnStartCapture( IMFAsyncResult* )
{
    HRESULT hr = S_OK;

    // Start the capture
    hr = m_audioClient->Start();
    if (SUCCEEDED( hr ))
    {
        m_deviceStateChanged->SetState( DeviceState::DeviceStateCapturing, S_OK, true );
        MFPutWaitingWorkItem( m_sampleReadyEvent, 0, m_sampleReadyAsyncResult.Get(), &m_sampleReadyKey );
    }
    else
    {
        m_deviceStateChanged->SetState( DeviceState::DeviceStateInError, hr, true );
    }

    return S_OK;
}

//
//  StopCaptureAsync()
//
//  Stop capture asynchronously via MF Work Item
//
HRESULT WASAPICapture::StopCaptureAsync()
{
    if ( (m_deviceStateChanged->GetState() != DeviceState::DeviceStateCapturing) &&
         (m_deviceStateChanged->GetState() != DeviceState::DeviceStateInError) )
    {
        return E_NOT_VALID_STATE;
    }

    m_deviceStateChanged->SetState( DeviceState::DeviceStateStopping, S_OK, true );

    return MFPutWorkItem2( MFASYNC_CALLBACK_QUEUE_MULTITHREADED, 0, &m_xStopCapture, nullptr );
}

//
//  OnStopCapture()
//
//  Callback method to stop capture
//
HRESULT WASAPICapture::OnStopCapture( IMFAsyncResult* )
{
    // Stop capture by cancelling Work Item
    // Cancel the queued work item (if any)
    if (0 != m_sampleReadyKey)
    {
        MFCancelWorkItem( m_sampleReadyKey );
        m_sampleReadyKey = 0;
    }

    m_audioClient->Stop();

    m_deviceStateChanged->SetState( DeviceState::DeviceStateFlushing, S_OK, true );

    m_buffer = nullptr;

    return S_OK;
}

//
//  OnSampleReady()
//
//  Callback method when ready to fill sample buffer
//
HRESULT WASAPICapture::OnSampleReady( IMFAsyncResult* )
{
    HRESULT hr = S_OK;

    hr = OnAudioSampleRequested( false );

    if (SUCCEEDED( hr ))
    {
        // Re-queue work item for next sample
        if (m_deviceStateChanged->GetState() ==  DeviceState::DeviceStateCapturing)
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
HRESULT WASAPICapture::OnAudioSampleRequested( bool isSilence )
{
    HRESULT hr = S_OK;
    uint32_t framesAvailable = 0;
    BYTE *data = nullptr;
    DWORD captureFlags;
    UINT64 devicePosition = 0;
    UINT64 QPCPosition = 0;
    DWORD bytesToCapture = 0;

    std::lock_guard<std::mutex> lck(m_mutex);

    // If this flag is set, we have already queued up the async call to finialize the WAV header
    // So we don't want to grab or write any more data that would possibly give us an invalid size
    if ( (m_deviceStateChanged->GetState() == DeviceState::DeviceStateStopping) ||
         (m_deviceStateChanged->GetState() == DeviceState::DeviceStateFlushing) )
    {
        goto exit;
    }

    // This should equal the buffer size when GetBuffer() is called
    hr = m_audioCaptureClient->GetNextPacketSize( &framesAvailable);
    if (FAILED( hr ))
    {
        goto exit;
    }

    if (framesAvailable > 0)
    {
        bytesToCapture = framesAvailable * m_mixFormat->nBlockAlign;
        
        // Get sample buffer
        hr = m_audioCaptureClient->GetBuffer( &data, &framesAvailable, &captureFlags, &devicePosition, &QPCPosition );
        if (FAILED( hr ))
        {
            goto exit;
        }

        if (captureFlags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)
        {
            // Pass down a discontinuity flag in case the app is interested and reset back to capturing
            m_deviceStateChanged->SetState( DeviceState::DeviceStateDiscontinuity, S_OK, true );
            m_deviceStateChanged->SetState( DeviceState::DeviceStateCapturing, S_OK, false );
        }

        // Zero out sample if silence
        if ( (captureFlags & AUDCLNT_BUFFERFLAGS_SILENT) || isSilence )
        {
            memset( data, 0, framesAvailable * m_mixFormat->nBlockAlign );
        }

        //Update circular buffer
        m_buffer->SetCaptureBuffer(bytesToCapture, data);

        // Release buffer back
        m_audioCaptureClient->ReleaseBuffer(framesAvailable);
    }

exit:
    return hr;
}
