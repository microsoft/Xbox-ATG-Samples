//--------------------------------------------------------------------------------------
// SimpleRaytracingTriangle_PC12.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimpleRaytracingTriangle_PC12.h"
#include "RayTracingHelper.h"
#include "CompiledShaders\SimpleRaytracing.hlsl.h"

extern void ExitSample();

using namespace DX;
using namespace DirectX;
using Microsoft::WRL::ComPtr;

const wchar_t* Sample::c_hitGroupName         = L"HitGroup";
const wchar_t* Sample::c_raygenShaderName     = L"RayGenerationShader";
const wchar_t* Sample::c_closestHitShaderName = L"ClosestHitShader";
const wchar_t* Sample::c_missShaderName       = L"MissShader";

Sample::Sample() noexcept(false)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
    m_deviceResources->RegisterDeviceNotify(this);
}

Sample::~Sample()
{
    if (m_deviceResources)
    {
        m_deviceResources->WaitForGpu();
    }

    // Force cleanup.
    OnDeviceLost();
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(HWND window, int width, int height)
{
    m_gamePad = std::make_unique<GamePad>();

    m_keyboard = std::make_unique<Keyboard>();

    m_mouse = std::make_unique<Mouse>();
    m_mouse->SetWindow(window);

    m_deviceResources->SetWindow(window, width, height);
    m_deviceResources->InitializeDXGIAdapter();

    if (!CheckRaytracingSupported(m_deviceResources->GetDXRAdapter()))
    {
        ThrowIfFalse(false, L"Sample requires DRX support.\n");
    }

    m_deviceResources->CreateDeviceResources();
    m_deviceResources->CreateRaytracingInterfaces();

    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();
}

#pragma region Frame Update
// Executes basic render loop.
void Sample::Tick()
{
    Update();
    Render();
}

// Updates the world.
void Sample::Update()
{
    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);

        if (pad.IsViewPressed())
        {
            ExitSample();
        }
    }
    else
    {
        m_gamePadButtons.Reset();
    }

    auto kb = m_keyboard->GetState();
    m_keyboardButtons.Update(kb);

    if (kb.Escape)
    {
        ExitSample();
    }

    auto mouse = m_mouse->GetState();

}
#pragma endregion

#pragma region Frame Render

// Draws the scene.
void Sample::Render()
{
    // Prepare the command list to render a new frame.
    m_deviceResources->Prepare();

    Clear();
    DoRaytracing();
    CopyRaytracingOutputToBackbuffer();
    m_deviceResources->Present(D3D12_RESOURCE_STATE_PRESENT);
}

