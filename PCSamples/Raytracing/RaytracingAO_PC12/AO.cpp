//--------------------------------------------------------------------------------------
// D3D12RaytracingAO.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "AO.h"
#include "CompiledShaders\AORaytracing.hlsl.h"
#include "CosineHemiSampler.h"
#include "UniformHemiSampler.h"
#include "UniformSampler.h"
#include "StratifiedSampler.h"
#include "RayTracingHelper.h"

using namespace DirectX;

using Microsoft::WRL::ComPtr;

const wchar_t* AO::c_hitGroupNames[] = {
    L"AOHitGroup",
    L"AOBounceHitGroup"
};
const wchar_t* AO::c_raygenShaderName = L"AORaygenShader";
const wchar_t* AO::c_closestHitShaderNames[] =
{
    L"AOClosestHitShader",
    L"AOBounceClosestHitShader"
};
const wchar_t* AO::c_missShaderNames[] =
{
    L"AOMissShader",
    L"AOBounceMissShader"
};

// Setup the root signatures for the shaders.
void AO::CreateRootSignatures()
{
    auto device = m_deviceResources->GetD3DDevice();

    // Global Root Signature
    // This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
    {
        CD3DX12_DESCRIPTOR_RANGE ranges[2]; // Perfomance TIP: Order from most frequent to least frequent.
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV,     1, 0);  // 1 output texture.
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 2, 0);  // point wrap and linear clamp samplers.

        CD3DX12_ROOT_PARAMETER rootParameters[AOGlobalRootSig::GlobalCount];
        rootParameters[AOGlobalRootSig::GlobalOutputViewSlot    ].InitAsDescriptorTable(1, &ranges[0]);
        rootParameters[AOGlobalRootSig::GlobalAccelStructSlot   ].InitAsShaderResourceView(0);
        rootParameters[AOGlobalRootSig::GlobalSceneConstSlot    ].InitAsConstantBufferView(0);
        rootParameters[AOGlobalRootSig::GlobalAOConstSlot       ].InitAsConstantBufferView(1);
        rootParameters[AOGlobalRootSig::GlobalAOOptionsConstSlot].InitAsConstantBufferView(2);
        rootParameters[AOGlobalRootSig::GlobalSamplerSlot       ].InitAsDescriptorTable(1, &ranges[1]);

        CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(_countof(rootParameters), rootParameters);

        {
            ComPtr<ID3DBlob> blob;
            ComPtr<ID3DBlob> error;
            DX::ThrowIfFailed(D3D12SerializeRootSignature(&globalRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error));
            DX::ThrowIfFailed(device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(m_raytracingGlobalRootSignature))));
        }
    }

    // Local Root Signature
    // This is a root signature that enables a shader to have unique arguments that come from shader tables.
    {
        CD3DX12_DESCRIPTOR_RANGE ranges[1]; // Perfomance TIP: Order from most frequent to least frequent.

        // indices, vertices, diffuse texture, specular texture, and normal map.
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, AOPerObjectCSUDesc::CSUCount, 0, SPACE_LOCAL);

        CD3DX12_ROOT_PARAMETER rootParameters[AOLocalRootSig::LocalCount];
        rootParameters[AOLocalRootSig::LocalSRVBufferSlot].InitAsDescriptorTable(1, &ranges[0]);
        rootParameters[AOLocalRootSig::LocalMeshConstSlot].InitAsConstants(RoundUp32(sizeof(MaterialConstantBuffer)), 0, SPACE_LOCAL);

        CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(_countof(rootParameters), rootParameters);
        localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

        {
            ComPtr<ID3DBlob> blob;
            ComPtr<ID3DBlob> error;

            DX::ThrowIfFailed(D3D12SerializeRootSignature(&localRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error));
            DX::ThrowIfFailed(device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(m_raytracingLocalRootSignature))));
        }
    }
#ifdef USE_NON_NULL_LOCAL_ROOT_SIG
    // Empty local root signature
    {
        CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(D3D12_DEFAULT);
        localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
        
        {
            ComPtr<ID3DBlob> blob;
            ComPtr<ID3DBlob> error;

            DX::ThrowIfFailed(D3D12SerializeRootSignature(&localRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error));
            DX::ThrowIfFailed(device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(m_raytracingLocalRootSignatureEmpty))));
        }
    }
#endif
}

