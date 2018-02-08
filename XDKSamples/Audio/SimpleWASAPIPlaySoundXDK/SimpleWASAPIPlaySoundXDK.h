//--------------------------------------------------------------------------------------
// SimpleWASAPIPlaySoundXDK.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "WASAPIManager.h"

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

    // Device resources.
    std::unique_ptr<DX::DeviceResources>        m_deviceResources;

    // Rendering loop timer.
    uint64_t                                    m_frame;
    DX::StepTimer                               m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>           m_gamePad;

    DirectX::GamePad::ButtonStateTracker        m_gamePadButtons;

    // Render objects.
    std::unique_ptr<DirectX::SpriteBatch>		m_spriteBatch;
    std::unique_ptr<DirectX::SpriteFont>		m_font;
    std::unique_ptr<DirectX::SpriteFont>		m_ctrlFont;

	WASAPIManager^								m_wm;
	bool 										m_playPressed;

    // DirectXTK objects.
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;
};
