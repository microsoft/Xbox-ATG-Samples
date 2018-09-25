//--------------------------------------------------------------------------------------
// AsyncPresent.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "Timeline.h"

// A basic sample implementation that creates a D3D12 device and
// provides a render loop.
class Sample
{
public:

    Sample() noexcept(false);

    // Initialization and management
    void Initialize(IUnknown* window);

    // Basic Sample loop
    void Tick();

    // Messages
    void OnSuspending();
    void OnResuming();

    // Properties
    bool RequestHDRMode() const { return m_deviceResources ? (m_deviceResources->GetDeviceOptions() & DX::DeviceResources::c_EnableHDR) != 0 : false; }

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    HRESULT Present();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>                m_deviceResources;

    // Rendering loop timer.
    uint64_t                                            m_frame;
    DX::StepTimer                                       m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>                   m_gamePad;

    DirectX::GamePad::ButtonStateTracker                m_gamePadButtons;
        
    // Desriptors for m_resourceDescriptorHeap
    struct ResourceDescriptors
    {
        enum : uint32_t
        {
            FontController, 
            FontTimeline, 
            FontUI, 

            Count
        };
    };

    // DirectXTK objects.
    std::unique_ptr<DirectX::GraphicsMemory>            m_graphicsMemory;
    std::unique_ptr<DirectX::DescriptorHeap>            m_resourceDescriptorHeap;

    // Helper classes
    std::unique_ptr<DirectX::BasicEffect>               m_timelineEffect;
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>>  m_primitiveBatch;
    std::unique_ptr<DirectX::SpriteBatch>               m_spriteBatch;
    std::unique_ptr<DirectX::SpriteFont>                m_fontController;
    std::unique_ptr<DirectX::SpriteFont>                m_fontTimeline;
    std::unique_ptr<DirectX::SpriteFont>                m_fontUI;

    // Timelines
    std::unique_ptr<Timeline>                           m_renderThreadTimes;
    std::unique_ptr<Timeline>                           m_yieldTimes;
    std::unique_ptr<Timeline>                           m_swapThrottleTimes;

    // User controls
    bool                                                m_asyncPresent;
    uint32_t                                            m_syncInterval;

    // Vsync
    HANDLE                                              m_vsyncEvent;
};
