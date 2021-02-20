//--------------------------------------------------------------------------------------
// BokehEffect.cpp
//
// Demonstrates how to render depth of field using point sprites.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "BokehEffect.h"
#include "shadersettings.h"

using namespace ATG;
using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace Microsoft::WRL;

namespace
{
#define CREATE_SHADER_FUNC(type) \
    void CreateShader(ID3D11Device* device, const wchar_t* filename, ID3D11##type##Shader** outShader)\
    {\
        std::vector<uint8_t> data = DX::ReadData(filename);\
        DX::ThrowIfFailed(device->Create##type##Shader(data.data(), data.size(), nullptr, outShader));\
    }

    CREATE_SHADER_FUNC(Vertex);
    CREATE_SHADER_FUNC(Geometry);
    CREATE_SHADER_FUNC(Pixel);
    CREATE_SHADER_FUNC(Compute);

    void GetTextureDescFromSRV(ID3D11ShaderResourceView* pSRV, D3D11_TEXTURE2D_DESC& desc)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        pSRV->GetDesc(&srvDesc);
        assert(srvDesc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D);

        ComPtr<ID3D11Texture2D> tex;
        pSRV->GetResource(reinterpret_cast< ID3D11Resource** >(tex.ReleaseAndGetAddressOf()));
        tex->GetDesc(&desc);
    }
}

namespace ATG
{
    BokehEffect::BokehEffect(ID3D11Device* device)
        : m_width(0)
        , m_height(0)
        , m_format(DXGI_FORMAT_UNKNOWN)
    {
        CreateDeviceDependentResources(device);
    }

    void BokehEffect::ResizeResources(ID3D11Device* device, int uMaxWidth, int uMaxHeight, DXGI_FORMAT format)
    {
        if (m_width == m_width && m_height == m_height && m_format == format)
        {
            return;
        }

        // DOF texture
        // contains multiple viewports
        m_width = uMaxWidth;
        m_height = uMaxHeight;
        m_format = format;

        UINT w = uMaxWidth;
        UINT h = 2 * uMaxHeight / FIRST_DOWNSAMPLE + uMaxHeight / (2 * FIRST_DOWNSAMPLE) + uMaxHeight / (4 * FIRST_DOWNSAMPLE);

        CreateColorTextureAndViews(device, w, h, format, 
            m_DOFColorTexture.ReleaseAndGetAddressOf(), 
            m_DOFColorTextureRTV.ReleaseAndGetAddressOf(), 
            m_DOFColorTextureSRV.ReleaseAndGetAddressOf());

        CreateColorTextureAndViews(device, uMaxWidth, uMaxHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, 
            m_sourceColorTextureRGBZCopy.ReleaseAndGetAddressOf(),
            m_sourceColorTextureRGBZCopyRTV.ReleaseAndGetAddressOf(),
            m_sourceColorTextureRGBZCopySRV.ReleaseAndGetAddressOf());

        CreateColorTextureAndViews(device, uMaxWidth / FIRST_DOWNSAMPLE, uMaxHeight / FIRST_DOWNSAMPLE, DXGI_FORMAT_R16G16B16A16_FLOAT, 
            m_sourceColorTextureRGBZHalfCopy.ReleaseAndGetAddressOf(),
            m_sourceColorTextureRGBZHalfCopyRTV.ReleaseAndGetAddressOf(),
            m_sourceColorTextureRGBZHalfCopySRV.ReleaseAndGetAddressOf());

        m_DOFColorTexture->SetName(L"DOFColorTexture");
        m_sourceColorTextureRGBZCopy->SetName(L"SourceColorTextureRGBZCopy");
        m_sourceColorTextureRGBZHalfCopy->SetName(L"SourceColorTextureRGBZHalfCopy");
    }

