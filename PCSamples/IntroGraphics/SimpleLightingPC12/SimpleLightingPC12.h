//--------------------------------------------------------------------------------------
// SimpleLightingPC12.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"


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
    void GetDefaultSize( int& width, int& height ) const;

private:

    struct ConstantBuffer
    {
        DirectX::XMMATRIX worldMatrix;
        DirectX::XMMATRIX viewMatrix;
        DirectX::XMMATRIX projectionMatrix;
        DirectX::XMVECTOR lightDir[2];
        DirectX::XMVECTOR lightColor[2];
        DirectX::XMVECTOR outputColor;
    };

    // We'll allocate space for several of these and they will need to be padded for alignment.
    static_assert(sizeof(ConstantBuffer) == 272, "Checking the size here.");

    // D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT < 272 < 2 * D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT
    // Create a union with the correct size and enough room for one ConstantBuffer
    union PaddedConstantBuffer
    {
        ConstantBuffer constants;
        uint8_t bytes[2 * D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT];
    };

    // Check the exact size of the PaddedConstantBuffer to make sure it will align properly
    static_assert(sizeof(PaddedConstantBuffer) == 2 * D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, "PaddedConstantBuffer is not aligned properly");

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>        m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                               m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>           m_gamePad;
    std::unique_ptr<DirectX::Keyboard>          m_keyboard;

    DirectX::GamePad::ButtonStateTracker        m_gamePadButtons;
    DirectX::Keyboard::KeyboardStateTracker     m_keyboardButtons;

    // DirectXTK objects.						 
    Microsoft::WRL::ComPtr<ID3D12RootSignature>  m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  m_lambertPipelineState;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  m_solidColorPipelineState;
    Microsoft::WRL::ComPtr<ID3D12Resource>       m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource>       m_indexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource>       m_perFrameConstants;
    PaddedConstantBuffer*                        m_mappedConstantData;
    D3D12_GPU_VIRTUAL_ADDRESS                    m_constantDataGpuAddr;
    D3D12_VERTEX_BUFFER_VIEW                     m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW                      m_indexBufferView;

    // In this simple sample, we know that there are three draw calls
    // and we will update the scene constants for each draw call.
    static const unsigned int                    c_numDrawCalls = 3;

    // A synchronization fence and an event. These members will be used
    // to synchronize the CPU with the GPU so that there will be no
    // contention for the constant buffers. 
    Microsoft::WRL::ComPtr<ID3D12Fence>          m_fence;
    Microsoft::WRL::Wrappers::Event              m_fenceEvent;

    // Index in the root parameter table
    static const UINT                            c_rootParameterCB = 0;

    // Scene constants, updated per-frame
    float                                        m_curRotationAngleRad;

    // These computed values will be loaded into a ConstantBuffer
    // during Render
    DirectX::XMFLOAT4X4                          m_worldMatrix;
    DirectX::XMFLOAT4X4                          m_viewMatrix;
    DirectX::XMFLOAT4X4                          m_projectionMatrix;
    DirectX::XMFLOAT4                            m_lightDirs[2];
    DirectX::XMFLOAT4                            m_lightColors[2];
    DirectX::XMFLOAT4                            m_outputColor;
};
