//--------------------------------------------------------------------------------------
// D3D12RaytracingAO.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "GlobalSharedHlslCompat.h"
#include "SSAOHlslCompat.h"
#include "DeviceResources.h"
#include "Effects.h"
#include "GeneralHelper.h"
#include "Lighting.h"
#include "Menus.h"
#include "PrimitiveBatch.h"
#include "VertexTypes.h"

namespace SSAOPerObjectCSUDesc
{
    enum PerObjectCSUDesc
    {
        SRVDiffuse,
        SRVSpecular,
        SRVNormal,
        CSUCount
    };
}

namespace SSAORootSig
{
    enum RootSig
    {
        RootSceneConstSlot,
        RootSSAOConstSlot,
        RootMaterialConstSlot,
        RootSRVBeginSlot,
        RootSRVEndSlot = RootSRVBeginSlot + 5 - 1,
        RootUAVBeginSlot,
        RootUAVEndSlot = RootUAVBeginSlot + 9 - 1,
        RootSamplerSlot,
        RootCount
    };
}

namespace SSAOCSUDesc
{
    enum CSUDesc
    {
        CBVScene,
        CBVSampleThickness,
        SRVDepthDownsizeStart,
        SRVDepthDownsizeEnd = SRVDepthDownsizeStart + NUM_BUFFERS - 1,
        SRVDepthTiledStart,
        SRVDepthTiledEnd = SRVDepthTiledStart + NUM_BUFFERS - 1,
        SRVNormalDownsizeStart,
        SRVNormalDownsizeEnd = SRVNormalDownsizeStart + NUM_BUFFERS - 1,
        SRVNormalTiledStart,
        SRVNormalTiledEnd = SRVNormalTiledStart + NUM_BUFFERS - 1,
        SRVMergedStart,
        SRVMergedEnd = SRVMergedStart + NUM_BUFFERS - 1,
        SRVSmoothStart,
        SRVSmoothEnd = SRVSmoothStart + NUM_BUFFERS - 1 - 1,
        SRVHighQualityStart,
        SRVHighQualityEnd = SRVHighQualityStart + NUM_BUFFERS - 1,
        SRVLinearDepth,
        SRVDepth,
        SRVGBuffer,
        SRVSSAO,
        SRVOutFrame,
        SRVPerObjectStart,
        SRVPerObjectEnd = SRVPerObjectStart + Parts::MaxParts * SSAOPerObjectCSUDesc::CSUCount - 1,
        UAVDepthDownsizeStart,
        UAVDepthDownsizeEnd = UAVDepthDownsizeStart + NUM_BUFFERS - 1,
        UAVDepthTiledStart,
        UAVDepthTiledEnd = UAVDepthTiledStart + NUM_BUFFERS - 1,
        UAVNormalDownsizeStart,
        UAVNormalDownsizeEnd = UAVNormalDownsizeStart + NUM_BUFFERS - 1,
        UAVNormalTiledStart,
        UAVNormalTiledEnd = UAVNormalTiledStart + NUM_BUFFERS - 1,
        UAVMergedStart,
        UAVMergedEnd = UAVMergedStart + NUM_BUFFERS - 1,
        UAVSmoothStart,
        UAVSmoothEnd = UAVSmoothStart + NUM_BUFFERS - 1 - 1,
        UAVHighQualityStart,
        UAVHighQualityEnd = UAVHighQualityStart + NUM_BUFFERS - 1,
        UAVLinearDepth,
        UAVSSAO,
        UAVOutFrame,
        CSUCount
    };
}

namespace SSAOSamplerDesc
{
    enum SamplerDesc
    {
        SamplerLinearWrap,
        SamplerLinearBorder,
        SamplerLinearClamp,
        SamplerPointClamp,
        SamplerCount
    };
}

namespace SSAORTVDesc
{
    enum RTVDesc
    {
        RTVGBuffer,
        RTVCount
    };
}

namespace SSAODSVDesc
{
    enum DSVDesc
    {
        DSVGBuffer,
        DSVCount
    };
}

class SSAO : public Lighting
{
public:
    SSAO();

    void Setup(std::shared_ptr<DX::DeviceResources> pDeviceResources) override;
    void Run(ID3D12Resource* pSceneConstantResource) override;
    void OnSizeChanged() override;
    void OnOptionUpdate(std::shared_ptr<Menus> pMenu) override;
    void OnCameraChanged(XMMATRIX& world, XMMATRIX& view, XMMATRIX& projection) override;

