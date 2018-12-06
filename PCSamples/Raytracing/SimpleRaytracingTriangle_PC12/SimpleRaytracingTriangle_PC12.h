//--------------------------------------------------------------------------------------
// D3D12RaytracingAO.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "GeneralHelper.h"

namespace GlobalRootSignatureParams {
    enum Value
    {
        OutputViewSlot = 0,
        AccelerationStructureSlot,
        Count
    };
}

//Should not be needed...
namespace LocalRootSignatureParams {
    enum Value
    {
        ViewportConstantSlot = 0,
        Count
    };
}

// A basic sample implementation that creates a D3D12 device and provides a render loop.
class Sample : public DX::IDeviceNotify
{
public:

    Sample() noexcept(false);
    ~Sample();

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
    std::unique_ptr<DirectX::Mouse>           m_mouse;

    DirectX::GamePad::ButtonStateTracker      m_gamePadButtons;
    DirectX::Keyboard::KeyboardStateTracker   m_keyboardButtons;


    /////////////////////////////////////////////////////////////////////////////////
    // START SAMPLE SPECIFIC
    
    void BuildSceneGeometry();

    // Geometry
    typedef UINT16 Index;
    struct Vertex { float v1, v2, v3; };

    Index indices[3]       = { 0, 1, 2 };
    const float depthValue = 1.0;
    const float offset     = 0.5f;
    // The sample raytraces in screen space coordinates.
    // Since DirectX screen space coordinates are right handed (i.e. Y axis points down).
    // Define the vertices in counter clockwise order ~ clockwise in left handed.
    Vertex vertices[3] = {{  0, -offset, depthValue }, { -offset, offset, depthValue }, {  offset, offset, depthValue }};

    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    
    // END SAMPLE SPECIFIC
    /////////////////////////////////////////////////////////////////////////////////


    /////////////////////////////////////////////////////////////////////////////////
    // START RAYTRACING SPECIFIC

    //Init
    bool CheckRaytracingSupported(IDXGIAdapter1* adapter);
    void CreateRaytracingRootSignatures();
    void SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, Microsoft::WRL::ComPtr<ID3D12RootSignature>* rootSig);
    void CreateRaytracingPipelineStateObject();
    void CreateRaytracingLocalRootSignatureSubobjects(CD3D12_STATE_OBJECT_DESC* raytracingPipeline);
    void CreateRaytracingDescriptorHeap();
    void BuildRaytracingAccelerationStructures();
    void BuildRaytracingShaderTables();
    void CreateRaytracingOutputResource();
    UINT AllocateRaytracingDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse = UINT_MAX);
    WRAPPED_GPU_POINTER CreateFallbackWrappedPointer(ID3D12Resource* resource, UINT bufferNumElements);

    //Runtime
    void DoRaytracing();
    void CopyRaytracingOutputToBackbuffer();


    // Root signatures
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_raytracingGlobalRootSignature;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_raytracingLocalRootSignature;

    // Descriptors
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_raytracingDescriptorHeap;
    UINT m_raytracingDescriptorsAllocated;
    UINT m_raytracingDescriptorSize;

    // Acceleration structure
    Microsoft::WRL::ComPtr<ID3D12Resource> m_accelerationStructure;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_bottomLevelAccelerationStructure;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_topLevelAccelerationStructure;
    WRAPPED_GPU_POINTER m_fallbackTopLevelAccelerationStructurePointer;

    // Raytracing output
    Microsoft::WRL::ComPtr<ID3D12Resource> m_raytracingOutput;
    D3D12_GPU_DESCRIPTOR_HANDLE m_raytracingOutputResourceUAVGpuDescriptor;
    UINT m_raytracingOutputResourceUAVDescriptorHeapIndex;

    // Shader tables
    static const wchar_t* c_hitGroupName;
    static const wchar_t* c_raygenShaderName;
    static const wchar_t* c_closestHitShaderName;
    static const wchar_t* c_missShaderName;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_missShaderTable;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_hitGroupShaderTable;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_rayGenShaderTable;

    // END RAYTRACING SPECIFIC
    /////////////////////////////////////////////////////////////////////////////////
};