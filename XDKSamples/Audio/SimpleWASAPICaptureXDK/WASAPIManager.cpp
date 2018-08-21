//--------------------------------------------------------------------------------------
// WASAPIManager.cpp
//
// Wraps the underlying WASAPIRenderer in a simple WinRT class that can receive
// the DeviceStateChanged events.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "WASAPIManager.h"

using namespace Platform;
using namespace Windows::Media::Devices;
using namespace Windows::Foundation;

WASAPIManager::WASAPIManager() :
    m_RenderStateChangedEvent( nullptr ),
    m_CaptureStateChangedEvent( nullptr ),
    m_Renderer( nullptr ),
    m_CaptureIndex ( 0 ),
    m_bUseLoopback ( false )
{
    ZeroMemory( &m_CaptureWfx, sizeof( WAVEFORMATEX ) );
    ZeroMemory( &m_RenderWfx, sizeof( WAVEFORMATEX ) );
            
    //Create a 32k buffer
    m_captureBuffer = new CBuffer(32768);

	m_renderEventToken = MediaDevice::DefaultAudioRenderDeviceChanged += ref new TypedEventHandler<Platform::Object^, DefaultAudioRenderDeviceChangedEventArgs^>(this, &WASAPIManager::OnRenderDeviceChange);

	InitializeCriticalSectionEx(&m_CritSec, 0, 0);
}


WASAPIManager::~WASAPIManager()
{
    if (m_RenderDeviceStateChangeToken.Value != 0)
    {
        m_RenderStateChangedEvent->StateChangedEvent -= m_RenderDeviceStateChangeToken;

        m_RenderStateChangedEvent = nullptr;
        m_RenderDeviceStateChangeToken.Value = 0;
    }

    if (m_CaptureDeviceStateChangeToken.Value != 0)
    {
        m_CaptureStateChangedEvent->StateChangedEvent -= m_CaptureDeviceStateChangeToken;

        m_CaptureStateChangedEvent = nullptr;
        m_CaptureDeviceStateChangeToken.Value = 0;
    }

    ZeroMemory( &m_CaptureWfx, sizeof( WAVEFORMATEX ) );
    ZeroMemory( &m_RenderWfx, sizeof( WAVEFORMATEX ) );

    DeleteCriticalSection( &m_CritSec );
}

void WASAPIManager::OnRenderDeviceChange(Platform::Object^,
	Windows::Media::Devices::DefaultAudioRenderDeviceChangedEventArgs^)
{
	RestartDevice();
}

//--------------------------------------------------------------------------------------
//  Name: OnDeviceStateChange
//  Desc: Event callback from WASAPI components for changes in device state
//--------------------------------------------------------------------------------------
void WASAPIManager::OnDeviceStateChange( Object^ sender, DeviceStateChangedEventArgs^ e )
{
    // Handle state specific messages
    switch( e->State )
    {
    case DeviceState::DeviceStateInitialized:
        InitializeCaptureDevice();
        break;

    case DeviceState::DeviceStateInError:
        if (m_RenderDeviceStateChangeToken.Value != 0)
        {
            m_RenderStateChangedEvent->StateChangedEvent -= m_RenderDeviceStateChangeToken;
            m_RenderStateChangedEvent = nullptr;
            m_RenderDeviceStateChangeToken.Value = 0;
        }

        if (m_CaptureDeviceStateChangeToken.Value != 0)
        {
            m_CaptureStateChangedEvent->StateChangedEvent -= m_CaptureDeviceStateChangeToken;
            m_CaptureStateChangedEvent = nullptr;
            m_CaptureDeviceStateChangeToken.Value = 0;
        }

        m_Renderer = nullptr;
    }

    UpdateStatus();
}


//--------------------------------------------------------------------------------------
//  Name: InitializeRenderDevice
//  Desc: Sets up a new instance of the WASAPI renderer and creates WASAPI session
//--------------------------------------------------------------------------------------
void WASAPIManager::InitializeRenderDevice()
{
    if (!m_Renderer)
    {
        // Create a new WASAPI instance
        m_Renderer = Microsoft::WRL::Make<WASAPIRenderer>();

        if (nullptr == m_Renderer)
        {
            OnDeviceStateChange( this, ref new DeviceStateChangedEventArgs( DeviceState::DeviceStateInError, E_OUTOFMEMORY ));
            return;
        }

        // Get a pointer to the device event interface
        m_RenderStateChangedEvent = m_Renderer->GetDeviceStateEvent();

        if (nullptr == m_RenderStateChangedEvent)
        {
            OnDeviceStateChange( this, ref new DeviceStateChangedEventArgs( DeviceState::DeviceStateInError, E_FAIL ));
            return;
        }

        // Register for events
        m_RenderDeviceStateChangeToken = m_RenderStateChangedEvent->StateChangedEvent += ref new DeviceStateChangedHandler( this, &WASAPIManager::OnDeviceStateChange );

        // Selects the Default Audio Device
        m_Renderer->InitializeAudioDeviceAsync();
    }
}


