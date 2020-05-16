//--------------------------------------------------------------------------------------
// VideoTexturePC12.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "MediaEnginePlayer.h"

// A basic sample implementation that creates a D3D12 device and
// provides a render loop.
class Sample final : public DX::IDeviceNotify
{
public:

    Sample() noexcept(false);
    ~Sample();

    Sample(Sample&&) = default;
    Sample& operator= (Sample&&) = default;

    Sample(Sample const&) = delete;
    Sample& operator= (Sample const&) = delete;

    // Initialization and management
    void Initialize(HWND window, int width, int height);

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
    void OnWindowMoved();
    void OnWindowSizeChanged(int width, int height);

    // Properties
    void GetDefaultSize( int& width, int& height ) const;

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
    std::unique_ptr<DirectX::Keyboard>          m_keyboard;
    std::unique_ptr<DirectX::Mouse>             m_mouse;

    DirectX::GamePad::ButtonStateTracker        m_gamePadButtons;
    DirectX::Keyboard::KeyboardStateTracker     m_keyboardButtons;

    // DirectXTK objects.
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;

    // Rendering
    bool                                            m_show3D;

    std::unique_ptr<DirectX::SpriteBatch>           m_batchOpaque;
    std::unique_ptr<DirectX::SpriteBatch>           m_spriteBatch;
    std::unique_ptr<DirectX::DescriptorHeap>        m_resourceDescriptors;
    std::unique_ptr<DirectX::CommonStates>          m_states;
    std::unique_ptr<DirectX::GeometricPrimitive>    m_cube;
    std::unique_ptr<DirectX::BasicEffect>           m_effect;

    std::unique_ptr<DirectX::SpriteFont>            m_smallFont;
    std::unique_ptr<DirectX::SpriteFont>            m_ctrlFont;

    DirectX::SimpleMath::Matrix                     m_world;
    DirectX::SimpleMath::Matrix                     m_view;
    DirectX::SimpleMath::Matrix                     m_proj;

    // Video texture
    Microsoft::WRL::ComPtr<ID3D12Resource>      m_videoTexture;
    HANDLE                                      m_sharedVideoTexture;

    std::unique_ptr<MediaEnginePlayer>          m_player;

    uint32_t    m_videoWidth;
    uint32_t    m_videoHeight;

    enum Descriptors
    {
        VideoTexture,
        TextFont,
        ControllerFont,
        Count
    };
};
