//--------------------------------------------------------------------------------------
// SimplePBRUWP12.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "../Shared/StepTimer.h"
#include "../Shared/SharedSimplePBR.h"

// A basic sample implementation that creates a D3D12 device and
// provides a render loop.
class Sample : public DX::IDeviceNotify
{
public:
    friend class SharedSimplePBR;

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

    inline static DXGI_FORMAT GetHDRRenderFormat() { return DXGI_FORMAT_R16G16B16A16_FLOAT; }
    inline static DXGI_FORMAT GetBackBufferFormat() { return DXGI_FORMAT_R16G16B16A16_FLOAT; }
    inline static DXGI_FORMAT GetDepthFormat() { return DXGI_FORMAT_D32_FLOAT; }

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
 
    // Input
    std::unique_ptr<DirectX::Keyboard>          m_keyboard;
    DirectX::Keyboard::KeyboardStateTracker     m_keyboardButtons;
    std::unique_ptr<DirectX::Mouse>             m_mouse;

    // DirectXTK objects.
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;

    // Core shared sample object
    std::unique_ptr<SharedSimplePBR>            m_sharedSimplePBR;
};