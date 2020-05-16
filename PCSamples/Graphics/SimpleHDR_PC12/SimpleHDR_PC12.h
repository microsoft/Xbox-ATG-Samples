//--------------------------------------------------------------------------------------
// SimpleHDR_PC12.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "FullScreenQuad\FullScreenQuad.h"
#include "RenderTexture.h"


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

    DirectX::GamePad::ButtonStateTracker        m_gamePadButtons;
    DirectX::Keyboard::KeyboardStateTracker     m_keyboardButtons;

    bool                                        m_ctrlConnected;

    // DirectXTK objects.
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;

#pragma region Standard Sample Defines

    std::unique_ptr<DirectX::DescriptorHeap>            m_rtvDescriptorHeap;
    std::unique_ptr<DirectX::DescriptorHeap>            m_resourceDescriptorHeap;
    std::unique_ptr<DirectX::SpriteFont>                m_textFont;
    std::unique_ptr<DirectX::SpriteFont>                m_controllerFont;
    std::unique_ptr<DirectX::SpriteBatch>               m_fontBatch;
    std::unique_ptr<DirectX::SpriteBatch>               m_spriteBatch;
    std::unique_ptr<DirectX::BasicEffect>               m_lineEffect;
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_primitiveBatch;
    std::unique_ptr<DX::FullScreenQuad>                 m_fullScreenQuad;

#pragma endregion

#pragma region HDR Defines                                                                                                  

    bool                                                m_bRender2084Curve;                     // Render the ST.2084 curve rather than the HDR scene
    bool                                                m_bShowOnlyPaperWhite;                  // If enabled, only the block with value 1.0f (paper white) will be rendered. Seeing bright values next to white can have the effect of perceiving white as gray
    double                                              m_countDownToBright;                    // The countdown before rendering bright values at the start of the sample, so that eyes can adjust to what paper white looks like, to realize the difference between white and bright
    float                                               m_current2084CurveRenderingNits;        // In the mode when rendering the curve, use this as the adjustable value indicated on the graph

    static const int                                    c_CustomInputValueIndex = 3;            // Index of input values set by left/right sticks, others use fixed values
    static const int                                    c_NumInputValues = 4;
    float                                               m_hdrSceneValues[c_NumInputValues];     // Values that will be rendering to the HDR scene buffer  
    std::unique_ptr<DX::RenderTexture>                  m_hdrScene;

    struct HDR10Data
    {
        float PaperWhiteNits;                                                                   // Defines how bright white is (in nits), which controls how bright the SDR range in the image will be, e.g. 200 nits
    } m_HDR10Data;

    void RenderHDRScene();
    void Render2084Curve();
    void RenderUI();
    void PrepareSwapChainBuffer();                                                             // Takes as input the HDR scene values and outputs an HDR and SDR signal to two seperate swapchains

#pragma endregion

#pragma region D3D12 Defines

    Microsoft::WRL::ComPtr<ID3D12RootSignature>         m_d3dRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>         m_d3dRenderHDRScenePSO;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>         m_d3dPrepareSwapChainBufferPSO;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>         m_d3dTonemapSwapChainBufferPS;

    // Descriptors for m_rtvDescriptorHeap 
    enum class RTVDescriptors
    {
        HDRScene,
        Count
    };

    // Desriptors for m_resourceDescriptorHeap
    enum class ResourceDescriptors
    {
        HDRScene,
        TextFont,
        ControllerFont,
        Count
    };

#pragma endregion
};
