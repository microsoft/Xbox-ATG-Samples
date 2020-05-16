//--------------------------------------------------------------------------------------
// MouseCursor.h
//
// MouseCursor UWP sample
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"


// A basic sample implementation that creates a D3D11 device and
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

    enum MouseMode {ABSOLUTE_MOUSE, RELATIVE_MOUSE, CLIPCURSOR_MOUSE};

    // Initialization and management
    void Initialize( IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation );

    // Basic render loop
    void Tick();

    //IDeviceNotify
    virtual void OnDeviceLost() override;
    virtual void OnDeviceRestored() override;

    // Mouse cursor camera and target updates
    void UpdatePointer( Windows::Foundation::Point screen );
    void UpdateCamera( DirectX::SimpleMath::Vector3 movement );
    void MoveForward( float amount );
    void MoveRight( float amount );

    // Sample logic
    MouseMode SetMode( Windows::Foundation::Point mouseLocation );
    void CheckLocation( Windows::Foundation::Point mouseLocation );

    // Messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnWindowSizeChanged( int width, int height, DXGI_MODE_ROTATION rotation );
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
    std::unique_ptr<DX::DeviceResources>	m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;

    // MouseCursor sample
    void SetView();

    bool m_isAbsolute;
    bool m_isRelative;
    bool m_isClipCursor;
    bool m_highlightFPS;
    bool m_highlightRTS;

    // FPS
    DirectX::SimpleMath::Vector3 m_eyeFPS;
    DirectX::SimpleMath::Vector3 m_targetFPS;
    // RTS
    DirectX::SimpleMath::Vector3 m_eyeRTS;
    DirectX::SimpleMath::Vector3 m_targetRTS;

    DirectX::SimpleMath::Vector3 m_eye;
    DirectX::SimpleMath::Vector3 m_target;

    float m_pitch, m_yaw;

    DirectX::SimpleMath::Matrix m_world;
    DirectX::SimpleMath::Matrix m_view;
    DirectX::SimpleMath::Matrix m_proj;

    Windows::Foundation::Point m_absoluteLocation;
    Windows::Foundation::Point m_screenLocation;

    // DirectXTK rendering
    std::unique_ptr<DirectX::SpriteFont> m_font;
    std::unique_ptr<DirectX::SpriteFont> m_font64;
    std::unique_ptr<DirectX::SpriteFont> m_font32;
    std::unique_ptr<DirectX::SpriteFont> m_font28;
    DirectX::SimpleMath::Vector2 m_fontPos;
    DirectX::SimpleMath::Vector2 m_fontPosTitle;
    DirectX::SimpleMath::Vector2 m_fontPosSubtitle;
    DirectX::SimpleMath::Vector2 m_fontPosFPS;
    DirectX::SimpleMath::Vector2 m_fontPosRTS;
    std::unique_ptr<DirectX::SpriteBatch> m_spriteBatch;
    std::unique_ptr<DirectX::CommonStates> m_states;
    std::unique_ptr<DirectX::EffectFactory> m_fxFactory;
    std::unique_ptr<DirectX::Model> m_modelFPS;
    std::unique_ptr<DirectX::Model> m_modelRTS;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_texture_background;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_texture_tile;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_texture_tile_border;
    RECT m_fullscreenRect;
    RECT m_FPStile;
    RECT m_RTStile;
};