// Helper method to clear the back buffers.
void Sample::Clear()
{
    auto commandList = m_deviceResources->GetCommandList();

    // Clear the views.
    auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
    auto dsvDescriptor = m_deviceResources->GetDepthStencilView();

    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
    commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Set the viewport and scissor rect.
    auto viewport = m_deviceResources->GetScreenViewport();
    auto scissorRect = m_deviceResources->GetScissorRect();
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Sample::OnActivated()
{
}

void Sample::OnDeactivated()
{
}

void Sample::OnSuspending()
{
}

void Sample::OnResuming()
{
    m_gamePadButtons.Reset();
    m_keyboardButtons.Reset();
}

void Sample::OnWindowMoved()
{
    auto r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}

void Sample::OnWindowSizeChanged(int width, int height)
{
    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;

    CreateWindowSizeDependentResources();
}

// Properties
void Sample::GetDefaultSize(int& width, int& height) const
{
    width = 1280;
    height = 720;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Sample::CreateDeviceDependentResources()
{
    // Create root signatures for the shaders.
    CreateRaytracingRootSignatures();

    // Create a raytracing pipeline state object which defines the binding of shaders, state and resources to be used during raytracing.
    CreateRaytracingPipelineStateObject();

    // Create a heap for descriptors.
    CreateRaytracingDescriptorHeap();

    // Build geometry to be used in the sample.
    BuildSceneGeometry();

    // Build raytracing acceleration structures from the generated geometry.
    BuildRaytracingAccelerationStructures();

    // Build shader tables, which define shaders and their local root arguments.
    BuildRaytracingShaderTables();

    // Create an output 2D texture to store the raytracing result to.
    CreateRaytracingOutputResource();
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
}

void Sample::OnDeviceLost()
{
}

void Sample::OnDeviceRestored()
{
    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}


#pragma endregion

#pragma region Raytracing

bool Sample::CheckRaytracingSupported(IDXGIAdapter1* adapter)
{
    m_deviceResources->m_isDxrNativelySupported = IsDirectXRaytracingSupported(adapter);

    if (!m_deviceResources->m_isDxrNativelySupported)
    {
        OutputDebugString(L"Warning: DirectX Raytracing is not natively supported by your GPU and driver.\n\n");
    }
    else
    {
        // Fallback Layer uses an experimental feature and needs to be enabled before creating a D3D12 device.
        bool isFallbackSupported = EnableComputeRaytracingFallback(adapter);

        if (!isFallbackSupported)
        {
            OutputDebugString(
                L"Warning: Could not enable Compute Raytracing Fallback (D3D12EnableExperimentalFeatures() failed).\n" \
                L"         Possible reasons: your OS is not in developer mode.\n\n");

            return false;
        }
    }
    return true;
}

void Sample::CreateRaytracingRootSignatures()
{
    // Global Root Signature, This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
    {
        CD3DX12_DESCRIPTOR_RANGE UAVDescriptor;
        UAVDescriptor.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
        CD3DX12_ROOT_PARAMETER rootParameters[GlobalRootSignatureParams::Count];
        rootParameters[GlobalRootSignatureParams::OutputViewSlot].InitAsDescriptorTable(1, &UAVDescriptor);
        rootParameters[GlobalRootSignatureParams::AccelerationStructureSlot].InitAsShaderResourceView(0);
        CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
        SerializeAndCreateRaytracingRootSignature(globalRootSignatureDesc, &m_raytracingGlobalRootSignature);
    }
}

void Sample::SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig)
{
    auto device = m_deviceResources->GetD3DDevice();
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> error;

    if (!m_deviceResources->m_isDxrNativelySupported)
    {
        ThrowIfFailed(m_deviceResources->m_fallbackDevice->D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), error ? static_cast<wchar_t*>(error->GetBufferPointer()) : nullptr);
        ThrowIfFailed(m_deviceResources->m_fallbackDevice->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig))));
    }
    else // DirectX Raytracing
    {
        ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), error ? static_cast<wchar_t*>(error->GetBufferPointer()) : nullptr);
        ThrowIfFailed(device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig))));
    }
}

// Create a raytracing pipeline state object (RTPSO).
// An RTPSO represents a full set of shaders reachable by a DispatchRays() call,
// with all configuration options resolved, such as local signatures and other state.
void Sample::CreateRaytracingPipelineStateObject()
{
    // Create 7 subobjects that combine into a RTPSO:
    // Subobjects need to be associated with DXIL exports (i.e. shaders) either by way of default or explicit associations.
    // Default association applies to every exported shader entrypoint that doesn't have any of the same type of subobject associated with it.
    // This simple sample utilizes default shader association except for local root signature subobject
    // which has an explicit association specified purely for demonstration purposes.
    // 1 - DXIL library
    // 1 - Triangle hit group
    // 1 - Shader config
    // 2 - Local root signature and association
    // 1 - Global root signature
    // 1 - Pipeline config
    CD3D12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };


    // DXIL library
    // This contains the shaders and their entrypoints for the state object.
    // Since shaders are not considered a subobject, they need to be passed in via DXIL library subobjects.
    auto lib = raytracingPipeline.CreateSubobject<CD3D12_DXIL_LIBRARY_SUBOBJECT>();
    D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE((void *)g_pSimpleRaytracing, ARRAYSIZE(g_pSimpleRaytracing));
    lib->SetDXILLibrary(&libdxil);
    // Define which shader exports to surface from the library.
    // If no shader exports are defined for a DXIL library subobject, all shaders will be surfaced.
    // In this sample, this could be omitted for convenience since the sample uses all shaders in the library. 
    {
        lib->DefineExport(c_raygenShaderName);
        lib->DefineExport(c_closestHitShaderName);
        lib->DefineExport(c_missShaderName);
    }

    // Triangle hit group
    // A hit group specifies closest hit, any hit and intersection shaders to be executed when a ray intersects the geometry's triangle/AABB.
    // In this sample, we only use triangle geometry with a closest hit shader, so others are not set.
    auto hitGroup = raytracingPipeline.CreateSubobject<CD3D12_HIT_GROUP_SUBOBJECT>();
    hitGroup->SetClosestHitShaderImport(c_closestHitShaderName);
    hitGroup->SetHitGroupExport(c_hitGroupName);
    hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

    // Shader config
    // Defines the maximum sizes in bytes for the ray payload and attribute structure.
    auto shaderConfig = raytracingPipeline.CreateSubobject<CD3D12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    UINT payloadSize = 4 * sizeof(float);   // float4 color
    UINT attributeSize = 2 * sizeof(float); // float2 barycentrics
    shaderConfig->Config(payloadSize, attributeSize);

    // Local root signature and shader association
    CreateRaytracingLocalRootSignatureSubobjects(&raytracingPipeline);
    // This is a root signature that enables a shader to have unique arguments that come from shader tables.

    // Global root signature
    // This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
    auto globalRootSignature = raytracingPipeline.CreateSubobject<CD3D12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    globalRootSignature->SetRootSignature(m_raytracingGlobalRootSignature.Get());

    // Pipeline config
    // Defines the maximum TraceRay() recursion depth.
    auto pipelineConfig = raytracingPipeline.CreateSubobject<CD3D12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
    // PERFOMANCE TIP: Set max recursion depth as low as needed 
    // as drivers may apply optimization strategies for low recursion depths. 
    UINT maxRecursionDepth = 1; // ~ primary rays only. 
    pipelineConfig->Config(maxRecursionDepth);

