//--------------------------------------------------------------------------------------
// SimpleRaytracingInstancing_PC12.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"

// A basic sample implementation that creates a D3D12 device and provides a render loop.
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

    // Initialization and management
    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    // Basic loop
    void Update();
    void Render();
    void Clear();

    // Device resources
    std::shared_ptr<DX::DeviceResources>      m_deviceResources;

    // Input devices
    std::unique_ptr<DirectX::GamePad>         m_gamePad;
    std::unique_ptr<DirectX::Keyboard>        m_keyboard;

    DirectX::GamePad::ButtonStateTracker      m_gamePadButtons;
    DirectX::Keyboard::KeyboardStateTracker   m_keyboardButtons;

    // Geometry
    using Index = uint16_t;
    struct Vertex { float v1, v2, v3; };

    void BuildSceneGeometry();

    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_bottomLevelInstancesBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_topLevelInstancesBuffer;

    //Init
    void CreateRaytracingRootSignatures();
    void SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, Microsoft::WRL::ComPtr<ID3D12RootSignature>* rootSig);
    void CreateRaytracingPipelineStateObject();
    void CreateRaytracingDescriptorHeap();
    void BuildRaytracingAccelerationStructures();
    void BuildRaytracingShaderTables();
    void CreateRaytracingOutputResource();
    UINT AllocateRaytracingDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse = UINT_MAX);

    //Runtime
    void DoRaytracing();
    void CopyRaytracingOutputToBackbuffer();

    // Root signatures
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_raytracingGlobalRootSignature;

    // Descriptors
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_raytracingDescriptorHeap;
    UINT m_raytracingDescriptorsAllocated;
    UINT m_raytracingDescriptorSize;

    // Acceleration structure
    Microsoft::WRL::ComPtr<ID3D12Resource> m_bottomLevelAccelerationStructure;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_topLevelAccelerationStructure;

    // Raytracing output
    Microsoft::WRL::ComPtr<ID3D12Resource> m_raytracingOutput;
    D3D12_GPU_DESCRIPTOR_HANDLE m_raytracingOutputResourceUAVGpuDescriptor;
    UINT m_raytracingOutputResourceUAVDescriptorHeapIndex;

    // State object
    Microsoft::WRL::ComPtr<ID3D12StateObject> m_dxrStateObject;

    // Shader tables
    Microsoft::WRL::ComPtr<ID3D12Resource> m_missShaderTable;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_hitGroupShaderTable;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_rayGenShaderTable;
};
