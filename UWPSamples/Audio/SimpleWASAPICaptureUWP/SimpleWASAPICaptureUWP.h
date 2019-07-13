//--------------------------------------------------------------------------------------
// SimpleWASAPICaptureUWP.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "WASAPICapture.h"
#include "WASAPIRenderer.h"
#include "CBuffer.h"

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
    
    // Render objects
    std::unique_ptr<DirectX::SpriteBatch>   m_spriteBatch;
    std::unique_ptr<DirectX::SpriteFont>    m_font;
    std::unique_ptr<DirectX::SpriteFont>    m_ctrlFont;

    bool                                    m_ctrlConnected;
    
    // WASAPI objects
    Microsoft::WRL::ComPtr<WASAPICapture>           m_captureInterface;
    Microsoft::WRL::ComPtr<WASAPIRenderer>          m_renderInterface;
    std::vector<Windows::Devices::Enumeration::DeviceInformation^> m_captureDevices;
    Windows::Devices::Enumeration::DeviceWatcher^   m_captureWatcher;
    Windows::Foundation::EventRegistrationToken		m_renderEventToken;
    Platform::String^                               m_currentId;
    std::unique_ptr<CBuffer>                        m_captureBuffer;
    WAVEFORMATEX*                                   m_renderFormat;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;
    
    bool                                    m_readInput;
    bool                                    m_finishInit;
    bool                                    m_isRendererSet;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>       m_gamePad;
    std::unique_ptr<DirectX::Keyboard>      m_keyboard;

    DirectX::GamePad::ButtonStateTracker    m_gamePadButtons;
    DirectX::Keyboard::KeyboardStateTracker m_keyboardButtons;
};