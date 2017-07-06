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
#include "Skybox/Skybox.h"
#include "GeometricPrimitive.h"
#include "PBRModel.h"
#include "RenderTexture.h"

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
    bool                                        m_gamepadConnected;

    // Render states
    std::unique_ptr<DirectX::CommonStates>      m_commonStates;

    // All SRV descriptors for sample
    std::unique_ptr<DirectX::DescriptorPile>    m_srvPile;

    enum StaticDescriptors
    {
        Font,
        CtrlFont,
        SceneTex,
        RadianceTex,
        IrradianceTex,
        Reserve
    };

    // Drawing
    using DebugVert = DirectX::VertexPositionColor;

    std::unique_ptr<DirectX::SpriteBatch>           m_spriteBatch;
    std::unique_ptr<DirectX::ToneMapPostProcess>    m_toneMap;

    // Render target view for tonemapping
    std::unique_ptr<DX::RenderTexture>              m_hdrScene;
    std::unique_ptr<DirectX::DescriptorHeap>        m_rtvHeap;

    // Sky/Environment textures
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_radianceTexture;

    // Irradiance texture
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_irradianceTexture;

    // Model
    std::vector< std::unique_ptr<ATG::PBRModel>>    m_pbrModels;

    // Skybox
    std::unique_ptr<ATG::Skybox>                    m_skybox;
};