#if _DEBUG
    PrintStateObjectDesc(raytracingPipeline);
#endif

    // Create the state object.
    if (!m_deviceResources->m_isDxrNativelySupported)
    {
        ThrowIfFailed(m_deviceResources->m_fallbackDevice->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&m_deviceResources->m_fallbackStateObject)), L"Couldn't create DirectX Raytracing state object.\n");
    }
    else // DirectX Raytracing
    {
        ThrowIfFailed(m_deviceResources->m_dxrDevice->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&m_deviceResources->m_dxrStateObject)), L"Couldn't create DirectX Raytracing state object.\n");
    }
}

void Sample::CreateRaytracingDescriptorHeap()
{
    auto device = m_deviceResources->GetD3DDevice();

    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
    // Allocate a heap for 3 descriptors:
    // 2 - bottom and top level acceleration structure fallback wrapped pointers
    // 1 - raytracing output texture SRV
    descriptorHeapDesc.NumDescriptors = 3;
    descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    descriptorHeapDesc.NodeMask = 0;
    device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(m_raytracingDescriptorHeap.ReleaseAndGetAddressOf()));
    NAME_D3D12_OBJECT(m_raytracingDescriptorHeap);

    m_raytracingDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

// Build geometry used in the sample.
void Sample::BuildSceneGeometry()
{
    auto device = m_deviceResources->GetD3DDevice();

    AllocateUploadBuffer(device, vertices, sizeof(vertices), &m_vertexBuffer);
    AllocateUploadBuffer(device, indices, sizeof(indices), &m_indexBuffer);
}

// Local root signature and shader association
// This is a root signature that enables a shader to have unique arguments that come from shader tables.
void Sample::CreateRaytracingLocalRootSignatureSubobjects(CD3D12_STATE_OBJECT_DESC* raytracingPipeline)
{
    // Hit group and miss shaders in this sample are not using a local root signature and thus one is not associated with them.

    // Local root signature to be used in a ray gen shader.
    {
        auto localRootSignature = raytracingPipeline->CreateSubobject<CD3D12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
        localRootSignature->SetRootSignature(m_raytracingLocalRootSignature.Get());
        // Shader association
        auto rootSignatureAssociation = raytracingPipeline->CreateSubobject<CD3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
        rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
        rootSignatureAssociation->AddExport(c_raygenShaderName);
    }
}