//--------------------------------------------------------------------------------------
//  Name: InitializeCaptureDevice
//  Desc: Sets up a new instance of the WASAPI capture
//--------------------------------------------------------------------------------------
void WASAPIManager::InitializeCaptureDevice()
{
    if (!m_Renderer)
    {
        InitializeRenderDevice();
    }
            
    if (!m_Capture)
    {
        m_Capture = Microsoft::WRL::Make<WASAPICapture>();

        if (nullptr == m_Capture)
        {
            OnDeviceStateChange( this, ref new DeviceStateChangedEventArgs( DeviceState::DeviceStateInError, E_OUTOFMEMORY ));
            return;
        }

        // Get a pointer to the device event interface
        m_CaptureStateChangedEvent = m_Capture->GetDeviceStateEvent();

        if (nullptr == m_CaptureStateChangedEvent)
        {
            OnDeviceStateChange( this, ref new DeviceStateChangedEventArgs( DeviceState::DeviceStateInError, E_FAIL ));
            return;
        }

        // Register for events
        m_CaptureDeviceStateChangeToken = m_CaptureStateChangedEvent->StateChangedEvent += ref new DeviceStateChangedHandler( this, &WASAPIManager::OnDeviceStateChange );

        //Activate the capture device
		if (m_Capture->Activate(m_CaptureIndex, m_bUseLoopback, m_captureBuffer) != S_OK)
		{
			m_Capture = nullptr;
			return;
		}
                
        //Get a copy of the capture/render format
        CopyMemory(&m_CaptureWfx, m_Capture->GetFormat(), sizeof(WAVEFORMATEX));
        CopyMemory(&m_RenderWfx, m_Renderer->GetFormat(), sizeof(WAVEFORMATEX));

        //Pass the capture buffer to the renderer (for loopback)
        m_Renderer->SetCaptureBuffer(m_captureBuffer);

        //Provide the filename to capture
        m_Capture->SetCaptureFilename(g_FileName);

        //Give the capture buffer format information
        m_captureBuffer->SetRenderFormat(&m_RenderWfx);
        m_captureBuffer->SetSourceFormat(&m_CaptureWfx);

        //Register the device manager
		Microsoft::WRL::ComPtr<IMMDeviceEnumerator> deviceEnum;
        CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&deviceEnum);
        deviceEnum->RegisterEndpointNotificationCallback(&m_deviceManager);
    }

    UpdateStatus();
}


//--------------------------------------------------------------------------------------
//  Name:   RecordToggle
//  Desc:   Toggles the state of recording
//--------------------------------------------------------------------------------------
void WASAPIManager::RecordToggle()
{
    if (m_Capture)
    {
        DeviceState captureDeviceState = m_CaptureStateChangedEvent->GetState();
        DeviceState renderDeviceState = m_RenderStateChangedEvent->GetState();

        if (captureDeviceState == DeviceState::DeviceStateCapturing)
        {
            // Starts a work item to stop capture
            m_Capture->StopCaptureAsync();

            if (renderDeviceState == DeviceState::DeviceStatePlaying)
            {
                // Starts a work item to stop playback
                m_Renderer->StopPlaybackAsync();
            }
        }
        else if (captureDeviceState == DeviceState::DeviceStateStopped ||
            captureDeviceState == DeviceState::DeviceStateInitialized)
        {
            if (renderDeviceState == DeviceState::DeviceStatePlaying)
            {
                //Renderer is playing and needs to stop
                m_Renderer->StopPlaybackAsync();
            }

            // Starts a work item to start capture
            m_Capture->StartCaptureAsync();

            if(m_bUseLoopback)
            {
                if (renderDeviceState == DeviceState::DeviceStatePaused ||
                    renderDeviceState == DeviceState::DeviceStateStopped ||
                    renderDeviceState == DeviceState::DeviceStateInitialized)
                {
                    // Starts a work item to start playback for loopback
                    m_Renderer->StartPlaybackAsync(true);
                }
            }
        }
    }
}
        

