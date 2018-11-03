//--------------------------------------------------------------------------------------
// BokehEffect12.cpp
//
// Demonstrates how to render depth of field using point sprites.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "BokehEffect12.h"
#include "shadersettings.h"

using namespace ATG;
using namespace DirectX;

namespace ATG
{
    BokehEffect::BokehEffect(
        ID3D12Device* device,
        DXGI_FORMAT format,
        DirectX::GraphicsMemory* graphicsMemory,
        DirectX::ResourceUploadBatch& batch)
        : m_graphicsMemory(graphicsMemory)
        , m_format(format)
        , m_width(0)
        , m_height(0)
        , m_srvHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 128, e_iSRVHeapEnd)
        , m_rtvHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 128)
    {
        CreateResources(device, batch);
        CreatePSO(device, format);
    }

    void BokehEffect::ResizeResources(ID3D12Device* device, int width, int height)
    {
        if (m_width == width && m_height == height)
        {
            return;
        }

        // DOF texture
        // contains multiple viewports
        m_width = width;
        m_height = height;

        UINT w = m_width;
        UINT h = 2 * m_height / FIRST_DOWNSAMPLE + m_height / (2 * FIRST_DOWNSAMPLE) + m_height / (4 * FIRST_DOWNSAMPLE);

        D3D12_CLEAR_VALUE clearFmtValue = CD3DX12_CLEAR_VALUE(m_format, DirectX::Colors::Transparent);
        D3D12_CLEAR_VALUE clearFP16Value = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R16G16B16A16_FLOAT, DirectX::Colors::Transparent);

        CreateColorTextureAndViews(device, w, h, m_format, m_DOFColorTexture.ReleaseAndGetAddressOf(), m_rtvHeap.GetCpuHandle(e_iRTVDOFColor), m_srvHeap.GetCpuHandle(e_iSRVDOFColor), &clearFmtValue);
        CreateColorTextureAndViews(device, m_width, m_height, DXGI_FORMAT_R16G16B16A16_FLOAT, m_sourceColorTextureRGBZCopy.ReleaseAndGetAddressOf(), m_rtvHeap.GetCpuHandle(e_iRTVSourceColorRGBZCopy), m_srvHeap.GetCpuHandle(e_iSRVSourceColorRGBZCopy), &clearFP16Value);
        CreateColorTextureAndViews(device, m_width / FIRST_DOWNSAMPLE, m_height / FIRST_DOWNSAMPLE, DXGI_FORMAT_R16G16B16A16_FLOAT, m_sourceColorTextureRGBZHalfCopy.ReleaseAndGetAddressOf(), m_rtvHeap.GetCpuHandle(e_iRTVSourceColorRGBZHalfCopy), m_srvHeap.GetCpuHandle(e_iSRVSourceColorRGBZHalfCopy), &clearFP16Value);

        m_DOFColorTexture->SetName(L"DOFColorTexture");
        m_sourceColorTextureRGBZCopy->SetName(L"SourceColorTextureRGBZCopy");
        m_sourceColorTextureRGBZHalfCopy->SetName(L"SourceColorTextureRGBZHalfCopy");
    }

    void BokehEffect::Render(
        ID3D12GraphicsCommandList* cmdList,
        ID3D12Resource* sceneTexture,
        D3D12_CPU_DESCRIPTOR_HANDLE srcColorSRV,
        D3D12_CPU_DESCRIPTOR_HANDLE srcDepthSRV,
        D3D12_CPU_DESCRIPTOR_HANDLE dstRTV,
        const DirectX::XMMATRIX& matInvProj,
        const Parameters& params,
        bool useDebugShader)
    {
        assert(srcColorSRV.ptr != 0);
        assert(srcDepthSRV.ptr != 0);
        assert(dstRTV.ptr != 0);

        ID3D12Device* device = nullptr;
        cmdList->GetDevice(IID_GRAPHICS_PPV_ARGS(&device));

        device->CopyDescriptorsSimple(1, m_srvHeap.GetCpuHandle(e_iSRVSrcColor), srcColorSRV, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        device->CopyDescriptorsSimple(1, m_srvHeap.GetCpuHandle(e_iSRVSrcDepth), srcDepthSRV, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        device->CopyDescriptorsSimple(1, m_srvHeap.GetCpuHandle(e_iSRVSrcDepth2), srcDepthSRV, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        device->CopyDescriptorsSimple(1, m_rtvHeap.GetCpuHandle(e_iRTVDst), dstRTV, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        StartRendering(cmdList, matInvProj, params);

        D3D12_VIEWPORT vpResult = { 0, 0, static_cast<FLOAT>(m_width), static_cast<FLOAT>(m_height),  0, 1 };
        D3D12_VIEWPORT vpResultHalf = { 0, 0, static_cast<FLOAT>(m_width) / 2.0f, static_cast<FLOAT>(m_height) / 2.0f,  0, 1 };
        D3D12_RECT scissorResult = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
        D3D12_RECT scissorResultHalf = { 0, 0, static_cast<LONG>(m_width) / 2, static_cast<LONG>(m_height) / 2 };

        // find iris texture weights
        {
            ScopedPixEvent setSize(cmdList, PIX_COLOR_DEFAULT, L"BokehEffect::Render - compute energy tex");

            ScopedBarrier s(cmdList, {
                CD3DX12_RESOURCE_BARRIER::Transition(m_scratchTex.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
            });

            TransitionResource(cmdList, m_irisTex.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ),
            TransitionResource(cmdList, m_energiesTex.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

            cmdList->SetComputeRootSignature(m_createEnergyTexRS.Get());
            cmdList->SetPipelineState(m_createEnergyTexPSO.Get());
            cmdList->SetComputeRootDescriptorTable(0, m_srvHeap.GetGpuHandle(e_iSRVIris));
            cmdList->SetComputeRootDescriptorTable(1, m_srvHeap.GetGpuHandle(e_iUAVScratch));

            cmdList->Dispatch(NUM_RADII_WEIGHTS / 8, NUM_RADII_WEIGHTS / 8, NUM_RADII_WEIGHTS);

            TransitionResource(cmdList, m_energiesTex.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }

        TransitionResource(cmdList, m_sourceColorTextureRGBZCopy.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
        TransitionResource(cmdList, m_DOFColorTexture.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
        TransitionResource(cmdList, m_sourceColorTextureRGBZHalfCopy.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);

        cmdList->ClearRenderTargetView(m_rtvHeap.GetCpuHandle(e_iRTVDOFColor), DirectX::Colors::Transparent, 0, nullptr);

        // copy out the source
        // this is 0.2 ms faster than CopyResource
        {
            ScopedPixEvent e(cmdList, PIX_COLOR_DEFAULT, L"BokehEffect::Render - copy out the source");

            TransitionResource(cmdList, sceneTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

            D3D12_CPU_DESCRIPTOR_HANDLE hRTVSourceColorRGBZCopy = m_rtvHeap.GetCpuHandle(e_iRTVSourceColorRGBZCopy);
            cmdList->OMSetRenderTargets(1, &hRTVSourceColorRGBZCopy, false, nullptr);
            cmdList->RSSetViewports(1, &vpResult);
            cmdList->RSSetScissorRects(1, &scissorResult);
            cmdList->SetGraphicsRootDescriptorTable(e_rootParameterPS0, m_srvHeap.GetGpuHandle(e_iSRVSrcColor));

            cmdList->SetPipelineState(m_createRGBZPSO.Get());
            cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            cmdList->DrawInstanced(3, 1, 0, 0);
        }

        // downsample
        {
            ScopedPixEvent e(cmdList, PIX_COLOR_DEFAULT, L"BokehEffect::Render - downsample");

            TransitionResource(cmdList, m_sourceColorTextureRGBZCopy.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            
            D3D12_CPU_DESCRIPTOR_HANDLE hRTVSourceColorRGBZHalfCopy = m_rtvHeap.GetCpuHandle(e_iRTVSourceColorRGBZHalfCopy);
            cmdList->OMSetRenderTargets(1, &hRTVSourceColorRGBZHalfCopy, false, nullptr);
            cmdList->RSSetViewports(1, &vpResultHalf);
            cmdList->RSSetScissorRects(1, &scissorResultHalf);
            cmdList->SetGraphicsRootDescriptorTable(e_rootParameterPS0, m_srvHeap.GetGpuHandle(e_iSRVSourceColorRGBZCopy));

            cmdList->SetPipelineState(m_downsampleRGBZPSO.Get());
            cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            cmdList->DrawInstanced(3, 1, 0, 0);
        }

        // prepare the multi-viewport dof render target
        {
            ScopedPixEvent e(cmdList, PIX_COLOR_DEFAULT, L"BokehEffect::Render - CoC DOF");

            TransitionResource(cmdList, m_sourceColorTextureRGBZHalfCopy.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

            D3D12_CPU_DESCRIPTOR_HANDLE hRTVDOFColor = m_rtvHeap.GetCpuHandle(e_iRTVDOFColor);
            cmdList->OMSetRenderTargets(1, &hRTVDOFColor, true, nullptr);
            cmdList->RSSetViewports(_countof(m_vpSplitOutput), m_vpSplitOutput);
            cmdList->RSSetScissorRects(_countof(m_scissorSplitOutput), m_scissorSplitOutput);

            // split into slices, do the CoC DOF
            cmdList->SetPipelineState(params.useFastShader ? m_quadPointFastPSO.Get() : m_quadPointPSO.Get());

            cmdList->SetGraphicsRootDescriptorTable(e_rootParameterVS0, m_srvHeap.GetGpuHandle(e_iSRVSourceColorRGBZHalfCopy));
            cmdList->SetGraphicsRootDescriptorTable(e_rootParameterGS0, m_srvHeap.GetGpuHandle(e_iSRVSourceColorRGBZHalfCopy));
            cmdList->SetGraphicsRootDescriptorTable(e_rootParameterPS0, m_srvHeap.GetGpuHandle(e_iSRVSrcColor));

            cmdList->OMSetBlendFactor(DirectX::Colors::Transparent);

            cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
            cmdList->DrawInstanced(m_width * m_height / (FIRST_DOWNSAMPLE * FIRST_DOWNSAMPLE * 2 * 2), 1, 0, 0);    // each GS can output up to 4 triangles
        }

        // combine the resulting viewports
        {
            ScopedPixEvent e(cmdList, PIX_COLOR_DEFAULT, L"BokehEffect::Render - Combine");

            TransitionResource(cmdList, m_DOFColorTexture.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            TransitionResource(cmdList, sceneTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
            
            cmdList->RSSetViewports(1, &vpResult);
            cmdList->RSSetScissorRects(1, &scissorResult);

            D3D12_CPU_DESCRIPTOR_HANDLE hRTVDst = m_rtvHeap.GetCpuHandle(e_iRTVDst);
            cmdList->OMSetRenderTargets(1, &hRTVDst, false, nullptr);

            // the 2 viewports are in one texture
            cmdList->SetGraphicsRootDescriptorTable(e_rootParameterPS0, m_srvHeap.GetGpuHandle(e_iSRVDOFColor));
            cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

            // Optionally set debug shader that shows viewports breakdown etc
            cmdList->SetPipelineState(useDebugShader ? m_recombinePSODebug.Get() : m_recombinePSO.Get());

            cmdList->DrawInstanced(3, 1, 0, 0);
        }

        TransitionResource(cmdList, m_irisTex.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
        TransitionResource(cmdList, m_energiesTex.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);

        TransitionResource(cmdList, m_sourceColorTextureRGBZCopy.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
        TransitionResource(cmdList, m_DOFColorTexture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
        TransitionResource(cmdList, m_sourceColorTextureRGBZHalfCopy.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
    }

    void BokehEffect::StartRendering(ID3D12GraphicsCommandList* cmdList, const XMMATRIX& matInvProj, const Parameters& params)
    {
        D3D12_RESOURCE_DESC dofTexDesc = m_DOFColorTexture->GetDesc();
        D3D12_RESOURCE_DESC irisTexDesc = m_irisTex->GetDesc();

        // output into several viewports
        float vpSx = float(m_width) / FIRST_DOWNSAMPLE;
        float vpSy = float(m_height) / FIRST_DOWNSAMPLE;
        float vpSx2 = float(m_width) / (2 * FIRST_DOWNSAMPLE);
        float vpSy2 = float(m_height) / (2 * FIRST_DOWNSAMPLE);
        float vpSx4 = float(m_width) / (4 * FIRST_DOWNSAMPLE);
        float vpSy4 = float(m_height) / (4 * FIRST_DOWNSAMPLE);

        D3D12_VIEWPORT vpOutput[6] =
        {
            // big ones one below another
            { 0, 0,    vpSx, vpSy,  0, 1 },
            { 0, vpSy, vpSx, vpSy,  0, 1 },

            // smaller ones along the bottom
            { 0,     vpSy * 2, vpSx2, vpSy2,  0, 1 },
            { vpSx2, vpSy * 2, vpSx2, vpSy2,  0, 1 },

            // smaller ones still along the bottom again
            { 0,     vpSy * 2 + vpSy2, vpSx4, vpSy4,  0, 1 },
            { vpSx4, vpSy * 2 + vpSy2, vpSx4, vpSy4,  0, 1 },
        };

        memcpy(m_vpSplitOutput, vpOutput, sizeof(vpOutput));
        for (int i = 0; i < _countof(m_vpSplitOutput); ++i)
        {
            m_scissorSplitOutput[i].left = static_cast<LONG>(m_vpSplitOutput[i].TopLeftX);
            m_scissorSplitOutput[i].top = static_cast<LONG>(m_vpSplitOutput[i].TopLeftY);
            m_scissorSplitOutput[i].right = m_scissorSplitOutput[i].left + static_cast<LONG>(m_vpSplitOutput[i].Width);
            m_scissorSplitOutput[i].bottom = m_scissorSplitOutput[i].top + static_cast<LONG>(m_vpSplitOutput[i].Height);
        }

        auto invProj = XMMatrixTranspose(matInvProj);

        BokehCB cb = { 0 };
        memcpy(&cb.mInvProj, &invProj, sizeof(invProj));
        cb.depthBufferSize[0] = static_cast<FLOAT>(m_width);
        cb.depthBufferSize[1] = static_cast<FLOAT>(m_height);
        cb.dofTexSize[0] = static_cast<FLOAT>(dofTexDesc.Width);
        cb.dofTexSize[1] = static_cast<FLOAT>(dofTexDesc.Height);
        cb.FNumber = params.FNumber;
        cb.focusLength = params.focusLength;
        cb.focalPlane = params.focalPlane;
        cb.maxCoCDiameterNear = params.maxCoCSizeNear;
        cb.maxCoCDiameterFar = params.maxCoCSizeFar;
        cb.irisTextureOffset = 0.5f / static_cast<FLOAT>(irisTexDesc.Width);
        cb.srcScreenSize[0] = static_cast<FLOAT>(m_width);
        cb.srcScreenSize[1] = static_cast<FLOAT>(m_height);
        cb.switchover1[0] = params.switchover1[0];
        cb.switchover1[1] = params.switchover1[1];
        cb.switchover2[0] = params.switchover2[0];
        cb.switchover2[1] = params.switchover2[1];
        cb.initialEnergyScale = params.initialEnergyScale;

        static_assert(_countof(vpOutput) == _countof(cb.viewports), "sizes should match");

        for (int i = 0; i < _countof(vpOutput); ++i)
        {
            cb.viewports[i][0] = vpOutput[i].TopLeftX;
            cb.viewports[i][1] = vpOutput[i].TopLeftY;
            cb.viewports[i][2] = vpOutput[i].Width;
            cb.viewports[i][3] = vpOutput[i].Height;
        }

        auto bokehMem = m_graphicsMemory->AllocateConstant(cb);

        ID3D12DescriptorHeap* pHeaps[1] = { m_srvHeap.Heap() };
        cmdList->SetDescriptorHeaps(_countof(pHeaps), pHeaps);
        cmdList->SetGraphicsRootSignature(m_commonRS.Get());

        cmdList->SetGraphicsRootConstantBufferView(e_rootParameterCB0, bokehMem.GpuAddress());
        cmdList->SetGraphicsRootDescriptorTable(e_rootParameterGS4, m_srvHeap.GetGpuHandle(e_iSRVEnergies));
    }

    void BokehEffect::CreateResources(ID3D12Device* device, ResourceUploadBatch& batch)
    {
        // Initialize constant buffer views
        // Create the 1D weights texture
        {
            const D3D12_HEAP_PROPERTIES defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            const D3D12_RESOURCE_DESC descEnergy = CD3DX12_RESOURCE_DESC::Tex1D(DXGI_FORMAT_R32_FLOAT, NUM_RADII_WEIGHTS, 1, 1, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
            DX::ThrowIfFailed(device->CreateCommittedResource(
                &defaultHeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &descEnergy,
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                nullptr,
                IID_GRAPHICS_PPV_ARGS(m_energiesTex.ReleaseAndGetAddressOf())));
            DX::ThrowIfFailed(m_energiesTex->SetName(L"EnergiesTex"));

            device->CreateShaderResourceView(m_energiesTex.Get(), nullptr, m_srvHeap.GetCpuHandle(e_iSRVEnergies));
            device->CreateUnorderedAccessView(m_energiesTex.Get(), nullptr, nullptr, m_srvHeap.GetCpuHandle(e_iUAVEnergies));

            const D3D12_RESOURCE_DESC descScratch = CD3DX12_RESOURCE_DESC::Tex3D(DXGI_FORMAT_R32_FLOAT, NUM_RADII_WEIGHTS / 8, NUM_RADII_WEIGHTS / 8, NUM_RADII_WEIGHTS, 1, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
            DX::ThrowIfFailed(device->CreateCommittedResource(
                &defaultHeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &descScratch,
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                nullptr,
                IID_GRAPHICS_PPV_ARGS(m_scratchTex.ReleaseAndGetAddressOf())));
            DX::ThrowIfFailed(m_scratchTex->SetName(L"ScratchTex"));

            device->CreateUnorderedAccessView(m_scratchTex.Get(), nullptr, nullptr, m_srvHeap.GetCpuHandle(e_iUAVScratch));
        }

        // Load the iris tex
        DX::ThrowIfFailed(DirectX::CreateDDSTextureFromFile(device, batch, L"Assets\\irishexa32.dds", m_irisTex.ReleaseAndGetAddressOf(), true));
        DX::ThrowIfFailed(m_irisTex->SetName(L"IrisTex"));

        device->CreateShaderResourceView(m_irisTex.Get(), nullptr, m_srvHeap.GetCpuHandle(e_iSRVIris));
    }

    void BokehEffect::CreatePSO(ID3D12Device* device, DXGI_FORMAT format)
    {
        //  Load the shaders
        std::vector<uint8_t> bokehRS = DX::ReadData(L"BokehRS.cso");
        std::vector<uint8_t> createEnergyTexRS = DX::ReadData(L"CreateEnergyTexRS.cso");

        DX::ThrowIfFailed(device->CreateRootSignature(
            0,
            bokehRS.data(),
            bokehRS.size(),
            IID_GRAPHICS_PPV_ARGS(m_commonRS.ReleaseAndGetAddressOf())));

        DX::ThrowIfFailed(device->CreateRootSignature(
            0,
            createEnergyTexRS.data(),
            createEnergyTexRS.size(),
            IID_GRAPHICS_PPV_ARGS(m_createEnergyTexRS.ReleaseAndGetAddressOf())));

        std::vector<uint8_t> quadPointVS = DX::ReadData(L"QuadPointVS.cso");
        std::vector<uint8_t> quadPointGS = DX::ReadData(L"QuadPointGS.cso");
        std::vector<uint8_t> quadPointFastGS = DX::ReadData(L"QuadPointFastGS.cso");
        std::vector<uint8_t> quadPointPS = DX::ReadData(L"QuadPointPS.cso");
        std::vector<uint8_t> quadVS = DX::ReadData(L"QuadVS.cso");
        std::vector<uint8_t> recombinePS = DX::ReadData(L"RecombinePS.cso");
        std::vector<uint8_t> recombineDebugPS = DX::ReadData(L"RecombineDebugPS.cso");
        std::vector<uint8_t> createRGBZPS = DX::ReadData(L"CreateRGBZPS.cso");
        std::vector<uint8_t> downsampleRGBZPS = DX::ReadData(L"DownsampleRGBZPS.cso");
        std::vector<uint8_t> createEnergyTexCS = DX::ReadData(L"CreateEnergyTexCS.cso");

        // Create OM blend state
        D3D12_BLEND_DESC descBlend = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        descBlend.RenderTarget[0].BlendEnable = true;
        descBlend.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
        descBlend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;

        D3D12_COMPUTE_PIPELINE_STATE_DESC computeDescPso = {};
        computeDescPso.pRootSignature = m_createEnergyTexRS.Get();
        computeDescPso.CS.pShaderBytecode = createEnergyTexCS.data();
        computeDescPso.CS.BytecodeLength = createEnergyTexCS.size();
        DX::ThrowIfFailed(device->CreateComputePipelineState(&computeDescPso, IID_GRAPHICS_PPV_ARGS(m_createEnergyTexPSO.ReleaseAndGetAddressOf())));

        // Fill the shared PSO fields
        D3D12_GRAPHICS_PIPELINE_STATE_DESC descPso = {};
        descPso.InputLayout.pInputElementDescs = nullptr;
        descPso.InputLayout.NumElements = 0;
        descPso.pRootSignature = m_commonRS.Get();
        descPso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        descPso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        descPso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        descPso.SampleMask = UINT_MAX;
        descPso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        descPso.NumRenderTargets = 1;
        descPso.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
        descPso.DSVFormat = DXGI_FORMAT_UNKNOWN;
        descPso.SampleDesc.Count = 1;

        // Create the Recombine PSOs
        descPso.VS.pShaderBytecode = quadVS.data();
        descPso.VS.BytecodeLength = quadVS.size();
        descPso.PS.pShaderBytecode = recombinePS.data();
        descPso.PS.BytecodeLength = recombinePS.size();
        DX::ThrowIfFailed(device->CreateGraphicsPipelineState(&descPso, IID_GRAPHICS_PPV_ARGS(m_recombinePSO.ReleaseAndGetAddressOf())));

        descPso.PS.pShaderBytecode = recombineDebugPS.data();
        descPso.PS.BytecodeLength = recombineDebugPS.size();
        DX::ThrowIfFailed(device->CreateGraphicsPipelineState(&descPso, IID_GRAPHICS_PPV_ARGS(m_recombinePSODebug.ReleaseAndGetAddressOf())));

        // Create the downsample PSOs
        descPso.PS.pShaderBytecode = createRGBZPS.data();
        descPso.PS.BytecodeLength = createRGBZPS.size();
        DX::ThrowIfFailed(device->CreateGraphicsPipelineState(&descPso, IID_GRAPHICS_PPV_ARGS(m_createRGBZPSO.ReleaseAndGetAddressOf())));

        descPso.PS.pShaderBytecode = downsampleRGBZPS.data();
        descPso.PS.BytecodeLength = downsampleRGBZPS.size();
        DX::ThrowIfFailed(device->CreateGraphicsPipelineState(&descPso, IID_GRAPHICS_PPV_ARGS(m_downsampleRGBZPSO.ReleaseAndGetAddressOf())));

        // Create the quad point PSOs
        descPso.BlendState = descBlend;
        descPso.RTVFormats[0] = format;
        descPso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;

        descPso.VS.pShaderBytecode = quadPointVS.data();
        descPso.VS.BytecodeLength = quadPointVS.size();
        descPso.GS.pShaderBytecode = quadPointGS.data();
        descPso.GS.BytecodeLength = quadPointGS.size();
        descPso.PS.pShaderBytecode = quadPointPS.data();
        descPso.PS.BytecodeLength = quadPointPS.size();
        DX::ThrowIfFailed(device->CreateGraphicsPipelineState(&descPso, IID_GRAPHICS_PPV_ARGS(m_quadPointPSO.ReleaseAndGetAddressOf())));

        descPso.GS.pShaderBytecode = quadPointFastGS.data();
        descPso.GS.BytecodeLength = quadPointFastGS.size();
        DX::ThrowIfFailed(device->CreateGraphicsPipelineState(&descPso, IID_GRAPHICS_PPV_ARGS(m_quadPointFastPSO.ReleaseAndGetAddressOf())));
    }
}
