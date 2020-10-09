//--------------------------------------------------------------------------------------
// D3D12RaytracingAO.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SSAO.h"
#include "CommonStates.h"

#include "CompiledShaders/SSAOBlurUpsampleCS.hlsl.h"
#include "CompiledShaders/SSAOBlurUpsamplePreMinCS.hlsl.h"
#include "CompiledShaders/SSAOBlurUpsamplePreMinBlendOutCS.hlsl.h"
#include "CompiledShaders/SSAOBlurUpsampleBlendOutCS.hlsl.h"
#include "CompiledShaders/SSAOGBufferRenderCS.hlsl.h"
#include "CompiledShaders/SSAOPrepareDepthBuffers1CS.hlsl.h"
#include "CompiledShaders/SSAOPrepareDepthBuffers2CS.hlsl.h"
#include "CompiledShaders/SSAORender1CS.hlsl.h"
#include "CompiledShaders/SSAORender2CS.hlsl.h"
#include "CompiledShaders/GBufferVS.hlsl.h"
#include "CompiledShaders/GBufferGS.hlsl.h"
#include "CompiledShaders/GBufferPS.hlsl.h"

using namespace DirectX;

using Microsoft::WRL::ComPtr;

SSAO::SSAO()
{
    // Init sample thickness.
    m_sampleThickness[0] = sqrt(1.f - 0.2f * 0.2f);
    m_sampleThickness[1] = sqrt(1.f - 0.4f * 0.4f);
    m_sampleThickness[2] = sqrt(1.f - 0.6f * 0.6f);
    m_sampleThickness[3] = sqrt(1.f - 0.8f * 0.8f);
    m_sampleThickness[4] = sqrt(1.f - 0.2f * 0.2f - 0.2f * 0.2f);
    m_sampleThickness[5] = sqrt(1.f - 0.2f * 0.2f - 0.4f * 0.4f);
    m_sampleThickness[6] = sqrt(1.f - 0.2f * 0.2f - 0.6f * 0.6f);
    m_sampleThickness[7] = sqrt(1.f - 0.2f * 0.2f - 0.8f * 0.8f);
    m_sampleThickness[8] = sqrt(1.f - 0.4f * 0.4f - 0.4f * 0.4f);
    m_sampleThickness[9] = sqrt(1.f - 0.4f * 0.4f - 0.6f * 0.6f);
    m_sampleThickness[10] = sqrt(1.f - 0.4f * 0.4f - 0.8f * 0.8f);
    m_sampleThickness[11] = sqrt(1.f - 0.6f * 0.6f - 0.6f * 0.6f);
}

// Setup the root signatures for the shaders.
void SSAO::CreateRootSignatures()
{
    // Root Signature
    {
        CD3DX12_DESCRIPTOR_RANGE rangesSRV[(SSAORootSig::RootSRVEndSlot - SSAORootSig::RootSRVBeginSlot + 1)];
        {
            for (unsigned int i = 0; i < _countof(rangesSRV); i++)
                rangesSRV[i].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, i);
        }

        CD3DX12_DESCRIPTOR_RANGE rangesUAV[(SSAORootSig::RootUAVEndSlot - SSAORootSig::RootUAVBeginSlot + 1)];
        {
            for (unsigned int i = 0; i < _countof(rangesUAV); i++)
                rangesUAV[i].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, i);
        }

        CD3DX12_DESCRIPTOR_RANGE rangeSampler;
        {
            rangeSampler.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, SSAOSamplerDesc::SamplerCount, 0);
        }

        CD3DX12_ROOT_PARAMETER rootParameters[SSAORootSig::RootCount];
        rootParameters[SSAORootSig::RootSceneConstSlot].InitAsConstantBufferView(0);
        rootParameters[SSAORootSig::RootSSAOConstSlot].InitAsConstantBufferView(1);
        rootParameters[SSAORootSig::RootMaterialConstSlot].InitAsConstantBufferView(2);

        for (unsigned int i = 0; i < _countof(rangesSRV); i++)
            rootParameters[SSAORootSig::RootSRVBeginSlot + i].InitAsDescriptorTable(1, &rangesSRV[i]);
        for (unsigned int i = 0; i < _countof(rangesUAV); i++)
            rootParameters[SSAORootSig::RootUAVBeginSlot + i].InitAsDescriptorTable(1, &rangesUAV[i]);

        rootParameters[SSAORootSig::RootSamplerSlot].InitAsDescriptorTable(1, &rangeSampler);

        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
        SerializeAndCreateRootSignature(
            m_deviceResources->GetD3DDevice(),
            rootSignatureDesc,
            m_rootSignature);
    }
}

// Setup the pipelines.
void SSAO::SetupPipelines()
{
    auto device = m_deviceResources->GetD3DDevice();

    // Rasterizer Pipeline.
    {
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "SV_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        CD3DX12_DEPTH_STENCIL_DESC1 depthDesc(D3D12_DEFAULT);
        depthDesc.DepthEnable = TRUE;
        depthDesc.StencilEnable = TRUE;

        // Describe and create the gBuffer pipeline.
        D3D12_GRAPHICS_PIPELINE_STATE_DESC gBufferPsoDesc = {};
        gBufferPsoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        gBufferPsoDesc.pRootSignature = m_rootSignature.Get();
        gBufferPsoDesc.VS = CD3DX12_SHADER_BYTECODE((void *)g_pGBufferVS, sizeof(g_pGBufferVS));
        gBufferPsoDesc.GS = CD3DX12_SHADER_BYTECODE((void *)g_pGBufferGS, sizeof(g_pGBufferGS));
        gBufferPsoDesc.PS = CD3DX12_SHADER_BYTECODE((void *)g_pGBufferPS, sizeof(g_pGBufferPS));
        gBufferPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        gBufferPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        gBufferPsoDesc.DepthStencilState = depthDesc;
        gBufferPsoDesc.SampleMask = UINT_MAX;
        gBufferPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        gBufferPsoDesc.NumRenderTargets = 1;
        gBufferPsoDesc.RTVFormats[0] = DXGI_FORMAT_R11G11B10_FLOAT;
        gBufferPsoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        gBufferPsoDesc.SampleDesc.Count = 1;
        DX::ThrowIfFailed(device->CreateGraphicsPipelineState(&gBufferPsoDesc, IID_PPV_ARGS(&m_gBufferResourcePipelineState)));
    }

    // Compute Pipeline.
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineDesc = {};
        computePipelineDesc.NodeMask = 0;
        computePipelineDesc.pRootSignature = m_rootSignature.Get();
        computePipelineDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

        computePipelineDesc.CS = CD3DX12_SHADER_BYTECODE((void *)g_pSSAOBlurUpsampleCS, sizeof(g_pSSAOBlurUpsampleCS));
        device->CreateComputePipelineState(&computePipelineDesc, IID_PPV_ARGS(&m_SSAOBlurUpsamplePipelineState));

        computePipelineDesc.CS = CD3DX12_SHADER_BYTECODE((void *)g_pSSAOBlurUpsamplePreMinCS, sizeof(g_pSSAOBlurUpsamplePreMinCS));
        device->CreateComputePipelineState(&computePipelineDesc, IID_PPV_ARGS(&m_SSAOBlurUpsamplePreMinPipelineState));

        computePipelineDesc.CS = CD3DX12_SHADER_BYTECODE((void *)g_pSSAOBlurUpsampleCS, sizeof(g_pSSAOBlurUpsampleCS));
        device->CreateComputePipelineState(&computePipelineDesc, IID_PPV_ARGS(&m_SSAOBlurUpsamplePipelineState));

        computePipelineDesc.CS = CD3DX12_SHADER_BYTECODE((void *)g_pSSAOBlurUpsampleBlendOutCS, sizeof(g_pSSAOBlurUpsampleBlendOutCS));
        device->CreateComputePipelineState(&computePipelineDesc, IID_PPV_ARGS(&m_SSAOBlurUpsampleBlendOutPipelineState));

        computePipelineDesc.CS = CD3DX12_SHADER_BYTECODE((void *)g_pSSAOBlurUpsamplePreMinBlendOutCS, sizeof(g_pSSAOBlurUpsamplePreMinBlendOutCS));
        device->CreateComputePipelineState(&computePipelineDesc, IID_PPV_ARGS(&m_SSAOBlurUpsamplePreMinBlendOutPipelineState));

        computePipelineDesc.CS = CD3DX12_SHADER_BYTECODE((void *)g_pSSAOGBufferRenderCS, sizeof(g_pSSAOGBufferRenderCS));
        device->CreateComputePipelineState(&computePipelineDesc, IID_PPV_ARGS(&m_SSAOGBufferRenderPipelineState));

        computePipelineDesc.CS = CD3DX12_SHADER_BYTECODE((void *)g_pSSAOPrepareDepthBuffers1CS, sizeof(g_pSSAOPrepareDepthBuffers1CS));
        device->CreateComputePipelineState(&computePipelineDesc, IID_PPV_ARGS(&m_SSAOPrepareDepthBuffers1PipelineState));

        computePipelineDesc.CS = CD3DX12_SHADER_BYTECODE((void *)g_pSSAOPrepareDepthBuffers2CS, sizeof(g_pSSAOPrepareDepthBuffers2CS));
        device->CreateComputePipelineState(&computePipelineDesc, IID_PPV_ARGS(&m_SSAOPrepareDepthBuffers2PipelineState));

        computePipelineDesc.CS = CD3DX12_SHADER_BYTECODE((void *)g_pSSAORender1CS, sizeof(g_pSSAORender1CS));
        device->CreateComputePipelineState(&computePipelineDesc, IID_PPV_ARGS(&m_SSAOPRender1PipelineState));

        computePipelineDesc.CS = CD3DX12_SHADER_BYTECODE((void *)g_pSSAORender2CS, sizeof(g_pSSAORender2CS));
        device->CreateComputePipelineState(&computePipelineDesc, IID_PPV_ARGS(&m_SSAOPRender2PipelineState));
    }
}

