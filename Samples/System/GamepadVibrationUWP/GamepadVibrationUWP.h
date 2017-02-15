//--------------------------------------------------------------------------------------
// GamepadVibrationUWP.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include <Windows.Gaming.Input.h>
#include <collection.h>

// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
class Sample : public DX::IDeviceNotify
{
public:
    static const int TRIGGEREFFECTS_MAX = 5;

    enum class TRIGGEREFFECTS
    {
        TRIGGEREFFECTS_IMPULSETEST = 0,
        TRIGGEREFFECTS_FLATTIRE,
        TRIGGEREFFECTS_GUNWITHRECOIL,
        TRIGGEREFFECTS_HEARTBEAT,
        TRIGGEREFFECTS_FOOTSTEPS
    };

    Sample();

    // Initialization and management
    void Initialize(IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation);

    // Basic render loop
    void Tick();

    // IDeviceNotify
    virtual void OnDeviceLost() override;
    virtual void OnDeviceRestored() override;

    // Messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation);
    void ValidateDevice();

    // Properties
    void GetDefaultSize( int& width, int& height ) const;

    void InitializeCurrentGamepad();
    void ShutdownCurrentGamepad();
    void InitializeImpulseTriggerEffects();
private:

    static const int MAX_PLAYER_COUNT = 8;
    
    Windows::Gaming::Input::Gamepad^ GetLastGamepad();
    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();
    
    // Render objects.
    std::unique_ptr<DirectX::SpriteBatch>   m_spriteBatch;
    std::unique_ptr<DirectX::SpriteFont>    m_font;
    std::unique_ptr<DirectX::SpriteFont>    m_ctrlFont;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_background;

    //Gamepad states
    Windows::Gaming::Input::GamepadReading      m_reading;
    Windows::Gaming::Input::Gamepad^            m_currentGamepad;
    Windows::Gaming::Input::GamepadVibration    m_vibration;
    Platform::Collections::Vector<Windows::Gaming::Input::Gamepad^>^ m_localCollection;

    bool                m_currentGamepadNeedsRefresh;
    bool                m_connected;
    double              m_leftMotorSpeed;
    double              m_rightMotorSpeed;
    double              m_leftTriggerLevel;
    double              m_rightTriggerLevel;
    bool                m_dPadPressed;

    // Variables used to control the Impulse Trigger Effects
    TRIGGEREFFECTS m_selectedTriggerEffect; // Effect currently selected
    DWORD          m_triggerEffectCounter;  // General purpose counter for use by trigger effects

    size_t         m_leftTriggerArraySize;
    uint32_t*        m_pLeftTriggerDurations;
    float*         m_pLeftTriggerLevels;
    size_t         m_rightTriggerArraySize;
    uint32_t*        m_pRightTriggerDurations;
    float*         m_pRightTriggerLevels;
    uint32_t         m_leftTriggerIndex;
    uint32_t         m_rightTriggerIndex;

    uint64_t  m_frequency;
    uint64_t  m_counter;
    uint64_t  m_leftTriggerIndexUpdateTime;
    uint64_t  m_rightTriggerIndexUpdateTime;

    // Device resources.
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;
};