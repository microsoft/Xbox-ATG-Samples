//--------------------------------------------------------------------------------------
// AdvancedESRAM12.h
//
// This sample showcases an advanced, fragmentless allocation scheme in DirectX12. It leverages 
// the ID3D12CommandQueue::CopyPageMappingsBatchX(...) API to implement a transient D3D resource 
// allocator (TransientAllocator.h). A transient resource is any resource that doesn't persist 
// between frames, such as G-Buffers and intermediate post-process targets. 
//
// The TransientAllocator functions similarly to the XGMemory library, mapping resources to memory 
// on a page-by-page basis, dynamically choosing ESRAM or DRAM depending on user specification and
// availability. The difference here being the mapping of the resource's virtual memory space occurs
// on the GPU timeline instead of the CPU. This allows the mapping to be determined dynamically during
// command list recording instead of requiring a pre-configured memory layout beforehand.
//
// No copy-in/-out ESRAM functionality is implemented for brevity's sake. A copy-in extension could 
// be highly effective since read-only resources can be copied into memory far in advance, referenced 
// in shaders, then discarded and reused at no additional GPU cost.
//
// The structures presented in the sample are single-thread ready only. Extending this to recording 
// multiple command lists in parallel over one queue shouldn't be too painful - extension to 2+ queues 
// would be an additional challenge.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"

#include "EsramVisualizeEffect.h"
#include "TransientAllocator.h"

enum SceneTexture
{
    ST_Color,
    ST_Depth,
    ST_Outline0,
    ST_Outline1,
    ST_Bloom0,
    ST_Bloom1,
    ST_Count,
};

class Sample
{
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
    void DrawHUD(ID3D12GraphicsCommandList* commandList);

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    ATG::TransientResource AcquireTransientTexture(ID3D12GraphicsCommandList* list, const ATG::TransientDesc& desc, const D3D12_RESOURCE_STATES& initialState, SceneTexture tex);
    void UpdateVisualizerRanges(const ATG::ResourceHandle(&handles)[ST_Count]);
    void UpdateVisualizerTimings();

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
    std::unique_ptr<DirectX::DescriptorPile>        m_srvPile;
    std::unique_ptr<DirectX::EffectTextureFactory>  m_textureFactory;

    // HUD
    std::unique_ptr<DirectX::SpriteBatch>           m_hudBatch;
    std::unique_ptr<DirectX::SpriteFont>            m_smallFont;
    std::unique_ptr<DirectX::SpriteFont>            m_ctrlFont;

    // Camera
    float                                           m_theta;
    float                                           m_phi;
    float                                           m_radius;
    DirectX::SimpleMath::Matrix                     m_proj;
    DirectX::SimpleMath::Matrix                     m_view;

    // Assets & Scene
    std::vector<std::unique_ptr<DirectX::Model>>    m_models;
    std::vector<ObjectInstance>                     m_scene;

    // Post Processing
    std::unique_ptr<DirectX::GeometricPrimitive>    m_fullScreenTri;
    std::unique_ptr<DirectX::ToneMapPostProcess>    m_tonemapEffect;
    std::unique_ptr<DirectX::BasicPostProcess>      m_blurEffect;
    std::unique_ptr<DirectX::BasicPostProcess>      m_bloomExtractEffect;
    std::unique_ptr<DirectX::BasicPostProcess>      m_bloomBlurEffect;
    std::unique_ptr<DirectX::DualPostProcess>       m_bloomCombineEffect;

    std::unique_ptr<DirectX::BasicEffect>           m_alphaCompositeEffect;
    std::unique_ptr<DirectX::BasicEffect>           m_emissiveEffect;

    // Sample Variables
    std::unique_ptr<ATG::TransientAllocator>        m_allocator;
    ATG::TransientDesc                              m_colorDesc;
    ATG::TransientDesc                              m_depthDesc;
    ATG::TransientDesc                              m_outlineDesc;
    ATG::TransientDesc                              m_bloomDesc;

    std::unique_ptr<ATG::EsramVisualizeEffect>      m_esramVisualizeEffect;
    float                                           m_esramRatios[ST_Count];
    float                                           m_esramChangeRate;

    int                                             m_outlineObjectIndex;
    bool                                            m_updateStats;

    uint32_t                                        padding;
    ATG::EsramVisualizeEffect::Constants            m_visData;
};