// Local root signature and shader association
// This is a root signature that enables a shader to have unique arguments that come from shader tables.
void AO::CreateLocalRootSignatureSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline)
{
    // Local root signature to be used in a ray gen shader.
    {
        auto localRootSignature = raytracingPipeline->CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
        localRootSignature->SetRootSignature(m_raytracingLocalRootSignature.Get());

        // Shader association
        auto rootSignatureAssociation = raytracingPipeline->CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
        rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
        rootSignatureAssociation->AddExports(c_hitGroupNames);
    }
#ifdef USE_NON_NULL_LOCAL_ROOT_SIG
    // Empty local root signature to be used in a miss shader and a hit group.
    {
        auto localRootSignature = raytracingPipeline->CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
        localRootSignature->SetRootSignature(m_raytracingLocalRootSignatureEmpty.Get());
        // Shader association
        auto rootSignatureAssociation = raytracingPipeline->CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
        rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
        rootSignatureAssociation->AddExport(c_raygenShaderName);
        rootSignatureAssociation->AddExports(c_missShaderNames);
    }
#endif
}

// Create a raytracing pipeline state object (RTPSO).
// An RTPSO represents a full set of shaders reachable by a DispatchRays() call,
// with all configuration options resolved, such as local signatures and other state.
void AO::CreateRaytracingPipelineStateObject()
{
    auto device = m_deviceResources->GetD3DDevice();

    // Create 8 subobjects that combine into a RTPSO:
    // Subobjects need to be associated with DXIL exports (i.e. shaders) either by way of default or explicit associations.
    // Default association applies to every exported shader entrypoint that doesn't have any of the same type of subobject associated with it.
    // This simple sample utilizes default shader association except for local root signature subobject
    // which has an explicit association specified purely for demonstration purposes.
    // 1 - DXIL library
    // 2 - Triangle hit group
    // 1 - Shader config
    // 2 - Local root signature and association
    // 1 - Global root signature
    // 1 - Pipeline config
    CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

    // DXIL library
    // This contains the shaders and their entrypoints for the state object.
    // Since shaders are not considered a subobject, they need to be passed in via DXIL library subobjects.
    auto lib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
    D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE((void *)g_pAORaytracing, sizeof(g_pAORaytracing));
    lib->SetDXILLibrary(&libdxil);

    // Define which shader exports to surface from the library.
    // If no shader exports are defined for a DXIL library subobject, all shaders will be surfaced.
    // In this sample, this could be ommited for convenience since the sample uses all shaders in the library. 
    {
        lib->DefineExport(c_raygenShaderName);
        lib->DefineExports(c_closestHitShaderNames);
        lib->DefineExports(c_missShaderNames);
    }

    // Triangle hit group
    // A hit group specifies closest hit, any hit and intersection shaders to be executed when a ray intersects the geometry's triangle/AABB.
    // In this sample, we only use triangle geometry with a closest hit shader, so others are not set.
    for (unsigned int i = 0; i < TraceRayParameters::HitGroup::Count; i++)
    {
        auto hitGroup = raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
        hitGroup->SetClosestHitShaderImport(c_closestHitShaderNames[i]);
        hitGroup->SetHitGroupExport(c_hitGroupNames[i]);
    }

    // Shader config
    // Defines the maximum sizes in bytes for the ray payload and attribute structure.
    auto shaderConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    unsigned int payloadSize = sizeof(XMFLOAT4);    // float4 pixelColor
    unsigned int attributeSize = sizeof(XMFLOAT2);  // float2 barycentrics
    shaderConfig->Config(payloadSize, attributeSize);

    // Local root signature and shader association
    // This is a root signature that enables a shader to have unique arguments that come from shader tables.
    CreateLocalRootSignatureSubobjects(&raytracingPipeline);

    // Global root signature
    // This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.

    auto globalRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    globalRootSignature->SetRootSignature(m_raytracingGlobalRootSignature.Get());

    // Pipeline config
    // Defines the maximum TraceRay() recursion depth.
    auto pipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
    // PERFOMANCE TIP: Set max recursion depth as low as needed 
    // as drivers may apply optimization strategies for low recursion depths.
    unsigned int maxRecursionDepth = 2; // ~ primary rays and first bounce AO rays. 
    pipelineConfig->Config(maxRecursionDepth);

#if _DEBUG
    PrintStateObjectDesc(raytracingPipeline);
#endif

    DX::ThrowIfFailed(device->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&m_dxrStateObject)));
}

