//--------------------------------------------------------------------------------------
// SimpleMSAA.h
//
// This sample demonstrates setting up a MSAA render target for DirectX 12
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

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>            m_deviceResources;

    // MSAA resources.
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_msaaRenderTarget;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_msaaDepthStencil;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>    m_msaaRTVDescriptorHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>    m_msaaDSVDescriptorHeap;

    bool                                            m_msaa;

    // Rendering loop timer.
    uint64_t                                        m_frame;
    DX::StepTimer                                   m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>               m_gamePad;

    DirectX::GamePad::ButtonStateTracker            m_gamePadButtons;
        
    // DirectXTK objects.
    std::unique_ptr<DirectX::GraphicsMemory>        m_graphicsMemory;

    std::unique_ptr<DirectX::SpriteBatch>           m_batch;

    std::unique_ptr<DirectX::DescriptorHeap>        m_resourceDescriptors;

    enum Descriptors
    {
        UIFont,
        CtrlFont,
        Count
    };

    std::unique_ptr<DirectX::SpriteFont>            m_smallFont;
    std::unique_ptr<DirectX::SpriteFont>            m_ctrlFont;

    std::unique_ptr<DirectX::CommonStates>          m_states;
    std::unique_ptr<DirectX::Model>                 m_model;
    std::unique_ptr<DirectX::EffectTextureFactory>  m_modelResources;
    std::unique_ptr<DirectX::IEffectFactory>        m_fxFactory;

    DirectX::Model::EffectCollection                m_modelMSAA;
    DirectX::Model::EffectCollection                m_modelStandard;

    DirectX::SimpleMath::Matrix                     m_world;
    DirectX::SimpleMath::Matrix                     m_view;
    DirectX::SimpleMath::Matrix                     m_proj;
};
