//--------------------------------------------------------------------------------------
// SimpleESRAM12.h
//
// This sample demonstrates the basics of utilizing ESRAM with DirectX12. It leverages the 
// XG & XGMemory APIs to reserve virtual resource memory and subsequently map it to DRAM & ESRAM. 
// A few different page mapping schemes are showcased to provide examples of how the XGMemory 
// library can be used to customize resource layout between DRAM & ESRAM.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "PerformanceTimersXbox.h"
#include "StepTimer.h"

constexpr int PlaneCount = _countof(XG_RESOURCE_LAYOUT::Plane);


struct MetadataDesc
{
    struct PageRange
    {
        int start;
        int count;

        int End() const { return start + count; }
    };

    int count;
    PageRange ranges[PlaneCount];

    int End() { assert(count > 0); return ranges[count - 1].End(); }
};

class Sample
{
public:

    Sample() noexcept(false);
    ~Sample() = default;

    Sample(Sample&&) = default;
    Sample& operator= (Sample&&) = default;

    Sample(Sample const&) = delete;
    Sample& operator= (Sample const&) = delete;

    // Initialization and management
    void Initialize(IUnknown* window);

    // Basic Sample loop
    void Tick();

    // Messages
    void OnSuspending();
    void OnResuming();

    // Properties
    bool RequestHDRMode() const noexcept { return m_deviceResources ? (m_deviceResources->GetDeviceOptions() & DX::DeviceResources::c_EnableHDR) != 0 : false; }

private:
    void Update(const DX::StepTimer& timer);
    void Render();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    void UpdateResourceMappings();

private:
    // Enumerates the example ESRAM mapping schemes for the sample.
    enum EsramMappingScheme
    {
        EMS_None = 0, // Maps all resource memory to DRAM.
        EMS_Simple,   // Maps a specified number of pages to ESRAM and the remaining pages to DRAM.
        EMS_Split,    // Splits the resource into a beginning DRAM section, a middle ESRAM section, and an ending DRAM section.
        EMS_Metadata, // Only maps the resource metadata to ESRAM.
        EMS_Random,   // Performs a randomized per-page choice of DRAM or ESRAM  according to a specified probability.
        EMS_Count
    };

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
    std::unique_ptr<DirectX::DescriptorPile>        m_rtvPile;
    std::unique_ptr<DirectX::DescriptorPile>        m_dsvPile;
                                                    
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
    std::unique_ptr<DirectX::EffectTextureFactory>  m_textureFactory;
    std::vector<std::unique_ptr<DirectX::Model>>    m_models;
    std::vector<ObjectInstance>                     m_scene;

    // Post Processing
    std::unique_ptr<DirectX::GeometricPrimitive>    m_fullScreenTri;
    std::unique_ptr<DirectX::BasicEffect>           m_manualClearEffect;
    std::unique_ptr<DirectX::BasicEffect>           m_esramBlendEffect;

    // Misc.
    std::mt19937                                    m_generator;


    //--------------------------------
    // ESRAM-pertinent variables

    bool                                            m_showOverlay = false;
    EsramMappingScheme                              m_mapScheme = EMS_None;
    int                                             m_colorEsramPageCount = 0;  // Simple Mapping
    int                                             m_depthEsramPageCount = 0;    
    float                                           m_bottomPercent = 0.2f;     // Split Mapping
    float                                           m_topPercent = 0.8f;
    bool                                            m_metadataEnabled = true;   // Metadata Mapping
    float                                           m_esramProbability = 0.5f;  // Random Mapping

    // XG Memory
    XGMemoryLayoutEngine                            m_layoutEngine;
    Microsoft::WRL::ComPtr<XGMemoryLayout>          m_layout;

    // Color & Depth Resources
    int                                             m_colorPageCount = 0;
    MetadataDesc                                    m_colorLayoutDesc;
    int                                             m_depthPageCount = 0;
    MetadataDesc                                    m_depthLayoutDesc;
    int                                             m_esramOverlayPageCount = 0;

    // Main render targets.
    D3D12_RESOURCE_DESC                             m_colorDesc;
    D3D12_RESOURCE_DESC                             m_depthDesc;
    D3D12_RESOURCE_DESC                             m_esramOverlayDesc;

    Microsoft::WRL::ComPtr<ID3D12Resource>          m_colorTexture;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_depthTexture;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_esramOverlayTexture;
};
