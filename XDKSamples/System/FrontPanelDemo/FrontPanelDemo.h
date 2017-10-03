//--------------------------------------------------------------------------------------
// FrontPanelDemo.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "FrontPanelManager.h"
#include "Dolphin.h"

#include <deque>

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

    void AddNewDolphins(unsigned count);
    void RemoveDolphin();
    void ClearDolphins();
    void ToggleWireframe();
    void TogglePause();
    
    void PauseSimulation(bool pause);
    
    // Device resources.
    std::unique_ptr<DX::DeviceResources>            m_deviceResources;
                                                     
    // Rendering loop timer.                         
    uint64_t                                        m_frame;
    DX::StepTimer                                   m_timer;
    bool                                            m_paused;
    bool                                            m_wireframe;
                                                     
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
    std::deque<std::shared_ptr<Dolphin>>            m_dolphins;

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

    // Front Panel Manager
    FrontPanelManager                               m_frontPanelManager;
};
