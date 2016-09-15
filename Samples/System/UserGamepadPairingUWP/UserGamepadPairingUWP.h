//--------------------------------------------------------------------------------------
// UserGamepadPairingUWP.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"

// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
class Sample : public DX::IDeviceNotify
{
public:

    Sample();

    // Initialization and management
    void Initialize(IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation);

    // Basic render loop
    void Tick();
    void Render();

    // Rendering helpers
    void Clear();

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

private:

    void Update(DX::StepTimer const& timer);

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();
    
    // Render objects.
    std::unique_ptr<DirectX::SpriteBatch>               m_spriteBatch;
    std::unique_ptr<DirectX::SpriteFont>                m_font;
    std::unique_ptr<DirectX::SpriteFont>                m_ctrlFont;

    //Gamepad states
    Windows::Gaming::Input::GamepadReading              m_reading;

    std::vector<std::wstring>                           m_buttonStrings;
    std::vector<std::wstring>                           m_userStrings;
    std::vector<double>                                 m_leftTrigger;
    std::vector<double>                                 m_rightTrigger;
    std::vector<double>                                 m_leftStickX;
    std::vector<double>                                 m_leftStickY;
    std::vector<double>                                 m_rightStickX;
    std::vector<double>                                 m_rightStickY;

    std::vector<concurrency::task<Platform::Object^>>   m_userTasks;

    Windows::Foundation::Collections::IVectorView<Windows::Gaming::Input::Gamepad^>^ m_gamepadList;
    bool                                                m_needsRefresh;
    bool                                                m_needsStrings;

    // Device resources.
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;
};