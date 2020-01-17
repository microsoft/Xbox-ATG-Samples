//--------------------------------------------------------------------------------------
// InputInterfacingUWP.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include <collection.h>

enum Modes
{
    NavigationDevice = 0,
    ArcadeStickDevice,
    RacingWheelDevice,
	FlightStickDevice
};

// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
class Sample final : public DX::IDeviceNotify
{
public:

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

private:
    void GenerateNavString();
    void GenerateStickString();
	void DrawFlightStick(DirectX::XMFLOAT2 startPosition);
	void DrawWheel(DirectX::XMFLOAT2 startPosition);

    void UpdateNavController();
    void UpdateArcadeStick();
    void UpdateWheel();
    void UpdateFlightStick();

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

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
    Windows::Gaming::Input::ForceFeedback::ConstantForceEffect^ m_effect;
	Windows::Gaming::Input::FlightStick^				m_currentFlightStick;
	Windows::Gaming::Input::FlightStickReading			m_flightStickReading;

    Platform::Collections::Vector<Windows::Gaming::Input::UINavigationController^>^ m_navCollection;
    Platform::Collections::Vector<Windows::Gaming::Input::ArcadeStick^>^ m_stickCollection;
    Platform::Collections::Vector<Windows::Gaming::Input::RacingWheel^>^ m_wheelCollection;
	Platform::Collections::Vector<Windows::Gaming::Input::FlightStick^>^ m_flightStickCollection;

    bool                                                m_effectLoaded;
    Modes                   m_currentMode;
    bool                    m_selectPressed;
    bool                    m_connected;
    std::wstring            m_buttonString;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;
};
