//--------------------------------------------------------------------------------------
// SimpleHDR_PC.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "FullScreenQuad\FullScreenQuad.h"


// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
class Sample final : public DX::IDeviceNotify
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
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>       m_gamePad;
    std::unique_ptr<DirectX::Keyboard>      m_keyboard;

    DirectX::GamePad::ButtonStateTracker    m_gamePadButtons;
    DirectX::Keyboard::KeyboardStateTracker m_keyboardButtons;

    bool                                    m_ctrlConnected;

    // Standard sample defines
    std::unique_ptr<DirectX::SpriteFont>                m_font;
    std::unique_ptr<DirectX::SpriteFont>                m_controllerFont;
    std::unique_ptr<DirectX::SpriteBatch>               m_spriteBatch;
    std::unique_ptr<DirectX::CommonStates>              m_states;
    std::unique_ptr<DirectX::BasicEffect>               m_lineEffect;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>           m_inputLayout;
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_primitiveBatch;
    std::unique_ptr<DX::FullScreenQuad>                 m_fullScreenQuad;

    // HDR defines
    void PrepareSwapChainBuffer();                                                              // Takes as input the HDR scene values and outputs an HDR or SDR signal

    bool                                                m_bRender2084Curve;                     // Render the ST.2084 curve rather than the HDR scene
    bool                                                m_bShowOnlyPaperWhite;                  // If enabled, only the block with value 1.0f (paper white) will be rendered. Seeing bright values next to white can have the effect of perceiving white as gray
    double                                              m_countDownToBright;                    // The countdown before rendering bright values at the start of the sample, so that eyes can adjust to what paper white looks like, to realize the difference between white and bright
    float                                               m_currentPaperWhiteNits;                // Current brightness for paper white

    static const int                                    c_CustomInputValueIndex = 3;            // Index of input values set by left/right sticks, others use fixed values
    static const int                                    c_NumInputValues = 4;
    float                                               m_hdrSceneValues[c_NumInputValues];     // Values that will be rendering to the HDR scene buffer  
                                                                                                // Rendering the HDR scene
    Microsoft::WRL::ComPtr<ID3D11PixelShader>           m_d3dColorPS;                           // Simple shader to output only color, useful to oupput very specific HDR color values
    Microsoft::WRL::ComPtr<ID3D11Texture2D>             m_d3dHDRSceneTexture;                   // HDR values will be rendered into this buffer
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>      m_d3dHDRSceneRTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>    m_d3dHDRSceneSRV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>    m_defaultTex;

    float m_current2084CurveRenderingNits;                                                      // In the mode when rendering the curve, use this as the adjustable value indicated on the graph

    void CreateHDRSceneResources();
    void RenderHDRScene();
    void Render2084Curve();
    void RenderUI();

    // Prepares HDR and SDR swapchain buffers
    Microsoft::WRL::ComPtr<ID3D11SamplerState>          m_d3dPointSampler;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>           m_d3dPrepareSwapChainBuffersPS;         // Outputs an signal for the swapchain buffers to correctly be displayed in HDR/SDR
    Microsoft::WRL::ComPtr<ID3D11PixelShader>           m_d3dTonemapSwapChainBufferPS;
    Microsoft::WRL::ComPtr<ID3D11Buffer>                m_d3dNitsForPaperWhiteCB;               // Define the nit value of "paper white", e.g. 100 nits    

    void Create2084CurveResources();
};