// Setup descriptor heaps.
void SSAO::CreateDescriptorHeaps()
{
    auto device = m_deviceResources->GetD3DDevice();

    // Allocate a csu heap.
    {
        const uint32_t c_csuCount = SSAOCSUDesc::CSUCount;
        m_csuDescriptors = std::make_unique<DescriptorHeap>(device, c_csuCount);
    }

    // Allocate a sampler heap.
    {
        const uint32_t c_samplerCount = SSAOSamplerDesc::SamplerCount;
        m_samplerDescriptors = std::make_unique<DescriptorHeap>(
            device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, c_samplerCount);
    }

    // Allocate a rtv.
    {
        const uint32_t c_rtvCount = SSAORTVDesc::RTVCount;
        m_rtvDescriptors = std::make_unique<DescriptorHeap>(
            device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, c_rtvCount);
    }

    // Allocate a dsv.
    {
        const uint32_t c_dsvCount = SSAODSVDesc::DSVCount;
        m_dsvDescriptors = std::make_unique<DescriptorHeap>(
            device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, c_dsvCount);
    }
}

void SSAO::CreateResources()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto renderTarget = m_deviceResources->GetRenderTarget();

    auto output = m_deviceResources->GetOutputSize();
    auto screenWidth = static_cast<UINT>(float(output.right - output.left) * m_screenWidthScale);
    auto screenHeight = static_cast<UINT>(output.bottom - output.top);

    D3D12_CLEAR_VALUE clear = {};
    clear.Color[0] = .0f;
    clear.Color[1] = .0f;
    clear.Color[2] = .0f;
    clear.Color[3] = 1.f;

    D3D12_CLEAR_VALUE depthClear = {};
    depthClear.DepthStencil.Depth = 1.f;
    depthClear.DepthStencil.Stencil = 0;

    // Create GBuffer.
    {
        clear.Format = DXGI_FORMAT_R11G11B10_FLOAT;
        AllocateTexture2DArr(
            device,
            &m_gBufferResource,
            screenWidth,
            screenHeight,
            2,
            &clear,
            DXGI_FORMAT_R11G11B10_FLOAT,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
        );

        depthClear.Format = DXGI_FORMAT_D32_FLOAT;
        AllocateTexture2DArr(
            device,
            &m_dBufferResource,
            screenWidth,
            screenHeight,
            2,
            &depthClear,
            DXGI_FORMAT_D32_FLOAT,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
        );
    }

    // Update creation vars.
    {
        for (unsigned int i = 0; i < _countof(bufferWidth); i++)
        {
            const unsigned int power = 1 << (i + 1);

            bufferWidth[i] = ((UINT)screenWidth + power - 1) / power;
            bufferHeight[i] = (screenHeight + power - 1) / power;
        }
    }

    // Allocate constant buffers.
    {
        for (unsigned int i = 0; i < _countof(m_mappedDepthTiledSSAORenderConstantResource); i++)
        {
            AllocateUploadBuffer(
                device,
                nullptr,
                sizeof(AlignedBlurAndUpscaleConstantBuffer),
                &m_mappedDepthTiledSSAORenderConstantResource[i]
            );

            m_mappedDepthTiledSSAORenderConstantResource[i]->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedDepthTiledSSAORenderConstantData[i]));
        }

        for (unsigned int i = 0; i < _countof(m_mappedHighQualitySSAORenderConstantResource); i++)
        {
            AllocateUploadBuffer(
                device,
                nullptr,
                sizeof(AlignedSSAORenderConstantBuffer),
                &m_mappedHighQualitySSAORenderConstantResource[i]
            );

            m_mappedHighQualitySSAORenderConstantResource[i]->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedHighQualitySSAORenderConstantData[i]));
        }

        for (unsigned int i = 0; i < _countof(m_mappedBlurAndUpscaleConstantResource); i++)
        {
            AllocateUploadBuffer(
                device,
                nullptr,
                sizeof(AlignedSSAORenderConstantBuffer),
                &m_mappedBlurAndUpscaleConstantResource[i]
            );

            m_mappedBlurAndUpscaleConstantResource[i]->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedBlurAndUpscaleConstantData[i]));
        }
    }

    // Allocate algorithm buffers.
    {
        AllocateTexture2DArr(
            device,
            &m_linearDepthResource,
            screenWidth,
            screenHeight,
            1,
            nullptr,
            DXGI_FORMAT_R16_FLOAT,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
        );

        for (unsigned int i = 0; i < _countof(m_depthDownsizeResource); i++)
        {
            AllocateTexture2DArr(
                device,
                &m_depthDownsizeResource[i],
                bufferWidth[i],
                bufferHeight[i],
                1,
                nullptr,
                DXGI_FORMAT_R32_FLOAT,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
            );
        }

        for (unsigned int i = 0; i < _countof(m_depthTiledResource); i++)
        {
            AllocateTexture2DArr(
                device,
                &m_depthTiledResource[i],
                bufferWidth[i + 2],
                bufferHeight[i + 2],
                16,
                nullptr,
                DXGI_FORMAT_R16_FLOAT,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
            );
        }

        for (unsigned int i = 0; i < _countof(m_normalDownsizeResource); i++)
        {
            AllocateTexture2DArr(
                device,
                &m_normalDownsizeResource[i],
                bufferWidth[i],
                bufferHeight[i],
                1,
                nullptr,
                DXGI_FORMAT_R10G10B10A2_UNORM,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
            );
        }

        for (unsigned int i = 0; i < _countof(m_normalTiledResource); i++)
        {
            AllocateTexture2DArr(
                device,
                &m_normalTiledResource[i],
                bufferWidth[i + 2],
                bufferHeight[i + 2],
                16,
                nullptr,
                DXGI_FORMAT_R10G10B10A2_UNORM,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
            );
        }

        for (unsigned int i = 0; i < _countof(m_mergedResource); i++)
        {
            AllocateTexture2DArr(
                device,
                &m_mergedResource[i],
                bufferWidth[i],
                bufferHeight[i],
                1,
                nullptr,
                DXGI_FORMAT_R8_UNORM,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
            );
        }

        for (unsigned int i = 0; i < _countof(m_smoothResource); i++)
        {
            AllocateTexture2DArr(
                device,
                &m_smoothResource[i],
                bufferWidth[i],
                bufferHeight[i],
                1,
                nullptr,
                DXGI_FORMAT_R8_UNORM,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
            );
        }

        for (unsigned int i = 0; i < _countof(m_highQualityResource); i++)
        {
            AllocateTexture2DArr(
                device,
                &m_highQualityResource[i],
                bufferWidth[i],
                bufferHeight[i],
                1,
                nullptr,
                DXGI_FORMAT_R8_UNORM,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
            );
        }
    }

    // Allocate final output.
    {
        AllocateTexture2DArr(
            device,
            &m_ssaoResources,
            screenWidth,
            screenHeight,
            1,
            nullptr,
            DXGI_FORMAT_R8_UNORM,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
        );

        AllocateTexture2DArr(
            device,
            &m_outFrameResources,
            screenWidth,
            screenHeight,
            1,
            nullptr,
            renderTarget->GetDesc().Format,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
        );
    }
}

