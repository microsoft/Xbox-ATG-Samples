//--------------------------------------------------------------------------------------
// SimpleHDR12.h
//
// This sample uses an API to determine if the attached display is HDR capable. If so, it
// will switch the display to HDR mode. A very simple HDR scene, with values above 1.0f,
// is rendered to a FP16 backbuffer and outputs to two different swapchains, one for HDR
// and one for SDR. Even if the consumer uses an HDR display, the SDR signal is still
// required for GameDVR and screenshots.
//
// Requirements for swapchain creation:
//  1) HDR swapchain has to be DXGI_FORMAT_R10G10B10A2_UNORM
//  2) HDR swapchain has to use XGIX_SWAP_CHAIN_FLAG_COLORIMETRY_RGB_BT2020_ST2084
//
// See DeviceResources.cpp for swapchain creation and presentation
//
// Refer to the white paper “HDR on Xbox One”
//
// Up to now, games were outputting and SDR signal using Rec.709 color primaries and Rec.709
// gamma curve. One new feature of UHD displays is a wider color gamut (WCG). To use this
// we need to use a new color space, Rec.2020 color primaries. Another new feature of UHD
// displays is high dynamic range (HDR). To use this we need to use a different curve, the
// ST.2084 curve. Therefore, to output an HDR signal, it needs to use Rec.2020 color primaries
// with ST.2084 curve.
//
// For displaying the SDR signal, a simple tonemapping shader is applied to simply clip all
// values above 1.0f in the HDR scene, and outputs 8bit values using Rec.709 color primaries,
// with the gamma curve applied.
//
// For displaying the HDR signal, a shader is used to rotate the Rec.709 color primaries to
// Rec.2020 color primaries, and then apply the ST.2084 curve to output 10bit values which
// the HDR display can correctly display. The whiteness and brightness of the output on an
// HDR display will be determined by the selected nits value for defining “paper white”.
// SDR specs define “paper white” as 80nits, but this is for a cinema with a dark environment.
// Consumers today are used to much brighter whites, e.g. ~550 nits for a smartphone(so that it
// can be viewed in sunlight), 200-300 nits for a PC monitor, 120-150 nits for an SDR TV, etc.
// The nits for “paper white” can be adjusted in the sample using the DPad up/down. Displaying
// bright values next to white can be deceiving to the eye, so you can use the A button to
// toggle if you only want to see the “paper white” block.
//
// The sample has two modes:
//  1) Render blocks with specific values in the scene
//	2) Render ST.2084 curve with specific brightness values(nits)
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "FullScreenQuad.h"
#include "RenderTexture.h"

// A basic sample implementation that creates a D3D12 device and provides a render loop.
class Sample
{
#pragma region Standard Sample Defines
public:

    Sample();

    // Initialization and management
    void Initialize(IUnknown* window);

    // Basic Sample loop
    void Tick();

    // Messages
    void OnSuspending();
    void OnResuming();

private:
    void Init(IUnknown* window);
    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();
    void InitializeSpriteFonts(ID3D12Device* d3dDevice, DirectX::ResourceUploadBatch& resourceUpload, DirectX::RenderTargetState& rtState);

    // Standard sample defines
    std::unique_ptr<DX::DeviceResources>                m_deviceResources;
    uint64_t                                            m_frame;
    DX::StepTimer                                       m_timer;
    std::unique_ptr<DirectX::GamePad>                   m_gamePad;
    DirectX::GamePad::ButtonStateTracker                m_gamePadButtons;
    std::unique_ptr<DirectX::GraphicsMemory>            m_graphicsMemory;
    std::unique_ptr<DirectX::DescriptorHeap>            m_rtvDescriptorHeap;
    std::unique_ptr<DirectX::DescriptorHeap>            m_resourceDescriptorHeap;
    std::unique_ptr<DirectX::SpriteFont>                m_textFont;
    std::unique_ptr<DirectX::SpriteFont>                m_controllerFont;
    std::unique_ptr<DirectX::SpriteBatch>               m_fontBatch;
    std::unique_ptr<DirectX::SpriteBatch>               m_spriteBatch;
    std::unique_ptr<DirectX::CommonStates>              m_states;
    std::unique_ptr<DirectX::BasicEffect>               m_lineEffect;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>           m_inputLayout;
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_primitiveBatch;
    std::unique_ptr<DX::FullScreenQuad>                 m_fullScreenQuad;

#pragma endregion

#pragma region HDR Defines                                                                                                  

    std::atomic_bool                                    m_bIsTVInHDRMode;                       // This will be set to TRUE if the attached display is in HDR mode
    bool                                                m_bRender2084Curve;                     // Render the ST.2084 curve rather than the HDR scene
    bool                                                m_bShowOnlyPaperWhite;                  // If enabled, only the block with value 1.0f (paper white) will be rendered. Seeing bright values next to white can have the effect of perceiving white as gray
    double                                              m_countDownToBright;                    // The countdown before rendering bright values at the start of the sample, so that eyes can adjust to what paper white looks like, to realize the difference between white and bright
    float                                               m_current2084CurveRenderingNits;        // In the mode when rendering the curve, use this as the adjustable value indicated on the graph
    const int                                           g_CustomInputValueIndex = 3;            // Index of input values set by left/right sticks, others use fixed values
    static const int                                    NUM_INPUT_VALUES = 4;
    float                                               m_hdrSceneValues[NUM_INPUT_VALUES] = { 0.5f, 1.0f, 6.0f, 10.0f };   // Values that will be rendering to the HDR scene buffer  
    std::unique_ptr<DX::RenderTexture>                  m_hdrScene;

    struct HDR10Data
    {
        float PaperWhiteNits;                                                                   // Defines how bright white is (in nits), which controls how bright the SDR range in the image will be, e.g. 200 nits
    } m_HDR10Data;

    void RenderHDRScene();
    void Render2084Curve();
    void RenderUI();
    void PrepareSwapChainBuffers();                                                             // Takes as input the HDR scene values and outputs an HDR and SDR signal to two seperate swapchains

    inline DirectX::XMVECTOR MakeColor(float value) { DirectX::XMVECTORF32 color = { value, value, value, 1.0f }; return color; }

#pragma endregion

#pragma region D3D12 Defines

    Microsoft::WRL::ComPtr<ID3D12RootSignature>         m_d3dRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>         m_d3dRenderHDRScenePSO; 
    Microsoft::WRL::ComPtr<ID3D12PipelineState>         m_d3dPrepareSwapChainBufferPSO;
   
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
