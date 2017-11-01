//--------------------------------------------------------------------------------------
// SimplePBRXbox.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "..\Shared\StepTimer.h"
#include "..\Shared\SharedSimplePBR.h"

// A basic sample implementation that creates a D3D12 device and
// provides a render loop.
class Sample
{
public:
    friend class SharedSimplePBR;

    Sample();

    // Initialization and management
    void Initialize(IUnknown* window);

    // Basic Sample loop
    void Tick();

    // Messages
    void OnSuspending();
    void OnResuming();

    inline static DXGI_FORMAT GetHDRRenderFormat() { return DXGI_FORMAT_R11G11B10_FLOAT; }
    inline static DXGI_FORMAT GetBackBufferFormat() { return DXGI_FORMAT_R10G10B10A2_UNORM; }
    inline static DXGI_FORMAT GetDepthFormat() { return DXGI_FORMAT_D32_FLOAT; }

    bool RequestHDRMode() const { return m_deviceResources ? (m_deviceResources->GetDeviceOptions() & DX::DeviceResources::c_EnableHDR) != 0 : false; }

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
        
    // DirectXTK objects.
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;

    // Core shared sample object
    std::unique_ptr<SharedSimplePBR>            m_sharedSimplePBR;
};
