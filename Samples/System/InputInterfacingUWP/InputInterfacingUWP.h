//--------------------------------------------------------------------------------------
// InputInterfacingUWP.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"


enum Modes
{
    NavigationDevice = 0,
    ArcadeStickDevice,
    RacingWheelDevice
};

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
    void GenerateNavString();
    void GenerateStickString();
    void DrawWheel(DirectX::XMFLOAT2 startPosition);

    void Update(DX::StepTimer const& timer);

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();
    
    // Device resources.
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Render objects.
    std::unique_ptr<DirectX::SpriteBatch>   m_spriteBatch;
    std::unique_ptr<DirectX::SpriteFont>    m_font;

    //Input
    Windows::Gaming::Input::UINavigationController^     m_currentNav;
    Windows::Gaming::Input::UINavigationReading         m_navReading;
    Windows::Gaming::Input::ArcadeStick^                m_currentStick;
    Windows::Gaming::Input::ArcadeStickReading          m_arcadeReading;
    Windows::Gaming::Input::RacingWheel^                m_currentWheel;
    Windows::Gaming::Input::RacingWheelReading          m_wheelReading;
    bool                                                m_currentNavNeedsRefresh;
    bool                                                m_currentWheelNeedsRefresh;
    bool                                                m_currentStickNeedsRefresh;
    
    Modes                   m_currentMode;
    bool                    m_selectPressed;
    bool                    m_connected;
    std::wstring            m_buttonString;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;
};