void SSAO::BindResources()
{
    auto device = m_deviceResources->GetD3DDevice();

    //  Depth downsize.
    {
        for (unsigned int i = 0; i < _countof(m_depthDownsizeResource); i++)
        {
            CreateTexture2DSRV(
                device,
                m_depthDownsizeResource[i].Get(),
                m_csuDescriptors->GetCpuHandle(SSAOCSUDesc::SRVDepthDownsizeStart + i),
                m_depthDownsizeResource[i]->GetDesc().Format
            );

            CreateTexture2DUAV(
                device,
                m_depthDownsizeResource[i].Get(),
                m_csuDescriptors->GetCpuHandle(SSAOCSUDesc::UAVDepthDownsizeStart + i),
                m_depthDownsizeResource[i]->GetDesc().Format
            );
        }
    }

    //  Depth tiled.
    {
        for (unsigned int i = 0; i < _countof(m_depthTiledResource); i++)
        {
            CreateTexture2DArrSRV(
                device,
                m_depthTiledResource[i].Get(),
                m_csuDescriptors->GetCpuHandle(SSAOCSUDesc::SRVDepthTiledStart + i),
                m_depthTiledResource[i]->GetDesc().DepthOrArraySize,
                m_depthTiledResource[i]->GetDesc().Format
            );

            CreateTexture2DArrUAV(
                device,
                m_depthTiledResource[i].Get(),
                m_csuDescriptors->GetCpuHandle(SSAOCSUDesc::UAVDepthTiledStart + i),
                m_depthTiledResource[i]->GetDesc().DepthOrArraySize,
                m_depthTiledResource[i]->GetDesc().Format
            );
        }
    }

    // Normal Downsize.
    {
        for (unsigned int i = 0; i < _countof(m_normalDownsizeResource); i++)
        {
            CreateTexture2DSRV(
                device,
                m_normalDownsizeResource[i].Get(),
                m_csuDescriptors->GetCpuHandle(SSAOCSUDesc::SRVNormalDownsizeStart + i),
                m_normalDownsizeResource[i]->GetDesc().Format
            );

            CreateTexture2DUAV(
                device,
                m_normalDownsizeResource[i].Get(),
                m_csuDescriptors->GetCpuHandle(SSAOCSUDesc::UAVNormalDownsizeStart + i),
                m_normalDownsizeResource[i]->GetDesc().Format
            );
        }
    }

    // Normal Tiled.
    {
        for (unsigned int i = 0; i < _countof(m_normalTiledResource); i++)
        {
            CreateTexture2DArrSRV(
                device,
                m_normalTiledResource[i].Get(),
                m_csuDescriptors->GetCpuHandle(SSAOCSUDesc::SRVNormalTiledStart + i),
                m_normalTiledResource[i]->GetDesc().DepthOrArraySize,
                m_normalTiledResource[i]->GetDesc().Format
            );

            CreateTexture2DArrUAV(
                device,
                m_normalTiledResource[i].Get(),
                m_csuDescriptors->GetCpuHandle(SSAOCSUDesc::UAVNormalTiledStart + i),
                m_normalTiledResource[i]->GetDesc().DepthOrArraySize,
                m_normalTiledResource[i]->GetDesc().Format
            );
        }
    }

    // Merged.
    {
        for (unsigned int i = 0; i < _countof(m_mergedResource); i++)
        {
            CreateTexture2DSRV(
                device,
                m_mergedResource[i].Get(),
                m_csuDescriptors->GetCpuHandle(SSAOCSUDesc::SRVMergedStart + i),
                m_mergedResource[i]->GetDesc().Format
            );

            CreateTexture2DUAV(
                device,
                m_mergedResource[i].Get(),
                m_csuDescriptors->GetCpuHandle(SSAOCSUDesc::UAVMergedStart + i),
                m_mergedResource[i]->GetDesc().Format
            );
        }
    }

    //  Smooth resource.
    {
        for (unsigned int i = 0; i < _countof(m_smoothResource); i++)
        {
            CreateTexture2DSRV(
                device,
                m_smoothResource[i].Get(),
                m_csuDescriptors->GetCpuHandle(SSAOCSUDesc::SRVSmoothStart + i),
                m_smoothResource[i]->GetDesc().Format
            );

            CreateTexture2DUAV(
                device,
                m_smoothResource[i].Get(),
                m_csuDescriptors->GetCpuHandle(SSAOCSUDesc::UAVSmoothStart + i),
                m_smoothResource[i]->GetDesc().Format
            );
        }
    }

    //  High quality resource.
    {
        for (unsigned int i = 0; i < _countof(m_highQualityResource); i++)
        {
            CreateTexture2DSRV(
                device,
                m_highQualityResource[i].Get(),
                m_csuDescriptors->GetCpuHandle(SSAOCSUDesc::SRVHighQualityStart + i),
                m_highQualityResource[i]->GetDesc().Format
            );

            CreateTexture2DUAV(
                device,
                m_highQualityResource[i].Get(),
                m_csuDescriptors->GetCpuHandle(SSAOCSUDesc::UAVHighQualityStart + i),
                m_highQualityResource[i]->GetDesc().Format
            );
        }
    }

    // Linear depth.
    {
        CreateTexture2DSRV(
            device,
            m_linearDepthResource.Get(),
            m_csuDescriptors->GetCpuHandle(SSAOCSUDesc::SRVLinearDepth),
            m_linearDepthResource->GetDesc().Format
        );

        CreateTexture2DUAV(
            device,
            m_linearDepthResource.Get(),
            m_csuDescriptors->GetCpuHandle(SSAOCSUDesc::UAVLinearDepth),
            m_linearDepthResource->GetDesc().Format
        );
    }

    // GBuffer.
    {
        CreateRTV(
            device,
            m_gBufferResource.Get(),
            m_rtvDescriptors->GetCpuHandle(SSAORTVDesc::RTVGBuffer)
        );

        CreateTexture2DArrSRV(
            device,
            m_gBufferResource.Get(),
            m_csuDescriptors->GetCpuHandle(SSAOCSUDesc::SRVGBuffer),
            m_gBufferResource->GetDesc().DepthOrArraySize,
            m_gBufferResource->GetDesc().Format
        );

        CreateDSV(
            device,
            m_dBufferResource.Get(),
            m_dsvDescriptors->GetCpuHandle(SSAODSVDesc::DSVGBuffer)
        );

        {
            CreateTexture2DSRV(
                device,
                m_dBufferResource.Get(),
                m_csuDescriptors->GetCpuHandle(SSAOCSUDesc::SRVDepth),
                DXGI_FORMAT_R32_FLOAT
            );
        }
    }

    // Final output.
    {
        CreateTexture2DSRV(
            device,
            m_ssaoResources.Get(),
            m_csuDescriptors->GetCpuHandle(SSAOCSUDesc::SRVSSAO),
            m_ssaoResources->GetDesc().Format
        );

        CreateTexture2DUAV(
            device,
            m_ssaoResources.Get(),
            m_csuDescriptors->GetCpuHandle(SSAOCSUDesc::UAVSSAO),
            m_ssaoResources->GetDesc().Format
        );

        CreateTexture2DSRV(
            device,
            m_outFrameResources.Get(),
            m_csuDescriptors->GetCpuHandle(SSAOCSUDesc::SRVOutFrame),
            m_outFrameResources->GetDesc().Format
        );

        CreateTexture2DUAV(
            device,
            m_outFrameResources.Get(),
            m_csuDescriptors->GetCpuHandle(SSAOCSUDesc::UAVOutFrame),
            m_outFrameResources->GetDesc().Format
        );
    }

    // Samplers.
    {
        CreateTexture2DSampler(
            device,
            m_samplerDescriptors->GetCpuHandle(SSAOSamplerDesc::SamplerLinearWrap),
            D3D12_FILTER_MIN_MAG_MIP_LINEAR,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP
        );

        float colorBlack[] = { 0.f, 0.f, 0.f, 0.f };
        CreateTexture2DSampler(
            device,
            m_samplerDescriptors->GetCpuHandle(SSAOSamplerDesc::SamplerLinearBorder),
            D3D12_FILTER_MIN_MAG_MIP_LINEAR,
            D3D12_TEXTURE_ADDRESS_MODE_BORDER,
            D3D12_TEXTURE_ADDRESS_MODE_BORDER,
            D3D12_TEXTURE_ADDRESS_MODE_BORDER,
            colorBlack,
            D3D12_COMPARISON_FUNC_LESS_EQUAL,
            16
        );

        CreateTexture2DSampler(
            device,
            m_samplerDescriptors->GetCpuHandle(SSAOSamplerDesc::SamplerLinearClamp),
            D3D12_FILTER_MIN_MAG_MIP_LINEAR,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP
        );

        float colorWhite[] = { 1.f, 1.f, 1.f, 1.f };
        CreateTexture2DSampler(
            device,
            m_samplerDescriptors->GetCpuHandle(SSAOSamplerDesc::SamplerPointClamp),
            D3D12_FILTER_MIN_MAG_MIP_POINT,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP
        );
    }
}

