//--------------------------------------------------------------------------------------
// SimpleDeviceAndSwapChain.h
//
// Setting up a Direct3D 11 device and swapchain for a Xbox One app
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "StepTimer.h"


// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
class Sample
{
public:

    Sample();

    // Initialization and management
    void Initialize(IUnknown* window);

    // Basic game loop
    void Tick();

    // Messages
    void OnSuspending();
    void OnResuming();

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();
    void Present();

    void CreateDevice();
    void CreateResources();

    // Application state
    IUnknown*                                       m_window;
    int                                             m_outputWidth;
    int                                             m_outputHeight;

    // Direct3D Objects
    D3D_FEATURE_LEVEL                               m_featureLevel;
    Microsoft::WRL::ComPtr<ID3D11DeviceX>           m_d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContextX>    m_d3dContext;

    // Rendering resources
    Microsoft::WRL::ComPtr<IDXGISwapChain1>         m_swapChain;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>         m_renderTarget;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>         m_depthStencil;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  m_renderTargetView;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>  m_depthStencilView;

    // Rendering loop timer.
    uint64_t                                        m_frame;
    DX::StepTimer                                   m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>               m_gamePad;

    // DirectXTK objects.
    std::unique_ptr<DirectX::GraphicsMemory>        m_graphicsMemory;
    std::unique_ptr<DirectX::SpriteBatch>           m_batch;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_background;
};
