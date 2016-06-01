//--------------------------------------------------------------------------------------
// WASAPIManager.cpp
//
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "WASAPIManager.h"


using namespace Platform;

WASAPIManager::WASAPIManager() :
    m_StateChangedEvent( nullptr ),
    m_Renderer( nullptr )
{
}

WASAPIManager::~WASAPIManager()
{
    if (m_DeviceStateChangeToken.Value != 0)
    {
        m_StateChangedEvent->StateChangedEvent -= m_DeviceStateChangeToken;

        m_StateChangedEvent = nullptr;
        m_DeviceStateChangeToken.Value = 0;
    }
}

//--------------------------------------------------------------------------------------
//  Name: OnDeviceStateChange
//  Desc: Event callback from WASAPI renderer for changes in device state
//--------------------------------------------------------------------------------------
void WASAPIManager::OnDeviceStateChange( Object^ sender, DeviceStateChangedEventArgs^ e )
{
    // Handle state specific messages
    switch( e->State )
    {
    case DeviceState::DeviceStateInitialized:
        StartDevice();
        break;

    case DeviceState::DeviceStatePlaying:
		OutputDebugString( L"Playback Started\n" );
        break;

    case DeviceState::DeviceStatePaused:
		OutputDebugString( L"Playback Paused\n" );
        break;

    case DeviceState::DeviceStateStopped:
        m_Renderer = nullptr;

        if (m_DeviceStateChangeToken.Value != 0)
        {
            m_StateChangedEvent->StateChangedEvent -= m_DeviceStateChangeToken;
            m_StateChangedEvent = nullptr;
            m_DeviceStateChangeToken.Value = 0;
        }

		OutputDebugString( L"Playback Stopped\n" );
        break;

    case DeviceState::DeviceStateInError:
        HRESULT hr = e->hr;

        if (m_DeviceStateChangeToken.Value != 0)
        {
            m_StateChangedEvent->StateChangedEvent -= m_DeviceStateChangeToken;
            m_StateChangedEvent = nullptr;
            m_DeviceStateChangeToken.Value = 0;
        }

        m_Renderer = nullptr;

        wchar_t hrVal[11];
        swprintf_s( hrVal, _countof( hrVal ), L"0x%08x", hr );
        String^ strHRVal = ref new String( hrVal );

        String^ strMessage = "";

        // Specifically handle a couple of known errors
        switch( hr )
        {
        case AUDCLNT_E_ENDPOINT_OFFLOAD_NOT_CAPABLE:
            strMessage = "ERROR: Endpoint Does Not Support HW Offload (" + strHRVal + ")\n";
			OutputDebugString(strMessage->Data());
            break;

        case AUDCLNT_E_RESOURCES_INVALIDATED:
            strMessage = "ERROR: Endpoint Lost Access To Resources (" + strHRVal + ")\n";
			OutputDebugString( strMessage->Data() );
            break;

        default:
            strMessage = "ERROR: " + strHRVal + " has occurred.\n";
			OutputDebugString( strMessage->Data() );
        }
    }
}

//--------------------------------------------------------------------------------------
//  Name: OnSetVolume
//  Desc: Updates the session volume
//--------------------------------------------------------------------------------------
void WASAPIManager::SetVolume( unsigned int volume )
{
    if (m_Renderer)
    {
        DeviceState deviceState = m_StateChangedEvent->GetState();

        if ( deviceState == DeviceState::DeviceStatePlaying || deviceState == DeviceState::DeviceStatePaused )
        {
            // Updates the Session volume on the AudioClient
            m_Renderer->SetVolumeOnSession( volume );
        }
    }
}

//--------------------------------------------------------------------------------------
//  Name: InitializeDevice
//  Desc: Sets up a new instance of the WASAPI renderer
//--------------------------------------------------------------------------------------
void WASAPIManager::InitializeDevice()
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
        m_StateChangedEvent = m_Renderer->GetDeviceStateEvent();

        if (nullptr == m_StateChangedEvent)
        {
            OnDeviceStateChange( this, ref new DeviceStateChangedEventArgs( DeviceState::DeviceStateInError, E_FAIL ));
            return;
        }

        // Register for events
        m_DeviceStateChangeToken = m_StateChangedEvent->StateChangedEvent += ref new DeviceStateChangedHandler( this, &WASAPIManager::OnDeviceStateChange );

        // Configure user based properties
        DEVICEPROPS props = { 0 };
        int BufferSize = 0;
        props.IsBackground = false;
        props.hnsBufferDuration = static_cast<REFERENCE_TIME>(BufferSize);
		props.Frequency = 440;

        m_Renderer->SetProperties( props );

        // Selects the Default Audio Device
        m_Renderer->InitializeAudioDeviceAsync();
    }
}

//--------------------------------------------------------------------------------------
//  Name: StartDevice
//  Desc: Initialize and start playback
//--------------------------------------------------------------------------------------
void WASAPIManager::StartDevice()
{
    if (nullptr == m_Renderer)
    {
        // Call from main UI thread
        InitializeDevice();
    }
    else
    {
        // Starts a work item to begin playback, likely in the paused state
        m_Renderer->StartPlaybackAsync();
    }
}

//--------------------------------------------------------------------------------------
//  Name:   StopDevice
//  Desc:   Stop playback, if WASAPI renderer exists
//--------------------------------------------------------------------------------------
void WASAPIManager::StopDevice()
{
    if (m_Renderer)
    {
        // Set the event to stop playback
        m_Renderer->StopPlaybackAsync();
    }
}

//--------------------------------------------------------------------------------------
//  Name: PauseDevice
//  Desc: If device is playing, pause playback. Otherwise do nothing.
//--------------------------------------------------------------------------------------
void WASAPIManager::PauseDevice()
{
    if (m_Renderer)
    {
        DeviceState deviceState = m_StateChangedEvent->GetState();

        if (deviceState == DeviceState::DeviceStatePlaying)
        {
            // Starts a work item to pause playback
            m_Renderer->PausePlaybackAsync();
        }
    }
}

//--------------------------------------------------------------------------------------
//  Name: PlayPauseToggle
//  Desc: Toggle pause state
//--------------------------------------------------------------------------------------
void WASAPIManager::PlayPauseToggle()
{
    if (m_Renderer)
    {
        DeviceState deviceState = m_StateChangedEvent->GetState();

        //  We only permit a pause state change if we're fully playing
        //  or fully paused.
        if (deviceState == DeviceState::DeviceStatePlaying)
        {
            // Starts a work item to pause playback
            m_Renderer->PausePlaybackAsync();
        }
        else if (deviceState == DeviceState::DeviceStatePaused)
        {
            // Starts a work item to pause playback
            m_Renderer->StartPlaybackAsync();
        }
    }
    else
    {
        StartDevice();
    }
}

