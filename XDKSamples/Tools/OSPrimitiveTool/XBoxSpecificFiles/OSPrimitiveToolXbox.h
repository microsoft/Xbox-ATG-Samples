//--------------------------------------------------------------------------------------
// OSPrimitiveToolXbox.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResourcesXbox.h"
#include "../StepTimer.h"
#include "../SharedOSPrimitiveTool.h"

// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
class Sample
{
    friend class SharedSample;

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
    void ParseCommandLine(Platform::String ^commandlineParams) { m_sharedSample->ParseCommandLine(commandlineParams->Data()); }

private:

    void Update(DX::StepTimer const& timer);
    void Render();
    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>        m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                               m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>           m_gamePad;

    DirectX::GamePad::ButtonStateTracker        m_gamePadButtons;

    // DirectXTK objects.
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_background;
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;
    std::unique_ptr<DirectX::SpriteBatch>       m_spriteBatch;
    std::unique_ptr<DirectX::SpriteFont>        m_font;

    std::unique_ptr<SharedSample>               m_sharedSample;
};