// Build acceleration structures needed for raytracing.
void Sample::BuildRaytracingAccelerationStructures()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto commandList = m_deviceResources->GetCommandList();
    auto commandAllocator = m_deviceResources->GetCommandAllocator();

    // Reset the command list for the acceleration structure construction.
    commandList->Reset(commandAllocator, nullptr);

    D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
    geometryDesc.Type                                 = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometryDesc.Triangles.IndexBuffer                = m_indexBuffer->GetGPUVirtualAddress();
    geometryDesc.Triangles.IndexCount                 = static_cast<UINT>(m_indexBuffer->GetDesc().Width) / sizeof(Index);
    geometryDesc.Triangles.IndexFormat                = DXGI_FORMAT_R16_UINT;
    geometryDesc.Triangles.Transform3x4               = 0;
    geometryDesc.Triangles.VertexFormat               = DXGI_FORMAT_R32G32B32_FLOAT;
    geometryDesc.Triangles.VertexCount                = static_cast<UINT>(m_vertexBuffer->GetDesc().Width) / sizeof(Vertex);
    geometryDesc.Triangles.VertexBuffer.StartAddress  = m_vertexBuffer->GetGPUVirtualAddress();
    geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);

    // Mark the geometry as opaque. 
    // PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
    // Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
    geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    // Get required sizes for an acceleration structure.
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS topLevelInputs = {};
    topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    topLevelInputs.Flags = buildFlags;
    topLevelInputs.NumDescs = 1;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};

    topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    topLevelInputs.Flags = buildFlags;
    topLevelInputs.NumDescs = 1;
    topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    if (!m_deviceResources->m_isDxrNativelySupported)
    {
        m_deviceResources->m_fallbackDevice->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
    }
    else // DirectX Raytracing
    {
        m_deviceResources->m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
    }
    ThrowIfFalse(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bottomLevelInputs = topLevelInputs;
    bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    bottomLevelInputs.pGeometryDescs = &geometryDesc;
    if (!m_deviceResources->m_isDxrNativelySupported)
    {
        m_deviceResources->m_fallbackDevice->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
    }
    else // DirectX Raytracing
    {
        m_deviceResources->m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
    }
    ThrowIfFalse(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

    ComPtr<ID3D12Resource> scratchResource;
    AllocateUAVBuffer(device, std::max(topLevelPrebuildInfo.ScratchDataSizeInBytes, bottomLevelPrebuildInfo.ScratchDataSizeInBytes), &scratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");

    // Allocate resources for acceleration structures.
    // Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
    // Default heap is OK since the application doesn’t need CPU read/write access to them. 
    // The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
    // and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
    //  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
    //  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.
    {
        D3D12_RESOURCE_STATES initialResourceState;
        if (!m_deviceResources->m_isDxrNativelySupported)
        {
            initialResourceState = m_deviceResources->m_fallbackDevice->GetAccelerationStructureResourceState();
        }
        else // DirectX Raytracing
        {
            initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        }

        AllocateUAVBuffer(device, bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &m_bottomLevelAccelerationStructure, initialResourceState, L"BottomLevelAccelerationStructure");
        AllocateUAVBuffer(device, topLevelPrebuildInfo.ResultDataMaxSizeInBytes, &m_topLevelAccelerationStructure, initialResourceState, L"TopLevelAccelerationStructure");
    }

    // Note on Emulated GPU pointers (AKA Wrapped pointers) requirement in Fallback Layer:
    // The primary point of divergence between the DXR API and the compute-based Fallback layer is the handling of GPU pointers. 
    // DXR fundamentally requires that GPUs be able to dynamically read from arbitrary addresses in GPU memory. 
    // The existing Direct Compute API today is more rigid than DXR and requires apps to explicitly inform the GPU what blocks of memory it will access with SRVs/UAVs.
    // In order to handle the requirements of DXR, the Fallback Layer uses the concept of Emulated GPU pointers, 
    // which requires apps to create views around all memory they will access for raytracing, 
    // but retains the DXR-like flexibility of only needing to bind the top level acceleration structure at DispatchRays.
    //
    // The Fallback Layer interface uses WRAPPED_GPU_POINTER to encapsulate the underlying pointer
    // which will either be an emulated GPU pointer for the compute - based path or a GPU_VIRTUAL_ADDRESS for the DXR path.

    // Create an instance desc for the bottom-level acceleration structure.
    ComPtr<ID3D12Resource> instanceDescs;
    if (!m_deviceResources->m_isDxrNativelySupported)
    {
        D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC instanceDesc = {};
        instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1;
        instanceDesc.InstanceMask = 1;
        UINT numBufferElements = static_cast<UINT>(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes) / sizeof(UINT32);
        instanceDesc.AccelerationStructure = CreateFallbackWrappedPointer(m_bottomLevelAccelerationStructure.Get(), numBufferElements);
        AllocateUploadBuffer(device, &instanceDesc, sizeof(instanceDesc), &instanceDescs, L"InstanceDescs");
    }
    else // DirectX Raytracing
    {
        D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
        instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1;
        instanceDesc.InstanceMask = 1;
        instanceDesc.AccelerationStructure = m_bottomLevelAccelerationStructure->GetGPUVirtualAddress();
        AllocateUploadBuffer(device, &instanceDesc, sizeof(instanceDesc), &instanceDescs, L"InstanceDescs");
    }

    // Create a wrapped pointer to the acceleration structure.
    if (!m_deviceResources->m_isDxrNativelySupported)
    {
        UINT numBufferElements = static_cast<UINT>(topLevelPrebuildInfo.ResultDataMaxSizeInBytes) / sizeof(UINT32);
        m_fallbackTopLevelAccelerationStructurePointer = CreateFallbackWrappedPointer(m_topLevelAccelerationStructure.Get(), numBufferElements);
    }

    // Bottom Level Acceleration Structure desc
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
    {
        bottomLevelBuildDesc.Inputs = bottomLevelInputs;
        bottomLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
        bottomLevelBuildDesc.DestAccelerationStructureData = m_bottomLevelAccelerationStructure->GetGPUVirtualAddress();
    }

    // Top Level Acceleration Structure desc
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = bottomLevelBuildDesc;
    {
        topLevelInputs.InstanceDescs = instanceDescs->GetGPUVirtualAddress();
        topLevelBuildDesc.Inputs = topLevelInputs;
        topLevelBuildDesc.DestAccelerationStructureData = m_topLevelAccelerationStructure->GetGPUVirtualAddress();
        topLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
    }

    // Build acceleration structure.
    if (!m_deviceResources->m_isDxrNativelySupported)
    {
        // Set the descriptor heaps to be used during acceleration structure build for the Fallback Layer.
        ID3D12DescriptorHeap *pDescriptorHeaps[] = { m_raytracingDescriptorHeap.Get() };
        m_deviceResources->m_fallbackCommandList->SetDescriptorHeaps(ARRAYSIZE(pDescriptorHeaps), pDescriptorHeaps);

        m_deviceResources->m_fallbackCommandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);

        CD3DX12_RESOURCE_BARRIER bottomBarrier = CD3DX12_RESOURCE_BARRIER::UAV(m_bottomLevelAccelerationStructure.Get());
        commandList->ResourceBarrier(1, &bottomBarrier);
        m_deviceResources->m_fallbackCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
    }
    else // DirectX Raytracing
    {
        m_deviceResources->m_dxrCommandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);

        CD3DX12_RESOURCE_BARRIER bottomBarrier = CD3DX12_RESOURCE_BARRIER::UAV(m_bottomLevelAccelerationStructure.Get());
        commandList->ResourceBarrier(1, &bottomBarrier);
        m_deviceResources->m_dxrCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
    }

    // Kick off acceleration structure construction.
    m_deviceResources->ExecuteCommandList();

    // Wait for GPU to finish as the locally created temporary GPU resources will get released once we go out of scope.
    m_deviceResources->WaitForGpu();
}

