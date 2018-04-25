//--------------------------------------------------------------------------------------
// GamepadCppWinRT.h
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

    Sample();

    // Initialization and management
    void Initialize(::IUnknown* window);

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

    // Render objects.
    std::unique_ptr<DirectX::GraphicsMemory>            m_graphicsMemory;
    std::unique_ptr<DirectX::SpriteBatch>               m_spriteBatch;
    std::unique_ptr<DirectX::SpriteFont>                m_font;
    std::unique_ptr<DirectX::SpriteFont>                m_ctrlFont;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>    m_background;

    //Gamepad states
    winrt::Windows::Xbox::Input::IGamepadReading        m_reading;
    winrt::Windows::Xbox::Input::IGamepad               m_currentGamepad;

    void OnGamepadAdded(winrt::Windows::Foundation::IInspectable const & sender, winrt::Windows::Xbox::Input::GamepadAddedEventArgs const & args);
    void OnGamepadRemoved(winrt::Windows::Foundation::IInspectable const & sender, winrt::Windows::Xbox::Input::GamepadRemovedEventArgs const & args);

    bool                    m_currentGamepadNeedsRefresh;
    std::wstring            m_buttonString;
    double                  m_leftTrigger;
    double                  m_rightTrigger;
    double                  m_leftStickX;
    double                  m_leftStickY;
    double                  m_rightStickX;
    double                  m_rightStickY;

    // Device resources.
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;
};