void SSAO::Setup(std::shared_ptr<DX::DeviceResources> pDeviceResources)
{
    // Run super class setup.
    Lighting::Setup(pDeviceResources);

    // Create raytracing interfaces: raytracing device and commandlist.
    CreateRootSignatures();

    // Create pipelines.
    SetupPipelines();

    // Allocate heaps.
    CreateDescriptorHeaps();
}

void SSAO::BindSRVGraphics(D3D12_GPU_DESCRIPTOR_HANDLE* srvs, unsigned int count)
{
    auto commandList = m_deviceResources->GetCommandList();

    for (unsigned int i = 0; i < count; i++)
    {
        if (srvs[i].ptr)
        {
            commandList->SetGraphicsRootDescriptorTable(
                SSAORootSig::RootSRVBeginSlot + i,
                srvs[i]
            );
        }
    }
}

void SSAO::BindUAVGraphics(D3D12_GPU_DESCRIPTOR_HANDLE* uavs, unsigned int count)
{
    auto commandList = m_deviceResources->GetCommandList();

    for (unsigned int i = 0; i < count; i++)
    {
        if (uavs[i].ptr)
        {
            commandList->SetGraphicsRootDescriptorTable(
                SSAORootSig::RootUAVBeginSlot + i,
                uavs[i]
            );
        }
    }
}

void SSAO::BindSRVCompute(D3D12_GPU_DESCRIPTOR_HANDLE* srvs, unsigned int count)
{
    auto commandList = m_deviceResources->GetCommandList();

    for (unsigned int i = 0; i < count; i++)
    {
        if (srvs[i].ptr)
        {
            commandList->SetComputeRootDescriptorTable(
                SSAORootSig::RootSRVBeginSlot + i,
                srvs[i]
            );
        }
    }
}

void SSAO::BindUAVCompute(D3D12_GPU_DESCRIPTOR_HANDLE* uavs, unsigned int count)
{
    auto commandList = m_deviceResources->GetCommandList();

    for (unsigned int i = 0; i < count; i++)
    {
        if (uavs[i].ptr)
        {
            commandList->SetComputeRootDescriptorTable(
                SSAORootSig::RootUAVBeginSlot + i,
                uavs[i]
            );
        }
    }
}

void SSAO::UpdateSSAOConstant(
    void* pConstData,
    const unsigned int width,
    const unsigned int height,
    const unsigned int depth,
    const float tanHalfFovH)
{
    // The shaders are set up to sample a circular region within a 5-pixel radius.
    const float screenspaceDiameter = 10.0f;

    // SphereDiameter = CenterDepth * ThicknessMultiplier.  This will compute the thickness of a sphere centered
    // at a specific depth.  The ellipsoid scale can stretch a sphere into an ellipsoid, which changes the
    // characteristics of the AO.
    // TanHalfFovH:  Radius of sphere in depth units if its center lies at Z = 1
    // ScreenspaceDiameter:  Diameter of sample sphere in pixel units
    // ScreenspaceDiameter / BufferWidth:  Ratio of the screen width that the sphere actually covers
    // Note about the "2.0f * ":  Diameter = 2 * Radius
    float thicknessMultiplier = 2.0f * tanHalfFovH * screenspaceDiameter / width;

    if (depth == 1)
        thicknessMultiplier *= 2.0f;

    // This will transform a depth value from [0, thickness] to [0, 1].
    float inverseRangeFactor = 1.f / thicknessMultiplier;

    AlignedSSAORenderConstantBuffer ssaoCB;

    // The thicknesses are smaller for all off-center samples of the sphere.  Compute thicknesses relative
    // to the center sample.
    {
        unsigned int offset = 0;
        for (unsigned int i = 0; i < _countof(ssaoCB.constants.invThicknessTable); i++)
        {
            ssaoCB.constants.invThicknessTable[i].x = inverseRangeFactor / m_sampleThickness[offset];
            offset++;

            ssaoCB.constants.invThicknessTable[i].y = inverseRangeFactor / m_sampleThickness[offset];
            offset++;

            ssaoCB.constants.invThicknessTable[i].z = inverseRangeFactor / m_sampleThickness[offset];
            offset++;

            ssaoCB.constants.invThicknessTable[i].w = inverseRangeFactor / m_sampleThickness[offset];
            offset++;
        }
    }

    // These are the weights that are multiplied against the samples because not all samples are
    // equally important.  The farther the sample is from the center location, the less they matter.
    // We use the thickness of the sphere to determine the weight.  The scalars in front are the number
    // of samples with this weight because we sum the samples together before multiplying by the weight,
    // so as an aggregate all of those samples matter more.  After generating this table, the weights
    // are normalized.
    {
        ssaoCB.constants.sampleWeightTable[0] = {
            4.0f * m_sampleThickness[0],	// Axial
            4.0f * m_sampleThickness[1],	// Axial
            4.0f * m_sampleThickness[2],	// Axial
            4.0f * m_sampleThickness[3]     // Axial
        };

        ssaoCB.constants.sampleWeightTable[1] = {
            4.0f * m_sampleThickness[4],	// Diagonal
            8.0f * m_sampleThickness[5],	// L-shaped
            8.0f * m_sampleThickness[6],	// L-shaped
            8.0f * m_sampleThickness[7]     // L-shaped
        };

        ssaoCB.constants.sampleWeightTable[2] = {
            4.0f * m_sampleThickness[8],	// Diagonal
            8.0f * m_sampleThickness[9],	// L-shaped
            8.0f * m_sampleThickness[10],	// L-shaped
            4.0f * m_sampleThickness[11]	// Diagonal
        };
    }

#ifndef SAMPLE_EXHAUSTIVELY
    ssaoCB.constants.sampleWeightTable[0].x = 0.0f;
    ssaoCB.constants.sampleWeightTable[0].z = 0.0f;
    ssaoCB.constants.sampleWeightTable[1].y = 0.0f;
    ssaoCB.constants.sampleWeightTable[1].w = 0.0f;
    ssaoCB.constants.sampleWeightTable[2].y = 0.0f;
#endif

    // Normalize the weights by dividing by the sum of all weights
    {
        float netWeight = 0;
        for (auto& el : ssaoCB.constants.sampleWeightTable)
        {
            netWeight += el.x;
            netWeight += el.y;
            netWeight += el.z;
            netWeight += el.w;
        }

        for (auto& el : ssaoCB.constants.sampleWeightTable)
        {
            el.x /= netWeight;
            el.y /= netWeight;
            el.z /= netWeight;
            el.w /= netWeight;
        }
    }

    // Compute final args.
    {
        ssaoCB.constants.invSliceDimension = {
            1.f / width,
            1.f / height
        };

        const float normalToDepthBrightnessEqualize = 2.f;
        ssaoCB.constants.normalMultiply = normalToDepthBrightnessEqualize * m_normalMultiply;
    }

    // Set Constant Buffer Data.
    {
        memcpy(pConstData, &ssaoCB, sizeof(ssaoCB));
    }
}