// Setup descriptor heaps.
void AO::CreateDescriptorHeaps()
{
    auto device = m_deviceResources->GetD3DDevice();

    // Allocate a csu heap.
    {
        const uint32_t c_csuCount = AOCSUDesc::CSUCount;
        m_csuDescriptors = std::make_unique<DescriptorHeap>(
            device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, c_csuCount);
    }

    // Allocate a sampler heap.
    {
        const uint32_t c_samplerCount = AOSamplerDesc::SamplerCount;
        m_samplerDescriptors = std::make_unique<DescriptorHeap>(
            device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, c_samplerCount);
    }

    // Set sampler heap values.
    {
        CreateTexture2DSampler(
            device,
            m_samplerDescriptors->GetCpuHandle(AOSamplerDesc::SamplerPointWrap),
            D3D12_FILTER_MIN_MAG_MIP_POINT,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP
        );

        CreateTexture2DSampler(
            device,
            m_samplerDescriptors->GetCpuHandle(AOSamplerDesc::SamplerLinearClamp),
            D3D12_FILTER_MIN_MAG_MIP_LINEAR,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP
        );
    }
}

// Build acceleration structures needed for raytracing.
void AO::BuildAccelerationStructures()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto commandList = m_deviceResources->GetCommandList();
    auto commandAllocator = m_deviceResources->GetCommandAllocator();

    // Reset the command list for the acceleration structure construction.
    commandList->Reset(commandAllocator, nullptr);

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Acceleration Structure");

    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescArr;
    // Create a descriptor for all geometry in the scene.
    {
        // Setup format for acceleration structure construction. 
        D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
        geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
        geometryDesc.Triangles.Transform3x4 = 0;
        geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);

        // Mark the geometry as opaque. 
        // PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
        // Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
        geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

        for (auto el : *m_mesh)
        {
            geometryDesc.Triangles.IndexCount = el.numIndices;
            geometryDesc.Triangles.IndexBuffer = el.indexResource->GetGPUVirtualAddress();
            geometryDesc.Triangles.VertexCount = el.numVertices;
            geometryDesc.Triangles.VertexBuffer.StartAddress = el.vertexResource->GetGPUVirtualAddress();

            geometryDescArr.push_back(geometryDesc);
        }
    }

    // Get required sizes for an acceleration structure.
    // For the purposes of the demo, a tree that has fast ray tracing at the cost of construction time is built.
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& bottomLevelInputs = bottomLevelBuildDesc.Inputs;
    bottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    bottomLevelInputs.Flags = buildFlags;
    bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    bottomLevelInputs.NumDescs = static_cast<unsigned int>(geometryDescArr.size());
    bottomLevelInputs.pGeometryDescs = geometryDescArr.data();

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
    {
        device->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
    }
    assert(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);


    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = bottomLevelBuildDesc;
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& topLevelInputs = topLevelBuildDesc.Inputs;
    topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    topLevelInputs.Flags = buildFlags;
    topLevelInputs.NumDescs = 1;
    topLevelInputs.pGeometryDescs = nullptr;
    topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
    {
        device->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
    }
    assert(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);


    // Allocate buffer to be used during acceleration structure construction.
    ComPtr<ID3D12Resource> scratchResource;
    {
        AllocateUAVBuffer(device,
            std::max(topLevelPrebuildInfo.ScratchDataSizeInBytes,
                bottomLevelPrebuildInfo.ScratchDataSizeInBytes),
            &scratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            L"ScratchResource");
    }

    // Allocate resources for acceleration structures.
    // Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
    // Default heap is OK since the application doesn’t need CPU read/write access to them. 
    // The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
    // and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
    //  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
    //  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.
    AllocateUAVBuffer(device, bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &m_bottomLevelAccelerationStructure, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, L"BottomLevelAccelerationStructure");
    AllocateUAVBuffer(device, topLevelPrebuildInfo.ResultDataMaxSizeInBytes, &m_topLevelAccelerationStructure, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, L"TopLevelAccelerationStructure");

    // Note on Emulated GPU pointers (AKA Wrapped pointers) requirement in Fallback Layer:
    // The primary point of divergence between the DXR API and the compute-based Fallback layer is the handling of GPU pointers. 
    // DXR fundamentally requires that GPUs be able to dynamically read from arbitrary addresses in GPU memory. 
    // The existing Direct Compute API today is more rigid than DXR and requires apps to explicitly inform the GPU what blocks of memory it will access with SRVs/UAVs.
    // In order to handle the requirements of DXR, the Fallback Layer uses the concept of Emulated GPU pointers, 
    // which requires apps to create views around all memory they will access for raytracing, 
    // but retains the DXR-like flexibility of only needing to bind the top level acceleration structure at DispatchRays.
    //
    // The Fallback Layer interface uses WRAPPED_GPU_POintER to encapsulate the underlying pointer
    // which will either be an emulated GPU pointer for the compute - based path or a GPU_VIRTUAL_ADDRESS for the DXR path.

    // Create an instance desc for the bottom-level acceleration structure.
    ComPtr<ID3D12Resource> instanceDescs;

    {
        D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
        instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1;
        instanceDesc.InstanceMask = 1;
        instanceDesc.AccelerationStructure = m_bottomLevelAccelerationStructure->GetGPUVirtualAddress();

        AllocateUploadBuffer(device, &instanceDesc, sizeof(instanceDesc), &instanceDescs, L"InstanceDescs");
    }

    // Bottom Level Acceleration Structure desc
    bottomLevelBuildDesc.ScratchAccelerationStructureData = { scratchResource->GetGPUVirtualAddress() };
    bottomLevelBuildDesc.DestAccelerationStructureData = { m_bottomLevelAccelerationStructure->GetGPUVirtualAddress() };

    // Top Level Acceleration Structure desc
    topLevelBuildDesc.DestAccelerationStructureData = { m_topLevelAccelerationStructure->GetGPUVirtualAddress() };
    topLevelBuildDesc.Inputs.InstanceDescs = instanceDescs->GetGPUVirtualAddress();
    topLevelBuildDesc.ScratchAccelerationStructureData = { scratchResource->GetGPUVirtualAddress() };

    // Build the acceleration structure.
    {
        CD3DX12_RESOURCE_BARRIER bottomBarrier = CD3DX12_RESOURCE_BARRIER::UAV(m_bottomLevelAccelerationStructure.Get());

        commandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
        commandList->ResourceBarrier(1, &bottomBarrier);
        commandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
    }

    // End commandlist.
    PIXEndEvent(commandList);

    // Kick off acceleration structure construction.
    DX::ThrowIfFailed(commandList->Close());
    m_deviceResources->GetCommandQueue()->ExecuteCommandLists(1, CommandListCast(&commandList));

    // Wait for GPU to finish as the locally created temporary GPU resources will get released once we go out of scope.
    m_deviceResources->WaitForGpu();
}

