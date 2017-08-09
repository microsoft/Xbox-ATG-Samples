//--------------------------------------------------------------------------------------
// SimpleTexturePC.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"


// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
class Sample : public DX::IDeviceNotify
{
public:

    Sample();

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
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>       m_gamePad;
    std::unique_ptr<DirectX::Keyboard>      m_keyboard;

    // Sample objects
    Microsoft::WRL::ComPtr<ID3D11InputLayout>           m_spInputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer>                m_spVertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>                m_spIndexBuffer;
    Microsoft::WRL::ComPtr<ID3D11VertexShader>          m_spVertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>           m_spPixelShader;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>          m_spSampler;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>    m_spTexture;
};