void SSAO::DispatchSSAO(
    D3D12_GPU_VIRTUAL_ADDRESS pConstantBuffer,
    D3D12_GPU_DESCRIPTOR_HANDLE pDestination,
    D3D12_GPU_DESCRIPTOR_HANDLE pDepthBuffer,
    D3D12_GPU_DESCRIPTOR_HANDLE pNormalBuffer,
    const unsigned int width,
    const unsigned int height,
    const unsigned int depth)
{
    auto commandList = m_deviceResources->GetCommandList();

    // Bind Constant Buffer.
    {
        commandList->SetComputeRootConstantBufferView(
            SSAORootSig::RootSSAOConstSlot,
            pConstantBuffer);
    }

    // Bind SRV and UAV.
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handleSRV[] = {
            pDepthBuffer,
            pNormalBuffer
        };
        BindSRVCompute(handleSRV, _countof(handleSRV));

        D3D12_GPU_DESCRIPTOR_HANDLE handleUAV[] = {
            pDestination
        };
        BindUAVCompute(handleUAV, _countof(handleUAV));
    }

    // Dispatch.
    {
        if (depth == 1)
            commandList->Dispatch(GetNumGrps(width, 16), GetNumGrps(height, 16), 1);
        else
            commandList->Dispatch(GetNumGrps(width, 8), GetNumGrps(height, 8), depth);
    }
}

void SSAO::UpdateBlurAndUpsampleConstant(
    void* pConstData,
    const unsigned int lowWidth,
    const unsigned int lowHeight,
    const unsigned int highWidth,
    const unsigned int highHeight)
{
    auto output = m_deviceResources->GetOutputSize();

    auto screenWidth = float(output.right - output.left) * m_screenWidthScale;

    float blurTolerance = 1.f - powf(10.f, m_blurTolerance) * static_cast<float>(screenWidth) / (float)lowWidth;
    blurTolerance *= blurTolerance;
    float upsampleTolerance = powf(10.f, m_upsampleTolerance);
    float noiseFilterWeight = 1.f / (powf(10.f, m_noiseFilterTolerance) + upsampleTolerance);

    // Fill constant buffer.
    AlignedBlurAndUpscaleConstantBuffer blurUpscaleCB;
    {
        blurUpscaleCB.constants.invLowResolution = { 1.f / float(lowWidth), 1.f / float(lowHeight) };
        blurUpscaleCB.constants.invHighResolution = { 1.f / float(highWidth), 1.f / float(highHeight) };
        blurUpscaleCB.constants.noiseFilterStrength = { noiseFilterWeight };
        blurUpscaleCB.constants.stepSize = static_cast<float>(screenWidth) / float(lowWidth);
        blurUpscaleCB.constants.blurTolerance = blurTolerance;
        blurUpscaleCB.constants.upsampleTolerance = upsampleTolerance;
    }

    // Set Constant Buffer Data.
    {
        memcpy(pConstData, &blurUpscaleCB, sizeof(blurUpscaleCB));
    }
}

void SSAO::DispatchBlurAndUpsample(
    D3D12_GPU_VIRTUAL_ADDRESS pConstantBuffer,
    D3D12_GPU_DESCRIPTOR_HANDLE pDestination,
    D3D12_GPU_DESCRIPTOR_HANDLE pLowResDepthBuffer,
    D3D12_GPU_DESCRIPTOR_HANDLE pHighResDepthBuffer,
    D3D12_GPU_DESCRIPTOR_HANDLE pinterleavedBuffer,
    D3D12_GPU_DESCRIPTOR_HANDLE pHighResBuffer,
    D3D12_GPU_DESCRIPTOR_HANDLE pHighQualityBuffer,
    const unsigned int highWidth,
    const unsigned int highHeight)
{
    auto commandList = m_deviceResources->GetCommandList();

    // Bind Constant Buffer.
    {
        commandList->SetComputeRootConstantBufferView(
            SSAORootSig::RootSSAOConstSlot,
            pConstantBuffer);
    }

    // Bind SRV and UAV.
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handleSRV[] = {
            pLowResDepthBuffer,
            pHighResDepthBuffer,
            pinterleavedBuffer,
            pHighQualityBuffer,
            pHighResBuffer
        };
        BindSRVCompute(handleSRV, _countof(handleSRV));

        D3D12_GPU_DESCRIPTOR_HANDLE handleUAV[] = {
            pDestination
        };
        BindUAVCompute(handleUAV, _countof(handleUAV));
    }

    // Dispatch.
    {
        commandList->Dispatch(GetNumGrps(highWidth + 2, 16), GetNumGrps(highHeight + 2, 16), 1);
    }
}

