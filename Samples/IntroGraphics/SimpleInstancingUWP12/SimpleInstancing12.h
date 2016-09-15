//--------------------------------------------------------------------------------------
// SimpleInstancing12.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "Shared.h"


// A basic sample implementation that creates a D3D12 device and
// provides a render loop.
class Sample : public DX::IDeviceNotify
{
public:

    Sample();

    // Initialization and management
    void Initialize(IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation);

    // Basic render loop
    void Tick();
    void Render();

    // Rendering helpers
    void Clear();

    // IDeviceNotify
    virtual void OnDeviceLost() override;
    virtual void OnDeviceRestored() override;

    // Messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation);
    void ValidateDevice();

    // Properties
    void GetDefaultSize( int& width, int& height ) const;

private:

    void Update(DX::StepTimer const& timer);

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    void ResetSimulation();
    float FloatRand(float lowerBound = -1.0f, float upperBound = 1.0f);
    
    // Device resources.
    std::unique_ptr<DX::DeviceResources>        m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                               m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>           m_gamePad;
    std::unique_ptr<DirectX::Keyboard>          m_keyboard;
    std::unique_ptr<DirectX::Mouse>             m_mouse;

    DirectX::GamePad::ButtonStateTracker        m_gamePadButtons;
    DirectX::Keyboard::KeyboardStateTracker     m_keyboardButtons;
    bool                                        m_gamepadPresent;

    // DirectXTK objects.
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;
    std::unique_ptr<DirectX::DescriptorHeap>    m_resourceDescriptors;
    std::unique_ptr<DirectX::SpriteBatch>       m_batch;
    std::unique_ptr<DirectX::SpriteFont>        m_smallFont;
    std::unique_ptr<DirectX::SpriteFont>        m_ctrlFont;

    enum Descriptors
    {
        TextFont,
        ControllerFont,
        Count
    };

    //--------------------------------------------------------------------------------------
    // Sample Objects.
    //--------------------------------------------------------------------------------------

    // Instance vertex definition
    struct Instance
    {
        DirectX::XMFLOAT4 quaternion;
        DirectX::XMFLOAT4 positionAndScale;
    };

    // Light data structure (maps to constant buffer in pixel shader)
    struct Lights
    {
        DirectX::XMFLOAT4 directional;
        DirectX::XMFLOAT4 pointPositions[c_pointLightCount];
        DirectX::XMFLOAT4 pointColors[c_pointLightCount];
    };

    // Direct3D 12 pipeline objects.
    Microsoft::WRL::ComPtr<ID3D12RootSignature>  m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  m_pipelineState;

    // Direct3D 12 resources.
    Microsoft::WRL::ComPtr<ID3D12Resource>       m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW                     m_vertexBufferView[3];
    Microsoft::WRL::ComPtr<ID3D12Resource>       m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW                      m_indexBufferView;
    Microsoft::WRL::ComPtr<ID3D12Resource>       m_boxColors;

    Microsoft::WRL::ComPtr<ID3D12Resource>       m_instanceData;
    uint8_t*                                     m_mappedInstanceData;
    D3D12_GPU_VIRTUAL_ADDRESS                    m_instanceDataGpuAddr;

    // A synchronization fence and an event. These members will be used
    // to synchronize the CPU with the GPU so that there will be no
    // contention for the instance data. 
    Microsoft::WRL::ComPtr<ID3D12Fence>          m_fence;
    Microsoft::WRL::Wrappers::Event              m_fenceEvent;

    struct aligned_deleter { void operator()(void* p) { _aligned_free(p); } };

    std::unique_ptr<Instance[]>                             m_CPUInstanceData;
    std::unique_ptr<DirectX::XMVECTOR[], aligned_deleter>   m_rotationQuaternions;
    std::unique_ptr<DirectX::XMVECTOR[], aligned_deleter>   m_velocities;
    uint32_t                                                m_usedInstanceCount;

    DirectX::XMFLOAT4X4                         m_proj;
    DirectX::XMFLOAT4X4                         m_clip;
    Lights                                      m_lights;
    float                                       m_pitch;
    float                                       m_yaw;

    std::default_random_engine                  m_randomEngine;
};