    void SetMesh(std::shared_ptr<Mesh> pMesh);

private:
    void BindResources();
    void CreateDescriptorHeaps();
    void CreateResources();
    void CreateRootSignatures();
    void SetupPipelines();
    void BindSRVGraphics(D3D12_GPU_DESCRIPTOR_HANDLE* srvs, unsigned int count);
    void BindUAVGraphics(D3D12_GPU_DESCRIPTOR_HANDLE* uavs, unsigned int count);
    void BindSRVCompute(D3D12_GPU_DESCRIPTOR_HANDLE* srvs, unsigned int count);
    void BindUAVCompute(D3D12_GPU_DESCRIPTOR_HANDLE* uavs, unsigned int count);
    void UpdateSSAOConstant(
        void* pConstData,
        const unsigned int width,
        const unsigned int height,
        const unsigned int depth,
        const float tanHalfFovH);
    void DispatchSSAO(
        D3D12_GPU_VIRTUAL_ADDRESS pConstantBuffer,
        D3D12_GPU_DESCRIPTOR_HANDLE pDestination,
        D3D12_GPU_DESCRIPTOR_HANDLE pDepthBuffer,
        D3D12_GPU_DESCRIPTOR_HANDLE pNormalBuffer,
        const unsigned int width,
        const unsigned int height,
        const unsigned int depth);
    void UpdateBlurAndUpsampleConstant(
        void* pConstData,
        const unsigned int lowWidth,
        const unsigned int lowHeight,
        const unsigned int highWidth,
        const unsigned int highHeight);
    void DispatchBlurAndUpsample(
        D3D12_GPU_VIRTUAL_ADDRESS pConstantBuffer,
        D3D12_GPU_DESCRIPTOR_HANDLE pDestination,
        D3D12_GPU_DESCRIPTOR_HANDLE pLowResDepthBuffer,
        D3D12_GPU_DESCRIPTOR_HANDLE pHighResDepthBuffer,
        D3D12_GPU_DESCRIPTOR_HANDLE pinterleavedBuffer,
        D3D12_GPU_DESCRIPTOR_HANDLE pHighResBuffer,
        D3D12_GPU_DESCRIPTOR_HANDLE pHighQualityBuffer,
        const unsigned int highWidth,
        const unsigned int highHeight);
    void UpdateConstants();

    // Heaps.
    std::unique_ptr<DirectX::DescriptorHeap> m_csuDescriptors;
    std::unique_ptr<DirectX::DescriptorHeap> m_samplerDescriptors;
    std::unique_ptr<DirectX::DescriptorHeap> m_rtvDescriptors;
    std::unique_ptr<DirectX::DescriptorHeap> m_dsvDescriptors;

    // Mesh.
    std::shared_ptr<Mesh> m_mesh;

    // Root signature.
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;

    // Pipeline states.
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_gBufferResourcePipelineState;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_SSAOBlurUpsamplePipelineState;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_SSAOBlurUpsamplePreMinPipelineState;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_SSAOBlurUpsampleBlendOutPipelineState;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_SSAOBlurUpsamplePreMinBlendOutPipelineState;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_SSAOGBufferRenderPipelineState;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_SSAOPrepareDepthBuffers1PipelineState;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_SSAOPrepareDepthBuffers2PipelineState;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_SSAOPRender1PipelineState;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_SSAOPRender2PipelineState;

    // Resources
    Microsoft::WRL::ComPtr<ID3D12Resource> m_depthDownsizeResource[NUM_BUFFERS];
    Microsoft::WRL::ComPtr<ID3D12Resource> m_depthTiledResource[NUM_BUFFERS];
    Microsoft::WRL::ComPtr<ID3D12Resource> m_normalDownsizeResource[NUM_BUFFERS];
    Microsoft::WRL::ComPtr<ID3D12Resource> m_normalTiledResource[NUM_BUFFERS];
    Microsoft::WRL::ComPtr<ID3D12Resource> m_mergedResource[NUM_BUFFERS];
    Microsoft::WRL::ComPtr<ID3D12Resource> m_smoothResource[NUM_BUFFERS - 1];
    Microsoft::WRL::ComPtr<ID3D12Resource> m_highQualityResource[NUM_BUFFERS];
    Microsoft::WRL::ComPtr<ID3D12Resource> m_linearDepthResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_gBufferResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_dBufferResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_samplerResources[2];
    Microsoft::WRL::ComPtr<ID3D12Resource> m_ssaoResources;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_outFrameResources;

    // Constant Buffers.
    union AlignedMaterialConstantBuffer
    {
        MaterialConstantBuffer constants;
        uint8_t alignmentPadding[CalculateConstantBufferByteSize(sizeof(MaterialConstantBuffer))];
    };
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_materialListCB;

    // Buffer sizes.
    uint32_t bufferWidth[NUM_BUFFERS + 2];
    uint32_t bufferHeight[NUM_BUFFERS + 2];

    // Cached vars.
    float m_sampleThickness[12];

    // This is necessary to filter out pixel shimmer due to bilateral upsampling with too much lost resolution.  High
    // frequency detail can sometimes not be reconstructed, and the noise filter fills in the missing pixels with the
    // result of the higher resolution SSAO.
    float m_noiseFilterTolerance = -3.f;
    float m_blurTolerance = -5.f;
    float m_upsampleTolerance = -7.f;

    float m_normalMultiply = 1.f;

    union AlignedSSAORenderConstantBuffer
    {
        SSAORenderConstantBuffer constants;
        uint8_t alignmentPadding[CalculateConstantBufferByteSize(sizeof(SSAORenderConstantBuffer))];
    };
    Microsoft::WRL::ComPtr<ID3D12Resource> m_mappedDepthTiledSSAORenderConstantResource[NUM_BUFFERS];
    void* m_mappedDepthTiledSSAORenderConstantData[NUM_BUFFERS];

    Microsoft::WRL::ComPtr<ID3D12Resource> m_mappedHighQualitySSAORenderConstantResource[NUM_BUFFERS];
    void* m_mappedHighQualitySSAORenderConstantData[NUM_BUFFERS];

    union AlignedBlurAndUpscaleConstantBuffer
    {
        BlurAndUpscaleConstantBuffer constants;
        uint8_t alignmentPadding[CalculateConstantBufferByteSize(sizeof(BlurAndUpscaleConstantBuffer))];
    };
    Microsoft::WRL::ComPtr<ID3D12Resource> m_mappedBlurAndUpscaleConstantResource[NUM_BUFFERS];
    void* m_mappedBlurAndUpscaleConstantData[NUM_BUFFERS];
};