//--------------------------------------------------------------------------------------
// UserGamepadPairing.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"


// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
class Sample
{
public:

    Sample() noexcept(false);
    ~Sample() = default;

    Sample(Sample&&) = default;
    Sample& operator= (Sample&&) = default;

    Sample(Sample const&) = delete;
    Sample& operator= (Sample const&) = delete;

    // Initialization and management
    void Initialize(IUnknown* window);

    // Basic game loop
    void Tick();

    // Messages
    void OnSuspending();
    void OnResuming();

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    void UpdateControllers();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>        m_deviceResources;

    // Rendering loop timer.
    uint64_t                                    m_frame;
    DX::StepTimer                               m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>           m_gamePad;

    DirectX::GamePad::ButtonStateTracker        m_gamePadButtons;

    // DirectXTK objects.
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;

    // Sample
    // Render objects.
    std::unique_ptr<DirectX::SpriteBatch>               m_spriteBatch;
    std::unique_ptr<DirectX::SpriteFont>                m_font;
    std::unique_ptr<DirectX::SpriteFont>                m_ctrlFont;

    //Gamepad states
    Windows::Xbox::Input::IGamepadReading^              m_reading;

    std::vector<std::wstring>                           m_buttonStrings;
    std::vector<std::wstring>                           m_userStrings;
    std::vector<double>                                 m_leftTrigger;
    std::vector<double>                                 m_rightTrigger;
    std::vector<double>                                 m_leftStickX;
    std::vector<double>                                 m_leftStickY;
    std::vector<double>                                 m_rightStickX;
    std::vector<double>                                 m_rightStickY;

    std::vector<concurrency::task<Platform::Object^>>   m_userTasks;

    Windows::Foundation::Collections::IVectorView< Windows::Xbox::Input::IGamepad^ >^ m_gamepadList;
    bool                                                m_needsRefresh;
    bool                                                m_needsStrings;
};
