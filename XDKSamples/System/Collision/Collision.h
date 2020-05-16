//--------------------------------------------------------------------------------------
// Collision.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "ControllerHelp.h"
#include "OrbitCamera.h"


// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
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

    void InitializeObjects();
    void Animate(double fTime);
    void Collide();
    void SetViewForGroup(int group);

    // Device resources.
    std::unique_ptr<DX::DeviceResources>        m_deviceResources;

        // Rendering loop timer.
    uint64_t                                    m_frame;
    DX::StepTimer                               m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>           m_gamePad;

    DirectX::GamePad::ButtonStateTracker        m_gamePadButtons;

    // Rendering objects.
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;
    std::unique_ptr<DirectX::CommonStates>      m_states;
    std::unique_ptr<DirectX::BasicEffect>       m_effect;
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_batch;
    std::unique_ptr<DirectX::SpriteFont>        m_font;
    std::unique_ptr<DirectX::SpriteFont>        m_ctrlFont;
    std::unique_ptr<DirectX::SpriteBatch>       m_sprites;

    Microsoft::WRL::ComPtr<ID3D11InputLayout>   m_layout;

    std::wstring                                m_name;

    // Sample Help.
    std::unique_ptr<ATG::Help>                  m_help;
    bool                                        m_showHelp;

    // View camera.
    DX::OrbitCamera                             m_camera;

    // Collision sample.
    using BoundingSphere = DirectX::BoundingSphere;
    using BoundingOrientedBox = DirectX::BoundingOrientedBox;
    using BoundingBox = DirectX::BoundingBox;
    using BoundingFrustum = DirectX::BoundingFrustum;
    using ContainmentType = DirectX::ContainmentType;
    using Vector3 = DirectX::SimpleMath::Vector3;

    struct CollisionSphere
    {
        BoundingSphere sphere;
        ContainmentType collision;
    };

    struct CollisionBox
    {
        BoundingOrientedBox obox;
        ContainmentType collision;
    };

    struct CollisionAABox
    {
        BoundingBox aabox;
        ContainmentType collision;
    };

    struct CollisionFrustum
    {
        BoundingFrustum frustum;
        ContainmentType collision;
    };

    struct CollisionTriangle
    {
        Vector3 pointa;
        Vector3 pointb;
        Vector3 pointc;
        ContainmentType collision;
    };

    struct CollisionRay
    {
        Vector3 origin;
        Vector3 direction;
    };

    static const size_t c_groupCount = 4;

    BoundingFrustum     m_primaryFrustum;
    BoundingOrientedBox m_primaryOrientedBox;
    BoundingBox         m_primaryAABox;
    CollisionRay        m_primaryRay;

    CollisionSphere     m_secondarySpheres[c_groupCount];
    CollisionBox        m_secondaryOrientedBoxes[c_groupCount];
    CollisionAABox      m_secondaryAABoxes[c_groupCount];
    CollisionTriangle   m_secondaryTriangles[c_groupCount];

    CollisionAABox      m_rayHitResultBox;

    Vector3             m_cameraOrigins[c_groupCount];
};