// Build shader tables.
// This encapsulates all shader records - shaders and the arguments for their local root signatures.
void Sample::BuildRaytracingShaderTables()
{
    auto device = m_deviceResources->GetD3DDevice();

    void* rayGenShaderIdentifier;
    void* missShaderIdentifier;
    void* hitGroupShaderIdentifier;

    auto GetShaderIdentifiers = [&](auto* stateObjectProperties)
    {
        rayGenShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_raygenShaderName);
        missShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_missShaderName);
        hitGroupShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_hitGroupName);
    };

    // Get shader identifiers.
    UINT shaderIdentifierSize;
    if (!m_deviceResources->m_isDxrNativelySupported)
    {
        GetShaderIdentifiers(m_deviceResources->m_fallbackStateObject.Get());
        shaderIdentifierSize = m_deviceResources->m_fallbackDevice->GetShaderIdentifierSize();
    }
    else // DirectX Raytracing
    {
        ComPtr<ID3D12StateObjectPropertiesPrototype> stateObjectProperties;
        ThrowIfFailed(m_deviceResources->m_dxrStateObject.As(&stateObjectProperties));
        GetShaderIdentifiers(stateObjectProperties.Get());
        shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    }

    // Ray gen shader table
    {
        UINT numShaderRecords = 1;
        UINT shaderRecordSize = shaderIdentifierSize;
        ShaderTable rayGenShaderTable(device, numShaderRecords, shaderRecordSize, L"RayGenShaderTable");
        rayGenShaderTable.Add(ShaderRecord(rayGenShaderIdentifier, shaderIdentifierSize));
        m_rayGenShaderTable = rayGenShaderTable.GetResource();
    }

    // Miss shader table
    {
        UINT numShaderRecords = 1;
        UINT shaderRecordSize = shaderIdentifierSize;
        ShaderTable missShaderTable(device, numShaderRecords, shaderRecordSize, L"MissShaderTable");
        missShaderTable.Add(ShaderRecord(missShaderIdentifier, shaderIdentifierSize));
        m_missShaderTable = missShaderTable.GetResource();
    }

    // Hit group shader table
    {
        UINT numShaderRecords = 1;
        UINT shaderRecordSize = shaderIdentifierSize;
        ShaderTable hitGroupShaderTable(device, numShaderRecords, shaderRecordSize, L"HitGroupShaderTable");
        hitGroupShaderTable.Add(ShaderRecord(hitGroupShaderIdentifier, shaderIdentifierSize));
        m_hitGroupShaderTable = hitGroupShaderTable.GetResource();
    }
}

