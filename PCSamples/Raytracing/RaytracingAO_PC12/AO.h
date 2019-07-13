//--------------------------------------------------------------------------------------
// D3D12RaytracingAO.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "AORaytracingHlslCompat.h"
#include "GlobalSharedHlslCompat.h"
#include "DeviceResources.h"
#include "GeneralHelper.h"
#include "Lighting.h"

namespace AOSampleType
{
    enum SampleType
    {
        Uniform,
        COSINE
    };
}

namespace AOPerObjectCSUDesc
{
    enum PerObjectCSUDesc
    {
        SRVIndices,
        SRVVertices,
        SRVDiffuse,
        SRVSpecular,
        SRVNormal,
        CSUCount
    };
}

namespace AOCSUDesc
{
    enum CSUDesc
    {
        CBVScene,
        SRVNoise,
        SRVBottomLevelAccel,
        SRVTopLevelAccel,
        UAVRaytracingOut,
        SRVPerObjectStart,
        SRVPerObjectEnd = SRVPerObjectStart + Parts::MaxParts * AOPerObjectCSUDesc::CSUCount - 1,
        SRVRaytracingOut,
        CSUCount
    };
}

namespace AOSamplerDesc
{
    enum SamplerDesc
    {
        SamplerPointWrap,
        SamplerLinearClamp,
        SamplerCount
    };
}

namespace AOGlobalRootSig
{
    enum GlobalRootSig
    {
        GlobalOutputViewSlot,
        GlobalAccelStructSlot,
        GlobalSceneConstSlot,
        GlobalAOConstSlot,
        GlobalAOOptionsConstSlot,
        GlobalSamplerSlot,
        GlobalCount
    };
}

namespace AOLocalRootSig
{
    enum LocalRootSig
    {
        LocalSRVBufferSlot,
        LocalMeshConstSlot,
        LocalCount
    };
}

class AO : public Lighting
{
public:
    void Setup(std::shared_ptr<DX::DeviceResources> pDeviceResources) override;

    void Run(ID3D12Resource* pSceneConstantResource) override;

    void SetMesh(std::shared_ptr<Mesh> pMesh) override;

    void OnSizeChanged() override;

    void OnOptionUpdate(std::shared_ptr<Menus> pMenu) override;

private:
    void CreateRootSignatures();
    void CreateLocalRootSignatureSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline);
    void CreateRaytracingPipelineStateObject();
    void CreateDescriptorHeaps();
    void BuildAccelerationStructures();
    void CreateConstantBuffers();
    void BuildShaderTables();
    void CreateRaytracingOutputResource();
    void RunAORaytracing(ID3D12Resource* pSceneConstantResource);
    void CopyRaytracingOutputToBackbuffer();

    // Pipeline
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_raytracingGlobalRootSignature;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_raytracingLocalRootSignature;
#ifdef USE_NON_NULL_LOCAL_ROOT_SIG
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_raytracingLocalRootSignatureEmpty;
#endif
    Microsoft::WRL::ComPtr<ID3D12StateObject>   m_dxrStateObject;

    // Acceleration structure
    Microsoft::WRL::ComPtr<ID3D12Resource> m_bottomLevelAccelerationStructure;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_topLevelAccelerationStructure;

    // Constant Buffers.
    union AlignedAOConstantBuffer
    {
        AOConstantBuffer constants;
        uint8_t alignmentPadding[CalculateConstantBufferByteSize(sizeof(AOConstantBuffer))];
    };
    Microsoft::WRL::ComPtr<ID3D12Resource> m_mappedAOConstantResource;
    AlignedAOConstantBuffer* m_mappedAOConstantData;

    union AlignedAOOptionsConstantBuffer
    {
        AOOptionsConstantBuffer constants;
        uint8_t alignmentPadding[CalculateConstantBufferByteSize(sizeof(AOOptionsConstantBuffer))];
    };
    Microsoft::WRL::ComPtr<ID3D12Resource> m_mappedAOOptionsConstantResource;
    AlignedAOOptionsConstantBuffer* m_mappedAOOptionsConstantData;

    // Heaps
    std::unique_ptr<DirectX::DescriptorHeap> m_csuDescriptors;
    std::unique_ptr<DirectX::DescriptorHeap> m_samplerDescriptors;

    // Output
    Microsoft::WRL::ComPtr<ID3D12Resource> m_raytracingOutput;

    // Shader Tables.
    static const wchar_t* c_hitGroupNames[TraceRayParameters::HitGroup::Count];
    static const wchar_t* c_raygenShaderName;
    static const wchar_t* c_closestHitShaderNames[TraceRayParameters::HitGroup::Count];
    static const wchar_t* c_missShaderNames[TraceRayParameters::MissShader::Count];
    Microsoft::WRL::ComPtr<ID3D12Resource> m_missShaderTable;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_hitGroupShaderTable;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_rayGenShaderTable;
    unsigned int m_hitGroupRecordSize;
    unsigned int m_missRecordSize;

    // Mesh.
    std::shared_ptr<Mesh> m_mesh;

    // Options.
    float m_distance = 100.f;
    float m_falloff = 0.f;
    unsigned int m_numSamples = 15;
    unsigned int m_sampleType = 0;
};