void SSAO::Run(ID3D12Resource* pSceneConstantResource)
{
    auto commandList = m_deviceResources->GetCommandList();
    auto renderTarget = m_deviceResources->GetRenderTarget();

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"SSAO");

    // Clear DSV.
    commandList->ClearDepthStencilView(
        m_dsvDescriptors->GetCpuHandle(SSAODSVDesc::DSVGBuffer),
        D3D12_CLEAR_FLAG_DEPTH,
        1.f,
        0,
        0,
        nullptr);

    // Setup screenspace.
    auto viewport = m_deviceResources->GetScreenViewport();
    viewport.Width *= m_screenWidthScale;

    commandList->RSSetViewports(1, &viewport);
    D3D12_RECT sissorRect = m_deviceResources->GetScissorRect();
    commandList->RSSetScissorRects(1, &sissorRect);

    commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    commandList->SetComputeRootSignature(m_rootSignature.Get());

    ID3D12DescriptorHeap* ppHeaps[] = { m_csuDescriptors->Heap(), m_samplerDescriptors->Heap() };
    commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // Set graphics tables.
    {
        commandList->SetGraphicsRootConstantBufferView(
            SSAORootSig::RootSceneConstSlot,
            pSceneConstantResource->GetGPUVirtualAddress());
    }

    // Set compute tables.
    {
        commandList->SetComputeRootConstantBufferView(
            SSAORootSig::RootSceneConstSlot,
            pSceneConstantResource->GetGPUVirtualAddress());
        commandList->SetComputeRootDescriptorTable(
            SSAORootSig::RootSamplerSlot,
            m_samplerDescriptors->GetGpuHandle(SSAOSamplerDesc::SamplerLinearWrap));
    }

    // Phase 1: Render GBuffer.
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render GBuffer");
    {
        D3D12_RESOURCE_BARRIER startBarrier[] = 
        {
            CD3DX12_RESOURCE_BARRIER::Transition(m_gBufferResource.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
        };

        commandList->ResourceBarrier(_countof(startBarrier), startBarrier);
        commandList->SetPipelineState(m_gBufferResourcePipelineState.Get());

        D3D12_CPU_DESCRIPTOR_HANDLE rtvDescHandle = m_rtvDescriptors->GetCpuHandle(SSAORTVDesc::RTVGBuffer);
        D3D12_CPU_DESCRIPTOR_HANDLE dsvDescHandle = m_dsvDescriptors->GetCpuHandle(SSAODSVDesc::DSVGBuffer);
        commandList->OMSetRenderTargets(1, &rtvDescHandle, FALSE, &dsvDescHandle);

        // Record commands.
        unsigned int offset = 0;
        for (auto el : *m_mesh)
        {
            D3D12_VERTEX_BUFFER_VIEW vbv;
            vbv.BufferLocation = el.vertexResource->GetGPUVirtualAddress();
            vbv.StrideInBytes = el.stride;
            vbv.SizeInBytes = static_cast<unsigned int>(el.numVertices * sizeof(Vertex));
            commandList->IASetVertexBuffers(0, 1, &vbv);

            D3D12_INDEX_BUFFER_VIEW ibv;
            ibv.BufferLocation = el.indexResource->GetGPUVirtualAddress();
            ibv.SizeInBytes = static_cast<unsigned int>(el.numIndices * sizeof(unsigned int));
            ibv.Format = DXGI_FORMAT_R32_UINT;
            commandList->IASetIndexBuffer(&ibv);

            commandList->SetGraphicsRootConstantBufferView(
                SSAORootSig::RootMaterialConstSlot,
                m_materialListCB[offset / SSAOPerObjectCSUDesc::CSUCount]->GetGPUVirtualAddress());

            D3D12_GPU_DESCRIPTOR_HANDLE handleSRV[] = {
                m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVPerObjectStart + SSAOPerObjectCSUDesc::SRVDiffuse + offset),
                m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVPerObjectStart + SSAOPerObjectCSUDesc::SRVSpecular + offset),
                m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVPerObjectStart + SSAOPerObjectCSUDesc::SRVNormal + offset)
            };
            BindSRVGraphics(handleSRV, _countof(handleSRV));

            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            commandList->DrawIndexedInstanced(el.numIndices, 2, 0, 0, 0);

            offset += SSAOPerObjectCSUDesc::CSUCount;
        }

        // Convert RTV and DBV to shader readable resources.
        D3D12_RESOURCE_BARRIER RTVToGBufferBarrier[] = {
            CD3DX12_RESOURCE_BARRIER::Transition(m_gBufferResource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
            CD3DX12_RESOURCE_BARRIER::Transition(m_dBufferResource.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
        };
        commandList->ResourceBarrier(_countof(RTVToGBufferBarrier), RTVToGBufferBarrier);
    }
    PIXEndEvent(commandList);

    // Phase 2: Decompress, linearize, downsample, and deinterleave the depth buffer.
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Decompress and Downsample.");
    {
        {
            // Setup SRV and UAV
            {
                D3D12_GPU_DESCRIPTOR_HANDLE handleSRV[] = {
                    m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVDepth),
                    m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVGBuffer)
                };
                BindSRVCompute(handleSRV, _countof(handleSRV));

                D3D12_GPU_DESCRIPTOR_HANDLE handleUAV[] = {
                    m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::UAVLinearDepth),
                    m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::UAVDepthDownsizeStart),
                    m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::UAVDepthTiledStart),
                    m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::UAVDepthDownsizeStart + 1),
                    m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::UAVDepthTiledStart + 1),
                    m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::UAVNormalDownsizeStart),
                    m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::UAVNormalTiledStart),
                    m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::UAVNormalDownsizeStart + 1),
                    m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::UAVNormalTiledStart + 1)
                };
                BindUAVCompute(handleUAV, _countof(handleUAV));
            }

            commandList->SetPipelineState(m_SSAOPrepareDepthBuffers1PipelineState.Get());

            auto desc = m_depthTiledResource[1]->GetDesc();
            commandList->Dispatch(GetNumGrps(unsigned int(desc.Width) * 8, 8), GetNumGrps(desc.Height * 8, 8), 1);
        }

        {
            // Setup SRV and UAV
            {
                D3D12_RESOURCE_BARRIER startBarrier[] = {
                    CD3DX12_RESOURCE_BARRIER::Transition(m_depthDownsizeResource[1].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
                    CD3DX12_RESOURCE_BARRIER::Transition(m_normalDownsizeResource[1].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
                };
                commandList->ResourceBarrier(_countof(startBarrier), startBarrier);

                D3D12_GPU_DESCRIPTOR_HANDLE handleSRV[] = {
                    m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVDepthDownsizeStart + 1),
                    m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVNormalDownsizeStart + 1)
                };
                BindSRVCompute(handleSRV, _countof(handleSRV));

                D3D12_GPU_DESCRIPTOR_HANDLE handleUAV[] = {
                    m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::UAVDepthDownsizeStart + 2),
                    m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::UAVDepthTiledStart + 2),
                    m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::UAVDepthDownsizeStart + 3),
                    m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::UAVDepthTiledStart + 3),
                    m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::UAVNormalDownsizeStart + 2),
                    m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::UAVNormalTiledStart + 2),
                    m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::UAVNormalDownsizeStart + 3),
                    m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::UAVNormalTiledStart + 3)
                };
                BindUAVCompute(handleUAV, _countof(handleUAV));

                D3D12_RESOURCE_BARRIER endBarrier[] = {
                    CD3DX12_RESOURCE_BARRIER::Transition(m_depthDownsizeResource[1].Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
                    CD3DX12_RESOURCE_BARRIER::Transition(m_normalDownsizeResource[1].Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
                };
                commandList->ResourceBarrier(_countof(endBarrier), endBarrier);
            }

            commandList->SetPipelineState(m_SSAOPrepareDepthBuffers2PipelineState.Get());

            auto desc = m_depthTiledResource[3]->GetDesc();
            commandList->Dispatch(GetNumGrps(unsigned int(desc.Width) * 8, 8), GetNumGrps(desc.Height * 8, 8), 1);
        }
    }
    PIXEndEvent(commandList);

    // Phase 3: Render SSAO for each sub-tile.
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render SSAO.");
    {
        PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Depth Tiled.");
        {
            commandList->SetPipelineState(m_SSAOPRender1PipelineState.Get());

            for (unsigned int i = 0; i < _countof(m_depthTiledResource); i++)
            {
                D3D12_RESOURCE_BARRIER startBarrier[] = {
                    CD3DX12_RESOURCE_BARRIER::Transition(m_depthTiledResource[i].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
                    CD3DX12_RESOURCE_BARRIER::Transition(m_normalTiledResource[i].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
                };

                commandList->ResourceBarrier(_countof(startBarrier), startBarrier);

                {
                    auto desc = m_depthTiledResource[i]->GetDesc();
                    DispatchSSAO(
                        m_mappedDepthTiledSSAORenderConstantResource[i]->GetGPUVirtualAddress(),
                        m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::UAVMergedStart + i),
                        m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVDepthTiledStart + i),
                        m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVNormalTiledStart + i),
                        unsigned int(desc.Width),
                        desc.Height,
                        desc.DepthOrArraySize
                    );
                }
            }
        }
        PIXEndEvent(commandList);

        PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Depth Downsize.");
        {
            commandList->SetPipelineState(m_SSAOPRender2PipelineState.Get());

            for (unsigned int i = 0; i < _countof(m_depthDownsizeResource); i++)
            {
                D3D12_RESOURCE_BARRIER startBarrier[] = {
                    CD3DX12_RESOURCE_BARRIER::Transition(m_depthDownsizeResource[i].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
                    CD3DX12_RESOURCE_BARRIER::Transition(m_normalDownsizeResource[i].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
                };
                commandList->ResourceBarrier(_countof(startBarrier), startBarrier);

                {
                    auto desc = m_highQualityResource[i]->GetDesc();
                    DispatchSSAO(
                        m_mappedHighQualitySSAORenderConstantResource[i]->GetGPUVirtualAddress(),
                        m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::UAVHighQualityStart + i),
                        m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVDepthDownsizeStart + i),
                        m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVNormalDownsizeStart + i),
                        unsigned int(desc.Width),
                        unsigned int(desc.Height),
                        desc.DepthOrArraySize
                    );
                }
            }
        }
        PIXEndEvent(commandList);
    }
    PIXEndEvent(commandList);

    // Phase 4: Iteratively blur and upsample, combining each result.
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Blur and Upsample.");
    {
        commandList->SetPipelineState(m_SSAOBlurUpsamplePreMinBlendOutPipelineState.Get());

        // Every iteration is twice larger in every dimension.
        for (unsigned int i = _countof(m_depthDownsizeResource) - 1; i != std::numeric_limits<unsigned int>::max(); i--)
        {
            // Edge cases (first and last) are taken into account.
            {
                if (i == _countof(m_depthDownsizeResource) - 1)
                {
                    D3D12_RESOURCE_BARRIER startBarrier[] = {
                        CD3DX12_RESOURCE_BARRIER::Transition(m_mergedResource[i].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
                        CD3DX12_RESOURCE_BARRIER::Transition(m_mergedResource[i - 1].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
                        CD3DX12_RESOURCE_BARRIER::Transition(m_highQualityResource[i].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
                    };
                    commandList->ResourceBarrier(_countof(startBarrier), startBarrier);

                    auto desc = m_depthDownsizeResource[i - 1]->GetDesc();
                    DispatchBlurAndUpsample(
                        m_mappedBlurAndUpscaleConstantResource[i]->GetGPUVirtualAddress(),
                        m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::UAVSmoothStart + i - 1),
                        m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVDepthDownsizeStart + i),
                        m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVDepthDownsizeStart + i - 1),
                        m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVMergedStart + i),
                        m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVMergedStart + i - 1),
                        m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVHighQualityStart + i),
                        unsigned int(desc.Width),
                        unsigned int(desc.Height)
                    );
                }
                else if (i == 0)
                {
                    // Since this is the last index of the loop, setting a new pipeline does not effect any other cases.
                    commandList->SetPipelineState(m_SSAOBlurUpsamplePreMinPipelineState.Get());

                    D3D12_RESOURCE_BARRIER startBarrier[] = {
                        CD3DX12_RESOURCE_BARRIER::Transition(m_smoothResource[i].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
                        CD3DX12_RESOURCE_BARRIER::Transition(m_linearDepthResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
                        CD3DX12_RESOURCE_BARRIER::Transition(m_highQualityResource[i].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
                    };
                    commandList->ResourceBarrier(_countof(startBarrier), startBarrier);

                    auto desc = m_linearDepthResource->GetDesc();
                    DispatchBlurAndUpsample(
                        m_mappedBlurAndUpscaleConstantResource[i]->GetGPUVirtualAddress(),
                        m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::UAVSSAO),
                        m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVDepthDownsizeStart + i),
                        m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVLinearDepth),
                        m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVSmoothStart + i),
                        { reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS>(nullptr) },
                        m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVHighQualityStart + i),
                        unsigned int(desc.Width),
                        unsigned int(desc.Height)
                    );
                }
                else
                {
                    D3D12_RESOURCE_BARRIER startBarrier[] = {
                        CD3DX12_RESOURCE_BARRIER::Transition(m_smoothResource[i].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
                        CD3DX12_RESOURCE_BARRIER::Transition(m_mergedResource[i - 1].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
                        CD3DX12_RESOURCE_BARRIER::Transition(m_highQualityResource[i].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
                    };
                    commandList->ResourceBarrier(_countof(startBarrier), startBarrier);

                    auto desc = m_depthDownsizeResource[i - 1]->GetDesc();
                    DispatchBlurAndUpsample(
                        m_mappedBlurAndUpscaleConstantResource[i]->GetGPUVirtualAddress(),
                        m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::UAVSmoothStart + i - 1),
                        m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVDepthDownsizeStart + i),
                        m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVDepthDownsizeStart + i - 1),
                        m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVSmoothStart + i),
                        m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVMergedStart + i - 1),
                        m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVHighQualityStart + i),
                        unsigned int(desc.Width),
                        unsigned int(desc.Height)
                    );
                }
            }
        }
    }
    PIXEndEvent(commandList);

    // Phase 5: Render.
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render.");
    {
        // Setup SRV and UAV
        {
            {
                D3D12_RESOURCE_BARRIER startBarrier[] = {
                    CD3DX12_RESOURCE_BARRIER::Transition(m_ssaoResources.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
                };
                commandList->ResourceBarrier(_countof(startBarrier), startBarrier);
            }

            D3D12_GPU_DESCRIPTOR_HANDLE handleSRV[] = {
                m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVSSAO),
                m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVGBuffer),
                m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVDepth),
            };
            BindSRVCompute(handleSRV, _countof(handleSRV));

            D3D12_GPU_DESCRIPTOR_HANDLE handleUAV[] = {
                m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::UAVOutFrame)
            };
            BindUAVCompute(handleUAV, _countof(handleUAV));
        }

        commandList->SetPipelineState(m_SSAOGBufferRenderPipelineState.Get());

        commandList->Dispatch(GetNumGrps(bufferWidth[1] * 8, 8), GetNumGrps(bufferHeight[1] * 8, 8), 1);
    }
    PIXEndEvent(commandList);

    // Phase 6: Copy to BackBuffer.
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Copy to Back Buffer.");
    {
        if (m_screenWidthScale == 1.f)
        {
            D3D12_RESOURCE_BARRIER startBarrier[] = {
                CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST),
                CD3DX12_RESOURCE_BARRIER::Transition(m_outFrameResources.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE)
            };
            commandList->ResourceBarrier(_countof(startBarrier), startBarrier);

            commandList->CopyResource(renderTarget, m_outFrameResources.Get());

            D3D12_RESOURCE_BARRIER endBarrier[] = {
                CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET),
                CD3DX12_RESOURCE_BARRIER::Transition(m_outFrameResources.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
            };
            commandList->ResourceBarrier(_countof(endBarrier), endBarrier);
        }
        else
        {
            D3D12_RESOURCE_BARRIER startBarrier[] = {
                CD3DX12_RESOURCE_BARRIER::Transition(m_outFrameResources.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
            };
            commandList->ResourceBarrier(_countof(startBarrier), startBarrier);
            CD3DX12_CPU_DESCRIPTOR_HANDLE renderTargetView = m_deviceResources->GetRenderTargetView();
            commandList->OMSetRenderTargets(
                1,
                &renderTargetView,
                FALSE,
                nullptr
            );

            D3D12_VIEWPORT screenViewport = m_deviceResources->GetScreenViewport();
            commandList->RSSetViewports(1, &screenViewport);

            {
                m_basicEffect->SetTexture(
                    m_csuDescriptors->GetGpuHandle(SSAOCSUDesc::SRVOutFrame),
                    m_samplerDescriptors->GetGpuHandle(SSAOSamplerDesc::SamplerLinearClamp)
                );
                m_basicEffect->Apply(commandList);
                m_primitiveBatch->Begin(commandList);
                {
                    // Left Quad.
                    {
                        m_primitiveBatch->DrawQuad(
                            { XMFLOAT3(0.f, 0.f, 0.f),{ 0.f, 0.f } },
                            { XMFLOAT3(m_screenWidthScale, 0.f, 0.f),{ 1.f, 0.f } },
                            { XMFLOAT3(m_screenWidthScale, 1.f, 0.f),{ 1.f, 1.f } },
                            { XMFLOAT3(0.f, 1.f, 0.f),{ 0.f, 1.f } }
                        );
                    }
                }
                m_primitiveBatch->End();
            }

            D3D12_RESOURCE_BARRIER endBarrier[] = {
                CD3DX12_RESOURCE_BARRIER::Transition(m_outFrameResources.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
            };
            commandList->ResourceBarrier(_countof(endBarrier), endBarrier);
        }
    }
    PIXEndEvent(commandList);

    // Restore original state.
    {
        std::vector<D3D12_RESOURCE_BARRIER> restoreBufferBarrier = {
            CD3DX12_RESOURCE_BARRIER::Transition(m_dBufferResource.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE),
            CD3DX12_RESOURCE_BARRIER::Transition(m_linearDepthResource.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
            CD3DX12_RESOURCE_BARRIER::Transition(m_ssaoResources.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
        };
        {
            for (auto& el : m_depthDownsizeResource)
            {
                restoreBufferBarrier.push_back(
                    CD3DX12_RESOURCE_BARRIER::Transition(el.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
                );
            }

            for (auto& el : m_depthTiledResource)
            {
                restoreBufferBarrier.push_back(
                    CD3DX12_RESOURCE_BARRIER::Transition(el.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
                );
            }

            for (auto& el : m_normalDownsizeResource)
            {
                restoreBufferBarrier.push_back(
                    CD3DX12_RESOURCE_BARRIER::Transition(el.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
                );
            }

            for (auto& el : m_normalTiledResource)
            {
                restoreBufferBarrier.push_back(
                    CD3DX12_RESOURCE_BARRIER::Transition(el.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
                );
            }

            for (auto& el : m_highQualityResource)
            {
                restoreBufferBarrier.push_back(
                    CD3DX12_RESOURCE_BARRIER::Transition(el.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
                );
            }

            for (auto& el : m_mergedResource)
            {
                restoreBufferBarrier.push_back(
                    CD3DX12_RESOURCE_BARRIER::Transition(el.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
                );
            }

            for (auto& el : m_smoothResource)
            {
                restoreBufferBarrier.push_back(
                    CD3DX12_RESOURCE_BARRIER::Transition(el.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
                );
            }
        }

        commandList->ResourceBarrier(unsigned int(restoreBufferBarrier.size()), restoreBufferBarrier.data());
    }

    PIXEndEvent(commandList);
}

void SSAO::SetMesh(std::shared_ptr<Mesh> pMesh)
{
    auto device = m_deviceResources->GetD3DDevice();

    // Store the args.
    {
        m_mesh = pMesh;
    }

    // Clear data.
    {
        m_materialListCB.clear();
    }

    // Create the texture handles.
    if (pMesh)
    {
        AlignedMaterialConstantBuffer cb;

        size_t offset = 0;
        for (auto el : *m_mesh)
        {
            auto& material = m_mesh->getModel()->materials[el.matID];

            // Set CBV.
            cb.constants.ambient = material.ambientColor;
            cb.constants.diffuse = material.diffuseColor;
            cb.constants.specular = material.specularColor;
            cb.constants.isDiffuseTexture = (material.diffuseTextureIndex != -1);
            cb.constants.isSpecularTexture = (material.specularTextureIndex != -1);
            cb.constants.isNormalTexture = (material.normalTextureIndex != -1);

            // Constant.
            ComPtr<ID3D12Resource> resource;
            AllocateUploadBuffer(
                device,
                &cb,
                sizeof(cb),
                &resource
            );
            m_materialListCB.push_back(resource);

            // Diffuse.
            m_mesh->SetTextureSRV(
                device,
                m_csuDescriptors->GetCpuHandle(
                    SSAOCSUDesc::SRVPerObjectStart + offset),
                material.diffuseTextureIndex
            );
            offset++;

            // Specular.
            m_mesh->SetTextureSRV(
                device,
                m_csuDescriptors->GetCpuHandle(
                    SSAOCSUDesc::SRVPerObjectStart + offset),
                material.specularTextureIndex
            );
            offset++;

            // Normal.
            m_mesh->SetTextureSRV(
                device,
                m_csuDescriptors->GetCpuHandle(
                    SSAOCSUDesc::SRVPerObjectStart + offset),
                material.normalTextureIndex
            );
            offset++;
        }
    }
}

void SSAO::UpdateConstants()
{
    for (unsigned int i = _countof(m_depthDownsizeResource) - 1; i != std::numeric_limits<unsigned int>::max(); i--)
    {
        auto descLow = m_depthDownsizeResource[i]->GetDesc();

        auto descHigh = (i != 0) ?
            m_depthDownsizeResource[i - 1]->GetDesc()
            : m_linearDepthResource->GetDesc();

        UpdateBlurAndUpsampleConstant(
            m_mappedBlurAndUpscaleConstantData[i],
            unsigned int(descLow.Width),
            descLow.Height,
            unsigned int(descHigh.Width),
            descHigh.Height
        );
    }
}

void SSAO::OnSizeChanged()
{
    // Allocate resouces (dependent on frame).
    {
        // Create resources.
        CreateResources();

        // Bind resources.
        BindResources();
    }

    // Update blur and upsample constant.
    UpdateConstants();
}

void SSAO::OnOptionUpdate(std::shared_ptr<Menus> pMenu)
{
    // Update options.
    m_noiseFilterTolerance = float(pMenu->m_ssaoNoiseFilterTolerance.Value());
    m_blurTolerance = float(pMenu->m_ssaoBlurTolerance.Value());
    m_upsampleTolerance = float(pMenu->m_ssaoUpsampleTolerance.Value());
    m_normalMultiply = float(pMenu->m_ssaoNormalMultiply.Value());

    UpdateConstants();
}

void SSAO::OnCameraChanged(XMMATRIX& world, XMMATRIX& view, XMMATRIX& projection)
{
    UNREFERENCED_PARAMETER(world);
    UNREFERENCED_PARAMETER(view);
    
    // Load first element of projection matrix which is the cotangent of the horizontal FOV divided by 2.
    float fovTangent = 1.f / XMVectorGetByIndex(projection.r[0], 0);

    // Update constant buffers.
    for (unsigned int i = 0; i < _countof(m_depthTiledResource); i++)
    {
        auto desc = m_depthTiledResource[i]->GetDesc();
        UpdateSSAOConstant(
            m_mappedDepthTiledSSAORenderConstantData[i],
            unsigned int(desc.Width),
            desc.Height,
            desc.DepthOrArraySize,
            fovTangent
        );
    }

    for (unsigned int i = 0; i < _countof(m_depthTiledResource); i++)
    {
        auto desc = m_highQualityResource[i]->GetDesc();
        UpdateSSAOConstant(
            m_mappedHighQualitySSAORenderConstantData[i],
            unsigned int(desc.Width),
            desc.Height,
            desc.DepthOrArraySize,
            fovTangent
        );
    }
}