    void BokehEffect::Render(
        ID3D11DeviceContextX* context,
        ID3D11ShaderResourceView* srcColorSRV,
        ID3D11ShaderResourceView* srcDepthSRV,
        ID3D11RenderTargetView* dstRTV,
        const DirectX::XMMATRIX& matInvProj,
        const Parameters& params,
        bool useDebugShader)
    {
        assert(context != nullptr);
        assert(srcColorSRV != nullptr);
        assert(srcDepthSRV != nullptr);
        assert(dstRTV != nullptr);

        // find iris texture weights
        {
            ScopedPixEvent setSize(context, PIX_COLOR_DEFAULT, L"BokehEffect::Render - compute energy tex");

            ID3D11UnorderedAccessView* uavs[] = { m_scratchTexUAV.Get(), m_energiesTexUAV.Get() };

            context->CSEnableAutomaticGpuFlush(TRUE);
            context->CSSetShader(m_createEnergyTexCS.Get(), nullptr, 0);
            context->CSSetSamplers(0, 1, m_sampler.GetAddressOf());
            context->CSSetShaderResources(0, 1, m_irisTexSRV.GetAddressOf());
            context->CSSetUnorderedAccessViews(0, 2, uavs, nullptr);
            context->Dispatch(NUM_RADII_WEIGHTS / 8, NUM_RADII_WEIGHTS / 8, NUM_RADII_WEIGHTS);
        }

        D3D11_TEXTURE2D_DESC texDesc;
        GetTextureDescFromSRV(srcDepthSRV, texDesc);

        D3D11_VIEWPORT vpResult = { 0, 0, (FLOAT)texDesc.Width, (FLOAT)texDesc.Height,  0, 1 };
        D3D11_VIEWPORT vpResultHalf = { 0, 0, (FLOAT)texDesc.Width / 2, (FLOAT)texDesc.Height / 2,  0, 1 };

        void* null = nullptr;

        StartRendering(context, texDesc, matInvProj, params);
        
        // copy out the source
        // this is 0.2 ms faster than CopyResource
        {
            ScopedPixEvent e(context, PIX_COLOR_DEFAULT, L"BokehEffect::Render - copy out the source");

            context->OMSetRenderTargets(1, m_sourceColorTextureRGBZCopyRTV.GetAddressOf(), nullptr);
            context->RSSetViewports(1, &vpResult);
            context->VSSetShader(m_quadVS.Get(), nullptr, 0);
            context->PSSetShader(m_createRGBZPS.Get(), nullptr, 0);
            context->PSSetShaderResources(0, 1, &srcColorSRV);
            context->PSSetShaderResources(2, 1, &srcDepthSRV);
            context->Draw(3, 0);
        }
        context->PSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&null);
        context->PSSetShaderResources(2, 1, (ID3D11ShaderResourceView**)&null);


        // downsample
        {
            ScopedPixEvent e(context, PIX_COLOR_DEFAULT, L"BokehEffect::Render - downsample");

            context->OMSetRenderTargets(1, m_sourceColorTextureRGBZHalfCopyRTV.GetAddressOf(), nullptr);
            context->RSSetViewports(1, &vpResultHalf);
            context->PSSetShader(m_downsampleRGBZPS.Get(), nullptr, 0);
            context->PSSetShaderResources(0, 1, m_sourceColorTextureRGBZCopySRV.GetAddressOf());
            context->Draw(3, 0);
        }
        context->PSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&null);


