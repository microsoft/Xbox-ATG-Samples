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
        ID3D11Device* pDev, 
        UINT width, 
        UINT height, 
        DXGI_FORMAT fmt,
        ID3D11Texture2D** ppTexture, 
        ID3D11RenderTargetView** ppRTV, 
        ID3D11ShaderResourceView** ppSRV,
        D3D11_USAGE usage)
    {
        UINT usageFlags = 0;
        if (ppRTV != nullptr) usageFlags |= D3D11_BIND_RENDER_TARGET;
        if (ppSRV != nullptr) usageFlags |= D3D11_BIND_SHADER_RESOURCE;

        D3D11_TEXTURE2D_DESC texDesc;
        ZeroMemory(&texDesc, sizeof(texDesc));
        texDesc.Width = width;
        texDesc.Height = height;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = fmt;
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Usage = usage;
        texDesc.BindFlags = usageFlags;
        texDesc.CPUAccessFlags = 0;
        texDesc.MiscFlags = D3D11X_RESOURCE_MISC_NO_COLOR_COMPRESSION;

        DX::ThrowIfFailed(pDev->CreateTexture2D(&texDesc, nullptr, ppTexture));

        if (texDesc.BindFlags & D3D11_BIND_RENDER_TARGET)
        {
            D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
            ZeroMemory(&rtvDesc, sizeof(rtvDesc));
            rtvDesc.Format = fmt;
            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            rtvDesc.Texture2D.MipSlice = 0;

            DX::ThrowIfFailed(pDev->CreateRenderTargetView(*ppTexture, &rtvDesc, ppRTV));
        }

        if (texDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
            srvDesc.Format = fmt;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = 1;

            DX::ThrowIfFailed(pDev->CreateShaderResourceView(*ppTexture, &srvDesc, ppSRV));
        }
    }

    void CreateDepthTextureAndViews(
        ID3D11Device* pDev,
        UINT width,
        UINT height,
        DXGI_FORMAT fmt,
        ID3D11Texture2D** ppTexture,
        ID3D11DepthStencilView** ppDSV,
        ID3D11ShaderResourceView** ppSRV,
        D3D11_USAGE usage)
    {
        UINT usageFlags = 0;
        if (ppDSV != nullptr) usageFlags |= D3D11_BIND_DEPTH_STENCIL;
        if (ppSRV != nullptr) usageFlags |= D3D11_BIND_SHADER_RESOURCE;

        DXGI_FORMAT resFormat;
        resFormat = (fmt == DXGI_FORMAT_D32_FLOAT) ? DXGI_FORMAT_R32_TYPELESS : fmt;
        resFormat = (fmt == DXGI_FORMAT_D16_UNORM) ? DXGI_FORMAT_R16_TYPELESS : resFormat;

        DXGI_FORMAT srvFormat;
        srvFormat = (fmt == DXGI_FORMAT_D32_FLOAT) ? DXGI_FORMAT_R32_FLOAT : fmt;
        srvFormat = (fmt == DXGI_FORMAT_D16_UNORM) ? DXGI_FORMAT_R16_UNORM : srvFormat;

        D3D11_TEXTURE2D_DESC texDesc;
        ZeroMemory(&texDesc, sizeof(texDesc));
        texDesc.Width = width;
        texDesc.Height = height;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = resFormat;
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Usage = usage;
        texDesc.BindFlags = usageFlags;
        texDesc.CPUAccessFlags = 0;
        texDesc.MiscFlags = D3D11X_RESOURCE_MISC_NO_DEPTH_COMPRESSION;

        DX::ThrowIfFailed(pDev->CreateTexture2D(&texDesc, nullptr, ppTexture));

        if (texDesc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
        {
            D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
            ZeroMemory(&dsvDesc, sizeof(dsvDesc));
            dsvDesc.Format = fmt;
            dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = 0;

            DX::ThrowIfFailed(pDev->CreateDepthStencilView(*ppTexture, &dsvDesc, ppDSV));
        }

        if (texDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
            srvDesc.Format = srvFormat;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = 1;

            DX::ThrowIfFailed(pDev->CreateShaderResourceView(*ppTexture, &srvDesc, ppSRV));
        }
    }
}
