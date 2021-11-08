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
class Sample final : public DX::IDeviceNotify
{
public:

    Sample() noexcept(false);
    ~Sample() = default;

    Sample(Sample&&) = default;
    Sample& operator= (Sample&&) = default;

    Sample(Sample const&) = delete;
    Sample& operator= (Sample const&) = delete;

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
    
    // Device resources.
    std::unique_ptr<DX::DeviceResources>            m_deviceResources;

    // MSAA resources.
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_msaaRenderTarget;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_msaaDepthStencil;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>    m_msaaRTVDescriptorHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>    m_msaaDSVDescriptorHeap;

    unsigned int                                    m_sampleCount;
    bool                                            m_msaa;

    // Rendering loop timer.
    DX::StepTimer                                   m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>               m_gamePad;
    std::unique_ptr<DirectX::Keyboard>              m_keyboard;

    DirectX::GamePad::ButtonStateTracker            m_gamePadButtons;
    DirectX::Keyboard::KeyboardStateTracker         m_keyboardButtons;
    bool                                            m_gamepadPresent;

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