// Create constant buffers.
void AO::CreateConstantBuffers()
{
    auto device = m_deviceResources->GetD3DDevice();

    // Create a constant buffer for AO.
    {
        // Fill AO const buffer with samples.
        AlignedAOConstantBuffer aoConstBuffer = {};

        AllocateUploadBuffer(
            device,
            &aoConstBuffer,
            sizeof(aoConstBuffer),
            &m_mappedAOConstantResource);
    }

    // Create Options constant buffer.
    {
        AlignedAOOptionsConstantBuffer aoOptionsConstBuffer = {};

        AllocateUploadBuffer(
            device,
            &aoOptionsConstBuffer,
            sizeof(aoOptionsConstBuffer),
            &m_mappedAOOptionsConstantResource);

        m_mappedAOOptionsConstantResource->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedAOOptionsConstantData));
    }
}

// Build shader tables.
// This encapsulates all shader records - shaders and the arguments for their local root signatures.
void AO::BuildShaderTables()
{
    auto device = m_deviceResources->GetD3DDevice();

    void* rayGenShaderIdentifier;
    void* missShaderIdentifiers[TraceRayParameters::MissShader::Count];
    void* hitGroupShaderIdentifiers[TraceRayParameters::MissShader::Count];

    unsigned int shaderIdentifierSize;
    // Record shader information.
    {
        ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
        DX::ThrowIfFailed(m_dxrStateObject.As(&stateObjectProperties));

        rayGenShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_raygenShaderName);

        for (unsigned int i = 0; i < TraceRayParameters::MissShader::Count; i++)
            missShaderIdentifiers[i] = stateObjectProperties->GetShaderIdentifier(c_missShaderNames[i]);

        for (unsigned int i = 0; i < TraceRayParameters::HitGroup::Count; i++)
            hitGroupShaderIdentifiers[i] = stateObjectProperties->GetShaderIdentifier(c_hitGroupNames[i]);

        shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    }

    // Ray gen shader table
    {
        unsigned int numShaderRecords = 1;
        unsigned int shaderRecordSize = shaderIdentifierSize;

        ShaderTable rayGenShaderTable(device, numShaderRecords, shaderRecordSize, L"RayGenShaderTable");
        rayGenShaderTable.Add(ShaderRecord(rayGenShaderIdentifier, shaderIdentifierSize));
        rayGenShaderTable.Close();

        m_rayGenShaderTable = rayGenShaderTable.GetResource();
    }

    // Miss shader table
    {
        unsigned int numShaderRecords = TraceRayParameters::MissShader::Count;
        unsigned int shaderRecordSize = shaderIdentifierSize;

        ShaderTable missShaderTable(device, numShaderRecords, shaderRecordSize, L"MissShaderTable");

        for (unsigned int i = 0; i < numShaderRecords; i++)
            missShaderTable.Add(ShaderRecord(missShaderIdentifiers[i], shaderIdentifierSize));

        missShaderTable.Close();

        m_missRecordSize = missShaderTable.GetShaderRecordSize();
        m_missShaderTable = missShaderTable.GetResource();
    }

    // Hit group shader table
    {
        unsigned int numShaderRecords = unsigned int(TraceRayParameters::HitGroup::Count * m_mesh->size());

        unsigned int descriptorOffset = Align(shaderIdentifierSize, sizeof(D3D12_GPU_DESCRIPTOR_HANDLE));
        unsigned int meshConstantOffset = Align(descriptorOffset + sizeof(D3D12_GPU_DESCRIPTOR_HANDLE), sizeof(uint32_t));
        unsigned int shaderRecordSize = meshConstantOffset + sizeof(MaterialConstantBuffer);

        ShaderTable hitGroupShaderTable(device, numShaderRecords, shaderRecordSize, L"HitGroupShaderTable");

        size_t offset = 0;
        std::vector<byte> rootArgs(shaderRecordSize - shaderIdentifierSize);

        for (auto el : *m_mesh)
        {
            // Copy descriptor location.
            D3D12_GPU_DESCRIPTOR_HANDLE csuHandle = m_csuDescriptors->GetGpuHandle(AOCSUDesc::SRVPerObjectStart + offset);
            memcpy(rootArgs.data() + descriptorOffset - shaderIdentifierSize, &csuHandle, sizeof(D3D12_GPU_DESCRIPTOR_HANDLE));

            offset += AOPerObjectCSUDesc::CSUCount;

            // Copy over constant buffer.
            auto& material = m_mesh->getModel()->materials[el.matID];

            MaterialConstantBuffer cb;
            cb.ambient = material.ambientColor;
            cb.diffuse = material.diffuseColor;
            cb.specular = material.specularColor;
            cb.isDiffuseTexture = (material.diffuseTextureIndex != -1);
            cb.isSpecularTexture = (material.specularTextureIndex != -1);
            cb.isNormalTexture = (material.normalTextureIndex != -1);

            memcpy(
                rootArgs.data() + meshConstantOffset - shaderIdentifierSize,
                &cb,
                sizeof(MaterialConstantBuffer));

            for (unsigned int i = 0; i < TraceRayParameters::HitGroup::Count; i++)
            {
                hitGroupShaderTable.Add(
                    ShaderRecord(
                        hitGroupShaderIdentifiers[i],
                        shaderIdentifierSize,
                        rootArgs.data(),
                        unsigned int(rootArgs.size())
                    )
                );
            }
        }

        hitGroupShaderTable.Close();

        m_hitGroupRecordSize = hitGroupShaderTable.GetShaderRecordSize();
        m_hitGroupShaderTable = hitGroupShaderTable.GetResource();
    }
}

