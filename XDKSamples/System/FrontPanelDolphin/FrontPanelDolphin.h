//--------------------------------------------------------------------------------------
// FrontPanelDolphin.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "Dolphin.h"

#include "FrontPanel\FrontPanelRenderTarget.h"
#include "FrontPanel\FrontPanelDisplay.h"
#include "FrontPanel\FrontPanelInput.h"


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

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    void SetWaterColor(float red, float green, float blue);

    void DrawDolphin(Dolphin &dolphin);
    
    // Device resources.
    std::unique_ptr<DX::DeviceResources>            m_deviceResources;
                                                     
    // Rendering loop timer.                         
    uint64_t                                        m_frame;
    DX::StepTimer                                   m_timer;
                                                     
    // Input devices.                                
    std::unique_ptr<DirectX::GamePad>               m_gamePad;

    DirectX::GamePad::ButtonStateTracker            m_gamePadButtons;

    // DirectXTK objects.
    std::unique_ptr<DirectX::GraphicsMemory>        m_graphicsMemory;
                                                     
    // DirectXTK objects                             
    std::unique_ptr<DirectX::CommonStates>          m_states;
    std::unique_ptr<DirectX::EffectFactory>         m_fxFactory;
                                                     
    // Game state                                    
    Microsoft::WRL::ComPtr<ID3D11Buffer>            m_VSConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>            m_PSConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>      m_pSamplerMirror;
                                                     
    // Transform matrices                            
    DirectX::SimpleMath::Matrix                     m_matView;
    DirectX::SimpleMath::Matrix                     m_matProj;
                                                     
    // array of dolphins                             
    std::array<std::shared_ptr<Dolphin>, 4>         m_dolphins;

    // Seafloor object
    std::unique_ptr<DirectX::Model>                 m_seafloor;
    std::unique_ptr<DirectX::IEffect>               m_seaEffect;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_seaFloorTextureView;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>       m_seaFloorVertexLayout;

    // Water caustics
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_causticTextureViews[32];
    unsigned int                                    m_currentCausticTextureView;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>       m_pixelShader;

    float                                           m_waterColor[4];
    float                                           m_ambient[4];

    // Front Panel Render Target
    // Helper class to convert a GPU resource to grayscale and then render to the Front Panel
    std::unique_ptr<ATG::FrontPanelRenderTarget>     m_frontPanelRenderTarget; 

    // Shader resource view for the whole screen
    // This is the input to the FrontPanelRender target
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_mainRenderTargetSRV;

    // FrontPanel objects
    Microsoft::WRL::ComPtr<IXboxFrontPanelControl> m_frontPanelControl;
    std::unique_ptr<ATG::FrontPanelDisplay>        m_frontPanelDisplay;
    std::unique_ptr<ATG::FrontPanelInput>          m_frontPanelInput;
    ATG::FrontPanelInput::ButtonStateTracker       m_frontPanelInputButtons;
};
