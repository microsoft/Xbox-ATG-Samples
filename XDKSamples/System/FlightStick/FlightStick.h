//--------------------------------------------------------------------------------------
// FlightStick.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include <atomic>


// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
class Sample
{
public:

    Sample();

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

    // Render objects.
    std::unique_ptr<DirectX::GraphicsMemory>            m_graphicsMemory;
    std::unique_ptr<DirectX::SpriteBatch>               m_spriteBatch;
    std::unique_ptr<DirectX::SpriteFont>                m_font;
    std::unique_ptr<DirectX::SpriteFont>                m_ctrlFont;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>    m_background;

    //FlightStick states
    Microsoft::Xbox::Input::RawFlightStickReading             m_reading;
    Microsoft::Xbox::Input::IFlightStick^                     m_currentFlightStick;
    Microsoft::Xbox::Input::IFlightStickProperties^           m_currentFlightStickProperties;

    std::atomic<bool>       m_currentFlightStickNeedsRefresh;
    std::wstring            m_buttonString;
    short                   m_stickRoll;
    short                   m_stickPitch;
    short                   m_stickYaw;
    unsigned short          m_throttle;
    unsigned char           m_extraButtonCount;
    unsigned char           m_extraAxisCount;

    // Device resources.
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;
};
