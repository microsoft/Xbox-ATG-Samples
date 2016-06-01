//--------------------------------------------------------------------------------------
// DeviceState.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

// NB: All states >= DeviceStateInitialized will allow some methods
// to be called successfully on the Audio Client
enum class DeviceState
{
    DeviceStateUnInitialized,
    DeviceStateInError,
    DeviceStateDiscontinuity,
    DeviceStateFlushing,
    DeviceStateActivated,
    DeviceStateInitialized,
    DeviceStateStarting,
    DeviceStatePlaying,
    DeviceStateCapturing,
    DeviceStatePausing,
    DeviceStatePaused,
    DeviceStateStopping,
    DeviceStateStopped
};

// Class for DeviceStateChanged events
ref class DeviceStateChangedEventArgs sealed
{
internal:
    DeviceStateChangedEventArgs(DeviceState newState, HRESULT hr) :
        m_DeviceState(newState),
        m_hr(hr)
    {};

    property DeviceState State
    {
        DeviceState get() { return m_DeviceState; }
    };

    property int hr
    {
        int get() { return m_hr; }
    }

private:
    DeviceState      m_DeviceState;
    HRESULT          m_hr;
};

// DeviceStateChanged delegate
delegate void DeviceStateChangedHandler(Platform::Object^ sender, DeviceStateChangedEventArgs^ e);

// DeviceStateChanged Event
ref class DeviceStateChangedEvent sealed
{
public:
    DeviceStateChangedEvent() :
        m_DeviceState(DeviceState::DeviceStateUnInitialized)
    {};


internal:
    DeviceState GetState() { return m_DeviceState; };

    void SetState(DeviceState newState, HRESULT hr, bool FireEvent) {
        if (m_DeviceState != newState)
        {
            m_DeviceState = newState;

            if (FireEvent)
            {
                DeviceStateChangedEventArgs^ e = ref new DeviceStateChangedEventArgs(m_DeviceState, hr);
                StateChangedEvent(this, e);
            }
        }
    };

public:
    static event DeviceStateChangedHandler^    StateChangedEvent;

private:
    DeviceState     m_DeviceState;
};
