//--------------------------------------------------------------------------------------
// D3D12RaytracingAO.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "GeneralHelper.h"
#include "StepTimer.h"
#include "AORaytracingHlslCompat.h"
#include "Menus.h"
#include "Mesh.h"

#include "Lighting.h"
#include "AO.h"
#include "SSAO.h"

namespace GlobalRootSignatureParams {
    enum Value
    {
        OutputViewSlot = 0,
        AccelerationStructureSlot,
        SceneConstantSlot,
        VertexBuffersSlot,
        Count
    };
}

namespace LocalRootSignatureParams {
    enum Value
    {
        CubeConstantSlot = 0,
        Count
    };
}

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
    void GetDefaultSize(int& width, int& height) const;

private:
    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    // Device resources
    std::shared_ptr<DX::DeviceResources>                       m_deviceResources;

    // Rendering loop timer
    DX::StepTimer                                              m_timer;

    // Input devices
    std::unique_ptr<DirectX::GamePad>                          m_gamePad;
    std::unique_ptr<DirectX::Keyboard>                         m_keyboard;

    DirectX::GamePad::ButtonStateTracker                       m_gamePadButtons;
    DirectX::Keyboard::KeyboardStateTracker                    m_keyboardButtons;

    void UpdateCameraMatrices();
    void InitializeScene();
    void CreateConstantBuffers();
    void SetupLighting();

    // DirectXTK objects
    std::unique_ptr<DirectX::GraphicsMemory>                   m_graphicsMemory;

    // Constant Buffers
    union AlignedSceneConstantBuffer
    {
        SceneConstantBuffer constants;
        uint8_t alignmentPadding[CalculateConstantBufferByteSize(sizeof(SceneConstantBuffer))];
    };
    std::vector<AlignedSceneConstantBuffer*>                    m_mappedSceneConstantData;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>         m_mappedSceneConstantResource;

    // Unmapped versions of bound data so we are not constantly copying small packets to the GPU
    std::vector<SceneConstantBuffer>                            m_sceneCB;

    // Lighting model
    std::unique_ptr<Lighting>                                   m_ssao, m_ao;

    // Mesh
    std::vector<std::string>                                    m_meshFiles;
    std::shared_ptr<Mesh>                                       m_mesh;
    unsigned int                                                m_meshIndex;

    // Menus
    std::shared_ptr<Menus>                                      m_menus;

    // Split
    bool                                                        m_isSplit;
    bool                                                        m_isSplitMode;

    // Camera
    float                                                       m_radius;
};