// Create 2D output texture for raytracing.
void AO::CreateRaytracingOutputResource()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto backbufferFormat = m_deviceResources->GetBackBufferFormat();

    auto output = m_deviceResources->GetOutputSize();
    auto screenWidth = static_cast<UINT>(float(output.right - output.left) * m_screenWidthScale);
    auto screenHeight = static_cast<UINT>(output.bottom - output.top);

    // Create the output resource. The dimensions and format should match the swap-chain.
    auto uavDesc = CD3DX12_RESOURCE_DESC::Tex2D(backbufferFormat, (UINT64)screenWidth, screenHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    DX::ThrowIfFailed(device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&m_raytracingOutput)));

    // Create the UAV resource
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
        UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        device->CreateUnorderedAccessView(m_raytracingOutput.Get(), nullptr, &UAVDesc, m_csuDescriptors->GetCpuHandle(AOCSUDesc::UAVRaytracingOut));
    }

    // Setup output SRV.
    {
        CreateTexture2DSRV(
            device,
            m_raytracingOutput.Get(),
            m_csuDescriptors->GetCpuHandle(AOCSUDesc::SRVRaytracingOut),
            m_raytracingOutput->GetDesc().Format
        );
    }
}

// AO algorithm.
void AO::RunAORaytracing(ID3D12Resource* pSceneConstantResource)
{
    auto commandList = m_deviceResources->GetCommandList();

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"AORaytracing");

    commandList->SetComputeRootSignature(m_raytracingGlobalRootSignature.Get());
    commandList->SetGraphicsRootSignature(m_raytracingGlobalRootSignature.Get());

    // Setup screenspace.
    auto viewport = m_deviceResources->GetScreenViewport();
    viewport.Width *= m_screenWidthScale;
    D3D12_RECT scissorRect = m_deviceResources->GetScissorRect();

    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);

    // Bind the heaps, acceleration structure and dispatch rays.
    {
        // Set index and successive vertex buffer decriptor tables
        {
            // Setup heaps
            {
                auto cbGpuAddress = pSceneConstantResource->GetGPUVirtualAddress();
                commandList->SetComputeRootConstantBufferView(AOGlobalRootSig::GlobalSceneConstSlot, cbGpuAddress);

                cbGpuAddress = m_mappedAOConstantResource->GetGPUVirtualAddress();
                commandList->SetComputeRootConstantBufferView(AOGlobalRootSig::GlobalAOConstSlot, cbGpuAddress);

                cbGpuAddress = m_mappedAOOptionsConstantResource->GetGPUVirtualAddress();
                commandList->SetComputeRootConstantBufferView(AOGlobalRootSig::GlobalAOOptionsConstSlot, cbGpuAddress);

                ID3D12DescriptorHeap* ppHeaps[] = { m_csuDescriptors->Heap(), m_samplerDescriptors->Heap() };
                commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
            }

            commandList->SetComputeRootDescriptorTable(AOGlobalRootSig::GlobalOutputViewSlot, m_csuDescriptors->GetGpuHandle(AOCSUDesc::UAVRaytracingOut));
            commandList->SetComputeRootDescriptorTable(AOGlobalRootSig::GlobalSamplerSlot, m_samplerDescriptors->GetGpuHandle(AOSamplerDesc::SamplerPointWrap));
        }

        // Bind the heaps, acceleration structure and dispatch rays.
        {
            commandList->SetComputeRootShaderResourceView(AOGlobalRootSig::GlobalAccelStructSlot, m_topLevelAccelerationStructure->GetGPUVirtualAddress());

            // Dispatch rays.
            {
                D3D12_DISPATCH_RAYS_DESC dispatchDesc               = {};
                // Since each shader table has only one shader record, the stride is same as the size.
                dispatchDesc.HitGroupTable.StartAddress             = m_hitGroupShaderTable->GetGPUVirtualAddress();
                dispatchDesc.HitGroupTable.SizeInBytes              = m_hitGroupRecordSize;
                dispatchDesc.HitGroupTable.StrideInBytes            = dispatchDesc.HitGroupTable.SizeInBytes;
                dispatchDesc.MissShaderTable.StartAddress           = m_missShaderTable->GetGPUVirtualAddress();
                dispatchDesc.MissShaderTable.SizeInBytes            = m_missShaderTable->GetDesc().Width;
                dispatchDesc.MissShaderTable.StrideInBytes          = m_missRecordSize;
                dispatchDesc.RayGenerationShaderRecord.StartAddress = m_rayGenShaderTable->GetGPUVirtualAddress();
                dispatchDesc.RayGenerationShaderRecord.SizeInBytes  = m_rayGenShaderTable->GetDesc().Width;
                dispatchDesc.Width                                  = unsigned int(std::max<long>(m_deviceResources->GetOutputSize().right - m_deviceResources->GetOutputSize().left, 1) * m_screenWidthScale);
                dispatchDesc.Height                                 = std::max<long>(m_deviceResources->GetOutputSize().bottom - m_deviceResources->GetOutputSize().top, 1);
                dispatchDesc.Depth = 1;

                commandList->SetPipelineState1(m_dxrStateObject.Get());
                commandList->DispatchRays(&dispatchDesc);
            }
        }
    }

    PIXEndEvent(commandList);
}