/// Create 2D output texture for raytracing.
void Sample::CreateRaytracingOutputResource()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto backbufferFormat = m_deviceResources->GetBackBufferFormat();

    // Create the output resource. The dimensions and format should match the swap-chain.
    auto uavDesc = CD3DX12_RESOURCE_DESC::Tex2D(backbufferFormat, m_deviceResources->GetScreenWidth(), m_deviceResources->GetScreenHeight(), 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(device->CreateCommittedResource(
        &defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&m_raytracingOutput)));
    NAME_D3D12_OBJECT(m_raytracingOutput);

    D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle;
    m_raytracingOutputResourceUAVDescriptorHeapIndex = AllocateRaytracingDescriptor(&uavDescriptorHandle, m_raytracingOutputResourceUAVDescriptorHeapIndex);
    D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
    UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    device->CreateUnorderedAccessView(m_raytracingOutput.Get(), nullptr, &UAVDesc, uavDescriptorHandle);
    m_raytracingOutputResourceUAVGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_raytracingDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), m_raytracingOutputResourceUAVDescriptorHeapIndex, m_raytracingDescriptorSize);
}

// Allocate a descriptor and return its index. 
// If the passed descriptorIndexToUse is valid, it will be used instead of allocating a new one.
UINT Sample::AllocateRaytracingDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse)
{
    auto descriptorHeapCpuBase = m_raytracingDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    if (descriptorIndexToUse >= m_raytracingDescriptorHeap->GetDesc().NumDescriptors)
    {
        descriptorIndexToUse = m_raytracingDescriptorsAllocated++;
    }
    *cpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeapCpuBase, descriptorIndexToUse, m_raytracingDescriptorSize);
    return descriptorIndexToUse;
}

// Create a wrapped pointer for the Fallback Layer path.
WRAPPED_GPU_POINTER Sample::CreateFallbackWrappedPointer(ID3D12Resource* resource, UINT bufferNumElements)
{
    auto device = m_deviceResources->GetD3DDevice();

    D3D12_UNORDERED_ACCESS_VIEW_DESC rawBufferUavDesc = {};
    rawBufferUavDesc.ViewDimension      = D3D12_UAV_DIMENSION_BUFFER;
    rawBufferUavDesc.Buffer.Flags       = D3D12_BUFFER_UAV_FLAG_RAW;
    rawBufferUavDesc.Format             = DXGI_FORMAT_R32_TYPELESS;
    rawBufferUavDesc.Buffer.NumElements = bufferNumElements;

    D3D12_CPU_DESCRIPTOR_HANDLE bottomLevelDescriptor;

    // Only compute fallback requires a valid descriptor index when creating a wrapped pointer.
    UINT descriptorHeapIndex = 0;
    if (!m_deviceResources->m_fallbackDevice->UsingRaytracingDriver())
    {
        descriptorHeapIndex = AllocateRaytracingDescriptor(&bottomLevelDescriptor);
        device->CreateUnorderedAccessView(resource, nullptr, &rawBufferUavDesc, bottomLevelDescriptor);
    }
    return m_deviceResources->m_fallbackDevice->GetWrappedPointerSimple(descriptorHeapIndex, resource->GetGPUVirtualAddress());
}

