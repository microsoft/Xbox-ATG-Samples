//--------------------------------------------------------------------------------------
// HLSLSymbols.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"


// A basic sample implementation that creates a D3D12 device and
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

    // Basic Sample loop
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

    // Device resources.
    std::unique_ptr<DX::DeviceResources>                m_deviceResources;

    // Rendering loop timer.
    uint64_t                                            m_frame;
    DX::StepTimer                                       m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>                   m_gamePad;

    DirectX::GamePad::ButtonStateTracker                m_gamePadButtons;
        
    // DirectXTK objects.
    std::unique_ptr<DirectX::GraphicsMemory>            m_graphicsMemory;
    std::unique_ptr<DirectX::DescriptorHeap>            m_resourceDescriptorHeap;

    // Direct3D 12 objects
    Microsoft::WRL::ComPtr<ID3D12RootSignature>         m_rootSignature;

    // Pipeline states for each version of the pixel shader.
    // These differ only in compiler arguments pertaining to symbols.
    Microsoft::WRL::ComPtr<ID3D12PipelineState>         m_pipelineStateEmbeddedPdb;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>         m_pipelineStateManualPdb;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>         m_pipelineStateAutoPdb;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>         m_pipelineStateAutoPdbNoPath;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>         m_pipelineStateStrippedPdb;

    // Draw helper
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>>  m_primitiveBatch;

    // Fonts
    struct ResourceDescriptors
    {
        enum : uint32_t
        {
            FontDescription, 
            Count
        };
    };

    std::unique_ptr<DirectX::SpriteBatch>               m_spriteBatch;
    std::unique_ptr<DirectX::SpriteFont>                m_fontDescription;
};