        // prepare the multi-viewport dof render target
        {
            ScopedPixEvent e(context, PIX_COLOR_DEFAULT, L"BokehEffect::Render - CoC DOF");

            context->OMSetRenderTargets(1, m_DOFColorTextureRTV.GetAddressOf(), nullptr);
            context->ClearRenderTargetView(m_DOFColorTextureRTV.Get(), DirectX::Colors::Transparent);
            context->RSSetViewports(_countof(m_vpSplitOutput), m_vpSplitOutput);
            context->OMSetBlendState(m_pointsBS.Get(), DirectX::Colors::Transparent, D3D11_DEFAULT_SAMPLE_MASK);

            // split into slices, do the CoC DOF
            context->VSSetShader(m_quadPointVS.Get(), nullptr, 0);
            context->GSSetShader(params.useFastShader ? m_quadPointFastGS.Get() : m_quadPointGS.Get(), nullptr, 0);
            context->PSSetShader(m_quadPointPS.Get(), nullptr, 0);

            context->VSSetShaderResources(0, 1, m_sourceColorTextureRGBZHalfCopySRV.GetAddressOf());
            context->GSSetShaderResources(0, 1, m_sourceColorTextureRGBZHalfCopySRV.GetAddressOf());
            context->PSSetShaderResources(1, 1, m_irisTexSRV.GetAddressOf());

            context->Draw(texDesc.Width * texDesc.Height / (FIRST_DOWNSAMPLE * FIRST_DOWNSAMPLE * 2 * 2), 0);    // each GS can output up to 4 triangles
        }
        context->OMSetBlendState(nullptr, DirectX::Colors::Transparent, D3D11_DEFAULT_SAMPLE_MASK);
        context->VSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&null);
        context->GSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&null);
        context->PSSetShaderResources(1, 1, (ID3D11ShaderResourceView**)&null);


        // combine the resulting viewports
        {
            ScopedPixEvent e(context, PIX_COLOR_DEFAULT, L"BokehEffect::Render - Combine");

            context->OMSetRenderTargets(1, &dstRTV, nullptr);
            context->RSSetViewports(1, &vpResult);
            context->VSSetShader(m_quadVS.Get(), nullptr, 0);
            context->GSSetShader(nullptr, nullptr, 0);
            context->PSSetShaderResources(0, 1, m_DOFColorTextureSRV.GetAddressOf());
            context->PSSetShaderResources(1, 1, m_irisTexSRV.GetAddressOf());
            context->PSSetShaderResources(2, 1, &srcDepthSRV);
            context->PSSetShaderResources(3, 1, m_sourceColorTextureRGBZCopySRV.GetAddressOf());  // pSrcColorSRV

            // Optionally set debug shader that shows viewports breakdown etc
            context->PSSetShader(useDebugShader ? m_recombineDebugPS.Get() : m_recombinePS.Get(), nullptr, 0);
            context->Draw(3, 0);
        }

        StopRendering(context);
    }

    void BokehEffect::StartRendering(ID3D11DeviceContextX* context, const D3D11_TEXTURE2D_DESC& texDesc, const DirectX::XMMATRIX& matInvProj, const Parameters& params)
    {
        D3D11_TEXTURE2D_DESC dofTexDesc;
        m_DOFColorTexture->GetDesc(&dofTexDesc);

        D3D11_TEXTURE2D_DESC irisTexDesc;
        GetTextureDescFromSRV(m_irisTexSRV.Get(), irisTexDesc);

        // output into several viewports
        float vpSx = float(m_width) / FIRST_DOWNSAMPLE;
        float vpSy = float(m_height) / FIRST_DOWNSAMPLE;
        float vpSx2 = float(m_width) / (2 * FIRST_DOWNSAMPLE);
        float vpSy2 = float(m_height) / (2 * FIRST_DOWNSAMPLE);
        float vpSx4 = float(m_width) / (4 * FIRST_DOWNSAMPLE);
        float vpSy4 = float(m_height) / (4 * FIRST_DOWNSAMPLE);

        D3D11_VIEWPORT vpOutput[6] =
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

        auto invProj = XMMatrixTranspose(matInvProj);

        BokehCB cb = { 0 };
        memcpy(&cb.mInvProj, &invProj, sizeof(invProj));
        cb.depthBufferSize[0] = (FLOAT)texDesc.Width;
        cb.depthBufferSize[1] = (FLOAT)texDesc.Height;
        cb.dofTexSize[0] = (FLOAT)dofTexDesc.Width;
        cb.dofTexSize[1] = (FLOAT)dofTexDesc.Height;
        cb.FNumber = params.FNumber;
        cb.focusLength = params.focusLength;
        cb.focalPlane = params.focalPlane;
        cb.maxCoCDiameterNear = params.maxCoCSizeNear;
        cb.maxCoCDiameterFar = params.maxCoCSizeFar;
        cb.irisTextureOffset = 0.5f / (FLOAT)irisTexDesc.Width;
        cb.srcScreenSize[0] = (FLOAT)texDesc.Width;
        cb.srcScreenSize[1] = (FLOAT)texDesc.Height;
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

        D3D11_MAPPED_SUBRESOURCE mapping;
        context->Map(m_bokehCB.Get(), 0, D3D11_MAP::D3D11_MAP_WRITE_DISCARD, 0, &mapping);
        std::memcpy(mapping.pData, &cb, sizeof(cb));
        context->Unmap(m_bokehCB.Get(), 0);

        context->VSSetConstantBuffers(0, 1, m_bokehCB.GetAddressOf());
        context->GSSetConstantBuffers(0, 1, m_bokehCB.GetAddressOf());
        context->PSSetConstantBuffers(0, 1, m_bokehCB.GetAddressOf());

        context->VSSetShaderResources(4, 1, m_energiesTexSRV.GetAddressOf());
        context->GSSetShaderResources(4, 1, m_energiesTexSRV.GetAddressOf());
        context->PSSetShaderResources(4, 1, m_energiesTexSRV.GetAddressOf());

        context->PSSetSamplers(0, 1, m_sampler.GetAddressOf());
        context->VSSetSamplers(0, 1, m_sampler.GetAddressOf());
        context->GSSetSamplers(0, 1, m_sampler.GetAddressOf());

        context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        void* null = nullptr;
        UINT zero = 0;
        context->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&null, &zero, &zero);
        context->OMSetBlendState(nullptr, DirectX::Colors::Transparent, D3D11_DEFAULT_SAMPLE_MASK);
    }

    void BokehEffect::StopRendering(ID3D11DeviceContextX* context)
    {
        void* null = 0;

        context->OMSetBlendState(nullptr, DirectX::Colors::Transparent, D3D11_DEFAULT_SAMPLE_MASK);

        context->PSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&null);
        context->PSSetShaderResources(1, 1, (ID3D11ShaderResourceView**)&null);
        context->PSSetShaderResources(2, 1, (ID3D11ShaderResourceView**)&null);
        context->PSSetShaderResources(3, 1, (ID3D11ShaderResourceView**)&null);

        context->VSSetShaderResources(4, 1, (ID3D11ShaderResourceView**)&null);
        context->GSSetShaderResources(4, 1, (ID3D11ShaderResourceView**)&null);
        context->PSSetShaderResources(4, 1, (ID3D11ShaderResourceView**)&null);

        for (int i = 0; i < 2; ++i)
        {
            context->VSSetConstantBuffers(i, 1, (ID3D11Buffer**)&null);
            context->GSSetConstantBuffers(i, 1, (ID3D11Buffer**)&null);
            context->PSSetConstantBuffers(i, 1, (ID3D11Buffer**)&null);
        }

        context->VSSetShader(nullptr, nullptr, 0);
        context->GSSetShader(nullptr, nullptr, 0);
        context->PSSetShader(nullptr, nullptr, 0);

        context->VSSetSamplers(0, 1, (ID3D11SamplerState**)&null);
        context->PSSetSamplers(0, 1, (ID3D11SamplerState**)&null);
        context->GSSetSamplers(0, 1, (ID3D11SamplerState**)&null);
    }

    void BokehEffect::CreateDeviceDependentResources(ID3D11Device* device)
    {
        //  Load the shaders
        CreateShader(device, L"QuadPointVS.cso", m_quadPointVS.ReleaseAndGetAddressOf());
        CreateShader(device, L"QuadPointGS.cso", m_quadPointGS.ReleaseAndGetAddressOf());
        CreateShader(device, L"QuadPointFastGS.cso", m_quadPointFastGS.ReleaseAndGetAddressOf());
        CreateShader(device, L"QuadPointPS.cso", m_quadPointPS.ReleaseAndGetAddressOf());
        CreateShader(device, L"QuadVS.cso", m_quadVS.ReleaseAndGetAddressOf());
        CreateShader(device, L"RecombinePS.cso", m_recombinePS.ReleaseAndGetAddressOf());
        CreateShader(device, L"RecombineDebugPS.cso", m_recombineDebugPS.ReleaseAndGetAddressOf());
        CreateShader(device, L"CreateRGBZPS.cso", m_createRGBZPS.ReleaseAndGetAddressOf());
        CreateShader(device, L"DownsampleRGBZPS.cso", m_downsampleRGBZPS.ReleaseAndGetAddressOf());
        CreateShader(device, L"CreateEnergyTexCS.cso", m_createEnergyTexCS.ReleaseAndGetAddressOf());

        // Initialize constant buffer views
        // Create the 1D weights texture
        {
            D3D11_TEXTURE1D_DESC descEnergy = CD3D11_TEXTURE1D_DESC(DXGI_FORMAT_R32_FLOAT, NUM_RADII_WEIGHTS, 1, 1, D3D11_BIND_UNORDERED_ACCESS| D3D11_BIND_SHADER_RESOURCE);
            DX::ThrowIfFailed(device->CreateTexture1D(&descEnergy, nullptr, m_energiesTex.ReleaseAndGetAddressOf()));
            DX::ThrowIfFailed(device->CreateShaderResourceView(m_energiesTex.Get(), nullptr, m_energiesTexSRV.ReleaseAndGetAddressOf()));
            DX::ThrowIfFailed(device->CreateUnorderedAccessView(m_energiesTex.Get(), nullptr, m_energiesTexUAV.ReleaseAndGetAddressOf()));

            D3D11_TEXTURE3D_DESC descScratch = CD3D11_TEXTURE3D_DESC(DXGI_FORMAT_R32_FLOAT, NUM_RADII_WEIGHTS / 8, NUM_RADII_WEIGHTS / 8, NUM_RADII_WEIGHTS, 1, D3D11_BIND_UNORDERED_ACCESS);
            DX::ThrowIfFailed(device->CreateTexture3D(&descScratch, nullptr, m_scratchTex.ReleaseAndGetAddressOf()));
            DX::ThrowIfFailed(device->CreateUnorderedAccessView(m_scratchTex.Get(), nullptr, m_scratchTexUAV.ReleaseAndGetAddressOf()));

            DX::ThrowIfFailed(m_energiesTex->SetName(L"EnergiesTex"));
            DX::ThrowIfFailed(m_scratchTex->SetName(L"ScratchTex"));
        }

        D3D11_BUFFER_DESC descBuffer = CD3D11_BUFFER_DESC(sizeof(BokehCB), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
        device->CreateBuffer(&descBuffer, nullptr, m_bokehCB.ReleaseAndGetAddressOf());

        // Load the iris tex
        DX::ThrowIfFailed(DirectX::CreateDDSTextureFromFile(device, L"Assets\\irishexa32.dds", nullptr, m_irisTexSRV.ReleaseAndGetAddressOf()));

        // Create OM blend state
        D3D11_BLEND_DESC bsDesc = {};
        bsDesc.RenderTarget[0].BlendEnable = TRUE;
        bsDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        bsDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
        bsDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        bsDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        bsDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        bsDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        bsDesc.RenderTarget[0].RenderTargetWriteMask = 0x0f;
        DX::ThrowIfFailed(device->CreateBlendState(&bsDesc, m_pointsBS.ReleaseAndGetAddressOf()));

        // Create the sampler state
        D3D11_SAMPLER_DESC  ssDesc = {};
        ssDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        ssDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
        ssDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
        ssDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        ssDesc.BorderColor[0] = 0;
        ssDesc.BorderColor[1] = 0;
        ssDesc.BorderColor[2] = 0;
        ssDesc.BorderColor[3] = 0;
        ssDesc.MinLOD = 0;
        ssDesc.MaxLOD = D3D11_FLOAT32_MAX;
        DX::ThrowIfFailed(device->CreateSamplerState(&ssDesc, m_sampler.ReleaseAndGetAddressOf()));
    }
}
