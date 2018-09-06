//--------------------------------------------------------------------------------------
// WASAPIManager.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "pch.h"
#include "DeviceState.h"
#include "WASAPIRenderer.h"
#include "WAVFileReader.h"



ref class WASAPIManager sealed
{
public:
    WASAPIManager();

    void StartDevice();
    void RestartDevice();
    void StopDevice();
    void PauseDevice();
    void PlayPauseToggle();

    void SetVolume( unsigned int volume );

    //--------------------------------------------------------------------------------------
    //  Name: IsStopped
    //  Desc: Returns true if the renderer doesn't exist or it does exist and it's in the
    //        DeviceStateStopped state.
    //--------------------------------------------------------------------------------------
    bool IsStopped()
    { 
        bool bStopped = true;

        if ( m_Renderer != nullptr )
        {
            bStopped = ( m_StateChangedEvent->GetState() == DeviceState::DeviceStateStopped );
        }
        return bStopped; 
    }

    bool IsPlaying()
    {
        bool bPlaying = false;

        if (m_Renderer != nullptr)
        {
            bPlaying = (m_StateChangedEvent->GetState() == DeviceState::DeviceStatePlaying);
        }
        return bPlaying;
    }

    void OnRenderDeviceChange(Platform::Object^,
        Windows::Media::Devices::DefaultAudioRenderDeviceChangedEventArgs^);

private:
    ~WASAPIManager();

    void OnDeviceStateChange( Object^ sender, DeviceStateChangedEventArgs^ e );

    void InitializeDevice();

    Windows::Foundation::EventRegistrationToken     m_DeviceStateChangeToken;

    DeviceStateChangedEvent^					m_StateChangedEvent;
    Microsoft::WRL::ComPtr<WASAPIRenderer>      m_Renderer;
    Windows::Foundation::EventRegistrationToken m_renderEventToken;
};
