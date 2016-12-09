//--------------------------------------------------------------------------------------
// SharedSimplePBR.h
//
// Shared sample class to demonstrate PBRModel and PBREffect in DirectX 12 on Xbox ERA
// and PC UWP.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "StepTimer.h"

#include "PBREffect/PBREffect.h"
#include "ToneMapEffect/ToneMapEffect.h"
#include "Skybox/Skybox.h"
#include "GeometricPrimitive.h"
#include "PBRModel.h"
#include "DescriptorPile.h"

class Sample;

class SharedSimplePBR
{
public:
    SharedSimplePBR(Sample* sample);
    ~SharedSimplePBR() {}

    void Update(DX::StepTimer const& timer);
    void Render();
    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();
    void OnDeviceLost();

private:
    Sample* m_sample;

    // Hud
    std::unique_ptr<DirectX::SpriteBatch>       m_hudBatch;
    std::unique_ptr<DirectX::SpriteFont>        m_smallFont;
    std::unique_ptr<DirectX::SpriteFont>        m_ctrlFont;

    // Input and Camera
    std::unique_ptr<DirectX::GamePad>           m_gamePad;
    DirectX::GamePad::ButtonStateTracker        m_gamePadButtons;
    std::unique_ptr<DX::OrbitCamera>            m_camera;

    // Render states
    std::unique_ptr<DirectX::CommonStates>      m_commonStates;

    // All SRV descriptors for sample
    std::unique_ptr<ATG::DescriptorPile>         m_srvPile;

    // Drawing
    using DebugVert = DirectX::VertexPositionColor;
    using ToneMapVert = ATG::ToneMapEffect::VertexType;

    std::unique_ptr<DirectX::SpriteBatch>                   m_spriteBatch;
    std::unique_ptr<ATG::ToneMapEffect>                     m_toneMapEffect;
    std::unique_ptr<DirectX::PrimitiveBatch<ToneMapVert>>   m_toneMapBatch;

    // Render target view for tonemapping
    std::unique_ptr<DirectX::DescriptorHeap>        m_rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_rtvHDRBuffer;
    
    // Shader resource view for tonemapping
    ATG::DescriptorPile::IndexType                  m_HDRBufferDescIndex;

    // Sky/Environment textures
    ATG::DescriptorPile::IndexType                  m_radTexDescIndex;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_radianceTexture;

    // Irradiance texture
    ATG::DescriptorPile::IndexType                  m_irrTexDescIndex;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_irradianceTexture;

    // Model
    std::vector< std::unique_ptr<ATG::PBRModel>>    m_pbrModels;

    // Skybox
    std::unique_ptr<ATG::Skybox>                    m_skybox;
};