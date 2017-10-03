//--------------------------------------------------------------------------------------
// SystemInfo.h
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

    // DirectXTK objects.
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;

    // UI
    std::unique_ptr<DirectX::SpriteBatch>               m_batch;
    std::unique_ptr<DirectX::SpriteFont>                m_smallFont;
    std::unique_ptr<DirectX::SpriteFont>                m_largeFont;
    std::unique_ptr<DirectX::SpriteFont>                m_ctrlFont;
    float                                               m_scale;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>    m_background;

    enum class InfoPage
    {
        SYSTEMINFO = 0,
        GLOBALMEMORYSTATUS,
        ANALYTICSINFO,
        DIRECT3D,
        MAX,
    };

    int                                                 m_current;
};