//--------------------------------------------------------------------------------------
//  Name:   LoopbackToggle
//  Desc:   Toggles loopback (playback of the capture device)
//--------------------------------------------------------------------------------------
void WASAPIManager::LoopbackToggle()
{
    if(nullptr != m_Capture)
    {
        if(!m_bUseLoopback)
        {
            if(m_CaptureStateChangedEvent->GetState() == DeviceState::DeviceStateCapturing &&
                (m_RenderStateChangedEvent->GetState() == DeviceState::DeviceStatePaused ||
                m_RenderStateChangedEvent->GetState() == DeviceState::DeviceStateInitialized ||
                m_RenderStateChangedEvent->GetState() == DeviceState::DeviceStateStopped))
            {
                //Already capturing and want loopback, so start the renderer
                m_Renderer->StartPlaybackAsync(true);
            }
        }
        else
        {
            if(m_CaptureStateChangedEvent->GetState() == DeviceState::DeviceStateCapturing &&
                m_RenderStateChangedEvent->GetState() == DeviceState::DeviceStatePlaying)
            {
                //Already capturing and don't want loopback, so stop the renderer
                m_Renderer->StopPlaybackAsync();
            }
        }
                
        //Update the status in the capture and render components
        m_Capture->LoopbackToggle();
        m_bUseLoopback = !m_bUseLoopback;
        UpdateStatus();
    }
}
        

//--------------------------------------------------------------------------------------
//  Name:   SetCaptureDevice
//  Desc:   Switches the capture device
//--------------------------------------------------------------------------------------
void WASAPIManager::SetCaptureDevice(int index)
{
    if(m_Capture)
    {
        DeviceState captureDeviceState = m_CaptureStateChangedEvent->GetState();
        DeviceState renderDeviceState = m_RenderStateChangedEvent->GetState();

        if (captureDeviceState == DeviceState::DeviceStateCapturing)
        {
            // Starts a work item to stop capture
            m_Capture->StopCaptureAsync();

            if (renderDeviceState == DeviceState::DeviceStatePlaying)
            {
                // Starts a work item to stop playback
                m_Renderer->StopPlaybackAsync();
            }
        }

        m_deviceManager.SetCaptureId(index);
        m_CaptureIndex = index;
        m_Capture->SetCaptureDevice(index);
        m_Capture = nullptr;
        InitializeCaptureDevice();
    }
}


//--------------------------------------------------------------------------------------
//  Name: StartDevice
//  Desc: Initialize devices
//--------------------------------------------------------------------------------------
void WASAPIManager::StartDevice()
{
    if (nullptr == m_Renderer)
    {
        InitializeRenderDevice();
    }
}

//--------------------------------------------------------------------------------------
//  Name: RestartDevice
//  Desc: Restart playback
//--------------------------------------------------------------------------------------
void WASAPIManager::RestartDevice()
{
	m_Renderer = nullptr;
	InitializeRenderDevice();
}

//--------------------------------------------------------------------------------------
//  Name: GetStatus
//  Desc: Gets the status
//--------------------------------------------------------------------------------------
void WASAPIManager::GetStatus(ManagerStatus *inStatus)
{
    EnterCriticalSection( &m_CritSec );
            
    CopyMemory(inStatus, &m_Status, sizeof(ManagerStatus));

    LeaveCriticalSection( &m_CritSec );
}


//--------------------------------------------------------------------------------------
//  Name: UpdateStatus
//  Desc: Updates manager's status
//--------------------------------------------------------------------------------------
void WASAPIManager::UpdateStatus()
{
    EnterCriticalSection( &m_CritSec );
    if(m_Capture && m_Renderer)
    {
        DeviceState captureDeviceState = m_CaptureStateChangedEvent->GetState();
        DeviceState renderDeviceState = m_RenderStateChangedEvent->GetState();

        m_Status.bCapturing = (captureDeviceState == DeviceState::DeviceStateCapturing);
        m_Status.bLoopback = m_bUseLoopback;
        m_Status.bPlaying = (renderDeviceState == DeviceState::DeviceStatePlaying);
    }
    LeaveCriticalSection( &m_CritSec );
}


//--------------------------------------------------------------------------------------
//  Name: PlayPauseToggle
//  Desc: Toggle pause state
//--------------------------------------------------------------------------------------
void WASAPIManager::PlayPauseToggle()
{
    if (m_Renderer)
    {
        if(m_CaptureStateChangedEvent->GetState() == DeviceState::DeviceStateCapturing)
        {
            //Stop recording
            RecordToggle();
        }
        else if(m_RenderStateChangedEvent->GetState() == DeviceState::DeviceStatePlaying)
        {
            //Starts a work item to pause playback
            m_Renderer->PausePlaybackAsync();
        }
        else
        {
            //Tell the renderer to open the file and prepare the buffers 
            m_Renderer->ConfigureSource(g_FileName);

            //Starts a work item to start playback
            m_Renderer->StartPlaybackAsync(false);
        }
    }
}
