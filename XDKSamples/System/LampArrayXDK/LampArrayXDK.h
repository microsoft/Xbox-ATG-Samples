//--------------------------------------------------------------------------------------
// LampArrayXDK.h
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

    // Properties
    bool RequestHDRMode() const noexcept { return m_deviceResources ? (m_deviceResources->GetDeviceOptions() & DX::DeviceResources::c_EnableHDR) != 0 : false; }

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
    uint64_t                                    m_frame;
    DX::StepTimer                               m_timer;

    // Input devices.
	std::unique_ptr<DirectX::Keyboard>          m_keyboard;
	DirectX::Keyboard::KeyboardStateTracker     m_keyboardButtons;

    // DirectXTK objects.
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;

	int 										m_effectIndex;
	LightingManager^							m_lightingManager;
	bool										m_keyDown;
};
