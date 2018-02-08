//--------------------------------------------------------------------------------------
// SimpleWASAPIPlaySound.h
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
class Sample : public DX::IDeviceNotify
{
public:

    Sample();

    // Initialization and management
    void Initialize(IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation);

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
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>       m_gamePad;
    std::unique_ptr<DirectX::Keyboard>      m_keyboard;

	DirectX::GamePad::ButtonStateTracker    m_gamePadButtons;
	DirectX::Keyboard::KeyboardStateTracker m_keyboardButtons;
    bool                                    m_gamepadPresent;

    // Render objects.
    std::unique_ptr<DirectX::SpriteBatch>   m_spriteBatch;
    std::unique_ptr<DirectX::SpriteFont>    m_font;
    std::unique_ptr<DirectX::SpriteFont>    m_ctrlFont;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_background;

	WASAPIManager^						   m_wm;
};