void Sample::DoRaytracing()
{
    auto commandList = m_deviceResources->GetCommandList();

    auto DispatchRays = [&](auto* commandList, auto* stateObject, auto* dispatchDesc)
    {
        // Since each shader table has only one shader record, the stride is same as the size.
        dispatchDesc->Depth                                  = 1;
        dispatchDesc->Width                                  = m_deviceResources->GetScreenWidth();
        dispatchDesc->Height                                 = m_deviceResources->GetScreenHeight();
        dispatchDesc->HitGroupTable.StartAddress             = m_hitGroupShaderTable->GetGPUVirtualAddress();
        dispatchDesc->HitGroupTable.SizeInBytes              = m_hitGroupShaderTable->GetDesc().Width;
        dispatchDesc->HitGroupTable.StrideInBytes            = dispatchDesc->HitGroupTable.SizeInBytes;
        dispatchDesc->MissShaderTable.StartAddress           = m_missShaderTable->GetGPUVirtualAddress();
        dispatchDesc->MissShaderTable.SizeInBytes            = m_missShaderTable->GetDesc().Width;
        dispatchDesc->MissShaderTable.StrideInBytes          = dispatchDesc->MissShaderTable.SizeInBytes;
        dispatchDesc->RayGenerationShaderRecord.StartAddress = m_rayGenShaderTable->GetGPUVirtualAddress();
        dispatchDesc->RayGenerationShaderRecord.SizeInBytes  = m_rayGenShaderTable->GetDesc().Width;
        commandList->SetPipelineState1(stateObject);
        commandList->DispatchRays(dispatchDesc);
    };

    commandList->SetComputeRootSignature(m_raytracingGlobalRootSignature.Get());

    // Bind the heaps, acceleration structure and dispatch rays.    
    D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
    if (!m_deviceResources->m_isDxrNativelySupported)
    {
        m_deviceResources->m_fallbackCommandList->SetDescriptorHeaps(1, m_raytracingDescriptorHeap.GetAddressOf());
        commandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::OutputViewSlot, m_raytracingOutputResourceUAVGpuDescriptor);
        m_deviceResources->m_fallbackCommandList->SetTopLevelAccelerationStructure(GlobalRootSignatureParams::AccelerationStructureSlot, m_fallbackTopLevelAccelerationStructurePointer);
        DispatchRays(m_deviceResources->m_fallbackCommandList.Get(), m_deviceResources->m_fallbackStateObject.Get(), &dispatchDesc);
    }
    else // DirectX Raytracing
    {
        commandList->SetDescriptorHeaps(1, m_raytracingDescriptorHeap.GetAddressOf());
        commandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::OutputViewSlot, m_raytracingOutputResourceUAVGpuDescriptor);
        commandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams::AccelerationStructureSlot, m_topLevelAccelerationStructure->GetGPUVirtualAddress());
        DispatchRays(m_deviceResources->m_dxrCommandList.Get(), m_deviceResources->m_dxrStateObject.Get(), &dispatchDesc);
    }
}

// Copy the raytracing output to the backbuffer.
void Sample::CopyRaytracingOutputToBackbuffer()
{
    auto commandList = m_deviceResources->GetCommandList();
    auto renderTarget = m_deviceResources->GetRenderTarget();

    D3D12_RESOURCE_BARRIER preCopyBarriers[2];
    preCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
    preCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_raytracingOutput.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    commandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);

    commandList->CopyResource(renderTarget, m_raytracingOutput.Get());

    D3D12_RESOURCE_BARRIER postCopyBarriers[2];
    postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
    postCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_raytracingOutput.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    commandList->ResourceBarrier(ARRAYSIZE(postCopyBarriers), postCopyBarriers);
}

#pragma endregion