// Copy the raytracing output to the backbuffer.
void AO::CopyRaytracingOutputToBackbuffer()
{
    auto commandList = m_deviceResources->GetCommandList();
    auto renderTarget = m_deviceResources->GetRenderTarget();

    if (m_screenWidthScale == 1.f)
    {
        D3D12_RESOURCE_BARRIER preCopyBarriers[] = 
        {
            CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST),
            CD3DX12_RESOURCE_BARRIER::Transition(m_raytracingOutput.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE)
        };
        commandList->ResourceBarrier(_countof(preCopyBarriers), preCopyBarriers);

        commandList->CopyResource(renderTarget, m_raytracingOutput.Get());

        D3D12_RESOURCE_BARRIER postCopyBarriers[] = 
        {
            CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET),
            CD3DX12_RESOURCE_BARRIER::Transition(m_raytracingOutput.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
        };

        commandList->ResourceBarrier(_countof(postCopyBarriers), postCopyBarriers);
    }
    else
    {
        D3D12_RESOURCE_BARRIER preCopyBarriers[] = 
        {
            CD3DX12_RESOURCE_BARRIER::Transition(m_raytracingOutput.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        };
        commandList->ResourceBarrier(_countof(preCopyBarriers), preCopyBarriers);
        CD3DX12_CPU_DESCRIPTOR_HANDLE viewportHandle = m_deviceResources->GetRenderTargetView();
        commandList->OMSetRenderTargets(1, &viewportHandle, FALSE, nullptr);
        D3D12_VIEWPORT viewport = m_deviceResources->GetScreenViewport();
        commandList->RSSetViewports(1, &viewport);

        {
            m_basicEffect->SetTexture(m_csuDescriptors->GetGpuHandle(AOCSUDesc::SRVRaytracingOut), m_samplerDescriptors->GetGpuHandle(AOSamplerDesc::SamplerLinearClamp));
            m_basicEffect->Apply(commandList);
            m_primitiveBatch->Begin(commandList);
            {
                // Right Quad.
                {
                    m_primitiveBatch->DrawQuad(
                        { XMFLOAT3(m_screenWidthScale, 0.f, 0.f),{ 0.f, 0.f } },
                        { XMFLOAT3(1.f, 0.f, 0.f),{ 1.f, 0.f } },
                        { XMFLOAT3(1.f, 1.f, 0.f),{ 1.f, 1.f } },
                        { XMFLOAT3(m_screenWidthScale, 1.f, 0.f),{ 0.f, 1.f } }
                    );
                }
            }
            m_primitiveBatch->End();
        }

        D3D12_RESOURCE_BARRIER postCopyBarriers[] = 
        {
            CD3DX12_RESOURCE_BARRIER::Transition(m_raytracingOutput.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
        };

        commandList->ResourceBarrier(_countof(postCopyBarriers), postCopyBarriers);
    }
}

// Setup AO for the scene.
void AO::Setup(std::shared_ptr<DX::DeviceResources> pDeviceResources)
{
    // Run super class setup.
    Lighting::Setup(pDeviceResources);

    // Create raytracing root signature.
    CreateRootSignatures();

    // Create a raytracing pipeline state object which defines the binding of shaders, state, and resources to be used during raytracing.
    CreateRaytracingPipelineStateObject();

    // Create a heap for descriptors.
    CreateDescriptorHeaps();

    // Create constant buffers for the geometry and the scene.
    CreateConstantBuffers();

    // Create an output 2D texture to store the raytracing result to.
    CreateRaytracingOutputResource();
}

// Run AO.
void AO::Run(ID3D12Resource* pSceneConstantResource)
{
    // No reason to clear the backbuffer since the raytracing output will be copied to it.
    RunAORaytracing(pSceneConstantResource);
    CopyRaytracingOutputToBackbuffer();
}

void AO::SetMesh(std::shared_ptr<Mesh> pMesh)
{
    auto device = m_deviceResources->GetD3DDevice();

    // Store the args.
    {
        m_mesh = pMesh;

        // Check for nullptr case.
        if (!m_mesh)
            return;
    }

    // Create SRVs for vertices and indices.
    {
        size_t offset = 0;
        for (auto el : *m_mesh)
        {
            // Vertex buffer is passed to the shader along with index buffer as a descriptor table.
            // Vertex buffer descriptor must follow index buffer descriptor in the descriptor heap.

            // Note that raw SRV Buffers must use DXGI_FORMAT_R32_TYPELESS.
            // Ergo, indices must be uploaded as 32bit values regardless of actual size.
            // For the case of this sample, 32bit SRV's are used.

            // Indices.
            CreateBufferSRV(
                device,
                el.indexResource.Get(),
                m_csuDescriptors->GetCpuHandle(
                    AOCSUDesc::SRVPerObjectStart + offset),
                0,
                el.numIndices,
                0,
                DXGI_FORMAT_R32_TYPELESS,
                D3D12_BUFFER_SRV_FLAG_RAW);
            offset++;

            // Vertices.
            CreateBufferSRV(
                device,
                el.vertexResource.Get(),
                m_csuDescriptors->GetCpuHandle(
                    AOCSUDesc::SRVPerObjectStart + offset),
                0,
                el.numVertices,
                el.stride);
            offset++;

            auto& material = m_mesh->getModel()->materials[el.matID];

            // Diffuse.
            m_mesh->SetTextureSRV(
                device,
                m_csuDescriptors->GetCpuHandle(
                    AOCSUDesc::SRVPerObjectStart + offset),
                material.diffuseTextureIndex
            );
            offset++;

            // Specular.
            m_mesh->SetTextureSRV(
                device,
                m_csuDescriptors->GetCpuHandle(
                    AOCSUDesc::SRVPerObjectStart + offset),
                material.specularTextureIndex
            );
            offset++;

            // Normal.
            m_mesh->SetTextureSRV(
                device,
                m_csuDescriptors->GetCpuHandle(
                    AOCSUDesc::SRVPerObjectStart + offset),
                material.normalTextureIndex
            );
            offset++;
        }
    }

    // Build raytracing acceleration structures from the generated geometry.
    BuildAccelerationStructures();

    // Build shader tables, which define shaders and their local root arguments.
    BuildShaderTables();
}

void AO::OnSizeChanged()
{
    // Recreate output texture.
    CreateRaytracingOutputResource();
}

void AO::OnOptionUpdate(std::shared_ptr<Menus> pMenu)
{
    // Update shader.
    {
        m_mappedAOOptionsConstantResource->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedAOOptionsConstantData));

        m_mappedAOOptionsConstantData->constants.m_distance = float(pMenu->m_aoDistance.Value());
        m_mappedAOOptionsConstantData->constants.m_falloff = float(pMenu->m_aoFalloff.Value());
        m_mappedAOOptionsConstantData->constants.m_numSamples = unsigned int(pMenu->m_aoNumSamples.Value());
        m_mappedAOOptionsConstantData->constants.m_sampleType = unsigned int(pMenu->m_aoSampleType.Value());

        m_mappedAOOptionsConstantResource->Unmap(0, nullptr);
    }

    // Update samples (must be done since stratified).
    {
        m_mappedAOConstantResource->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedAOConstantData));

        std::unique_ptr<Sampler> sampler;
        Sampler::SetSeed(0);

        switch (unsigned int(pMenu->m_aoSampleType.Value()))
        {
        case AOSampleType::Uniform:
            sampler = std::unique_ptr<Sampler>{
                new StratifiedSampler<UniformHemiSampler>()
            };

            break;
        default: // Assume cosine.

            sampler = std::unique_ptr<Sampler>{
                new StratifiedSampler<CosineHemiSampler>()
            };
        }

        sampler->Sample(
            m_mappedAOConstantData->constants.rays,
            unsigned int(pMenu->m_aoNumSamples.Value())
        );

        m_mappedAOConstantResource->Unmap(0, nullptr);
    }
}