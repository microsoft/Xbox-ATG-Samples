//--------------------------------------------------------------------------------------
// AdvancedESRAM12.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"

#include "BokehEffect12.h"

class Sample
{
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
    Sample();

    // Initialization and management
    void Initialize(IUnknown* window);

    // Basic Sample loop
    void Tick();

    // Messages
    void OnSuspending();
    void OnResuming();

    // Properties
    bool RequestHDRMode() const { return m_deviceResources ? (m_deviceResources->GetDeviceOptions() & DX::DeviceResources::c_EnableHDR) != 0 : false; }

private:
    void Update(const DX::StepTimer& timer);
    void Render();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    void SetPredefinedScene(int index);
    void CalculateCameraMatrix();

private:
    // Represents an instance of a scene object.
    struct ObjectInstance
    {
        using EffectList = std::vector<std::shared_ptr<DirectX::IEffect>>;

        DirectX::SimpleMath::Matrix world;
        DirectX::Model*             model;
        EffectList                  effects;
    };

private:
    // Device resources.
    std::unique_ptr<DX::DeviceResources>            m_deviceResources;
    int                                             m_displayWidth;
    int                                             m_displayHeight;

    // Rendering loop timer.
    uint64_t                                        m_frame;
    DX::StepTimer                                   m_timer;
    std::unique_ptr<DX::GPUTimer>                   m_profiler;

    // Input devices.
    DirectX::GamePad                                m_gamePad;
    DirectX::GamePad::ButtonStateTracker            m_gamePadButtons;

    // DirectXTK objects.                           
    std::unique_ptr<DirectX::GraphicsMemory>        m_graphicsMemory;
    std::unique_ptr<DirectX::CommonStates>          m_commonStates;
    std::unique_ptr<DirectX::DescriptorPile>        m_cpuPile;
    std::unique_ptr<DirectX::DescriptorPile>        m_srvPile;
    std::unique_ptr<DirectX::DescriptorPile>        m_rtvPile;
    std::unique_ptr<DirectX::DescriptorPile>        m_dsvPile;
    std::unique_ptr<DirectX::EffectTextureFactory>  m_textureFactory;

    std::unique_ptr<DirectX::BasicPostProcess>      m_copyShader;

    // HUD
    std::unique_ptr<DirectX::SpriteBatch>           m_hudBatch;
    std::unique_ptr<DirectX::SpriteFont>            m_smallFont;
    std::unique_ptr<DirectX::SpriteFont>            m_ctrlFont;

    // Assets & Scene
    std::vector<std::unique_ptr<DirectX::Model>>    m_models;
    std::vector<ObjectInstance>                     m_scene;

    std::unique_ptr<ATG::BokehEffect>               m_bokehDOF;
    ATG::BokehEffect::Parameters                    m_bokehParams;

    ComPtr<ID3D12Resource>                          m_sceneColor;
    ComPtr<ID3D12Resource>                          m_sceneDepth;

    DirectX::XMMATRIX	                            m_matView;
    DirectX::XMMATRIX                               m_matProj;
    DirectX::XMMATRIX                               m_matInvProj;

    int                                             m_presetScene;
    float                                           m_cameraAngle;
    float                                           m_cameraElevation;
    float                                           m_cameraDistance;
};
