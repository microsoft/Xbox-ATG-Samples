//--------------------------------------------------------------------------------------
// pch.cpp
//
// Include the standard header and generate the precompiled header.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"

namespace ATG
{
    void CreateColorTextureAndViews(
        ID3D12Device* device,
        UINT width,
        UINT height,
        DXGI_FORMAT format,
        ID3D12Resource** resource,
        D3D12_CPU_DESCRIPTOR_HANDLE hRTV,
        D3D12_CPU_DESCRIPTOR_HANDLE hSRV,
        D3D12_CLEAR_VALUE* pOptimizedClearValue,
        D3D12_HEAP_TYPE heapType)
    {
        D3D12_RESOURCE_DESC descTex = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1);
        D3D12_HEAP_FLAGS heapMiscFlag = D3D12_HEAP_FLAG_NONE;
        D3D12_RESOURCE_STATES usage = D3D12_RESOURCE_STATE_COMMON;
        switch (heapType)
        {
        case D3D12_HEAP_TYPE_DEFAULT:
            descTex.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            break;
        case D3D12_HEAP_TYPE_UPLOAD:
            usage = D3D12_RESOURCE_STATE_GENERIC_READ;
            break;
        case D3D12_HEAP_TYPE_READBACK:
        {
            usage = D3D12_RESOURCE_STATE_COPY_DEST;

            D3D12_PLACED_SUBRESOURCE_FOOTPRINT Layout;
            UINT NumRows;
            UINT64 RowSize;
            UINT64 TotalBytes;
            device->GetCopyableFootprints(&descTex, 0, 1, 0, &Layout, &NumRows, &RowSize, &TotalBytes);
            descTex = CD3DX12_RESOURCE_DESC::Buffer(TotalBytes);
        }
        break;
        }

        const D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(heapType);
        DX::ThrowIfFailed(device->CreateCommittedResource(
            &heapProperties,
            heapMiscFlag,
            &descTex,
            usage,
            pOptimizedClearValue,
            IID_GRAPHICS_PPV_ARGS(resource)));

        if (hRTV.ptr != 0)
        {
            D3D12_RENDER_TARGET_VIEW_DESC descRTV = {};
            descRTV.Format = format;
            descRTV.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            device->CreateRenderTargetView(*resource, &descRTV, hRTV);
        }

        if (hSRV.ptr != 0)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC descSRV = {};
            descSRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            descSRV.Format = format;
            descSRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            descSRV.Texture2D.MipLevels = 1;
            device->CreateShaderResourceView(*resource, &descSRV, hSRV);
        }
    }

    void CreateDepthTextureAndViews(
        ID3D12Device* device,
        UINT width,
        UINT height,
        DXGI_FORMAT format,
        ID3D12Resource** resource,
        D3D12_CPU_DESCRIPTOR_HANDLE hDSV,
        D3D12_CPU_DESCRIPTOR_HANDLE hSRV,
        D3D12_CLEAR_VALUE* pOptimizedClearValue,
        D3D12_HEAP_TYPE heapType)
    {

        DXGI_FORMAT resFormat;
        resFormat = (format == DXGI_FORMAT_D32_FLOAT) ? DXGI_FORMAT_R32_TYPELESS : format;
        resFormat = (format == DXGI_FORMAT_D16_UNORM) ? DXGI_FORMAT_R16_TYPELESS : resFormat;

        DXGI_FORMAT srvFormat;
        srvFormat = (format == DXGI_FORMAT_D32_FLOAT) ? DXGI_FORMAT_R32_FLOAT : format;
        srvFormat = (format == DXGI_FORMAT_D16_UNORM) ? DXGI_FORMAT_R16_UNORM : srvFormat;

        D3D12_RESOURCE_DESC descTex = CD3DX12_RESOURCE_DESC::Tex2D(resFormat, width, height, 1, 1);
        D3D12_HEAP_FLAGS heapMiscFlag = D3D12_HEAP_FLAG_NONE;
        D3D12_RESOURCE_STATES usage = D3D12_RESOURCE_STATE_COMMON;
        switch (heapType)
        {
        case D3D12_HEAP_TYPE_DEFAULT:
            descTex.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
            break;
        case D3D12_HEAP_TYPE_UPLOAD:
            usage = D3D12_RESOURCE_STATE_GENERIC_READ;
            break;
        case D3D12_HEAP_TYPE_READBACK:
        {
            usage = D3D12_RESOURCE_STATE_COPY_DEST;

            D3D12_PLACED_SUBRESOURCE_FOOTPRINT Layout;
            UINT NumRows;
            UINT64 RowSize;
            UINT64 TotalBytes;
            device->GetCopyableFootprints(&descTex, 0, 1, 0, &Layout, &NumRows, &RowSize, &TotalBytes);
            descTex = CD3DX12_RESOURCE_DESC::Buffer(TotalBytes);
        }
        break;
        }

        const D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(heapType);
        DX::ThrowIfFailed(device->CreateCommittedResource(
            &heapProperties,
            heapMiscFlag,
            &descTex,
            usage,
            pOptimizedClearValue,
            IID_GRAPHICS_PPV_ARGS(resource)));

        if (hDSV.ptr != 0)
        {
            D3D12_DEPTH_STENCIL_VIEW_DESC descRTV = {};
            descRTV.Format = format;
            descRTV.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            device->CreateDepthStencilView(*resource, &descRTV, hDSV);
        }

        if (hSRV.ptr != 0)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC descSRV = {};
            descSRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            descSRV.Format = srvFormat;
            descSRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            descSRV.Texture2D.MipLevels = 1;
            device->CreateShaderResourceView(*resource, &descSRV, hSRV);
        }
    }
}
