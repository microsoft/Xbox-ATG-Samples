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
class Sample final : public DX::IDeviceNotify
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
        GETPROCESSINFO,
        GLOBALMEMORYSTATUS,
        ANALYTICSINFO,
        EASCLIENTINFO,
        GAMINGDEVICEINFO,
        APICONTRACT_PAGE,
        CPUSETS,
        DISPLAYINFO,
        DXGI,
        DIRECT3D11_1,
        DIRECT3D11_2,
        DIRECT3D11_3,
        DIRECT3D11_4,
        DIRECT3D12,
        DIRECT3D12_OPT1,
        DIRECT3D12_OPT2,
        DIRECT3D12_OPT3,
        MAX,
    };

    int                                                 m_current;
    bool                                                m_gamepadPresent;
};
