//--------------------------------------------------------------------------------------
// LampArrayUWP.h
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

    Sample() noexcept(false);

    // Initialization and management
    void Initialize(::IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation);

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

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

	void UpdateLighting();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>        m_deviceResources;
	std::unique_ptr<DirectX::SpriteBatch>       m_spriteBatch;
	std::unique_ptr<DirectX::SpriteFont>        m_font;

    // Rendering loop timer.
    DX::StepTimer							    m_timer;

    // Input devices.
    std::unique_ptr<DirectX::Keyboard>          m_keyboard;

    DirectX::Keyboard::KeyboardStateTracker		m_keyboardButtons;

	int 										m_effectIndex;
	LightingManager^							m_lightingManager;
	bool										m_keyDown;
};
