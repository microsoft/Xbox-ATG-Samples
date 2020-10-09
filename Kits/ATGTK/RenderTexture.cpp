//--------------------------------------------------------------------------------------
// File: RenderTexture.cpp
//
// Helper for managing offscreen render targets
//
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//-------------------------------------------------------------------------------------

#include "pch.h"
#include "RenderTexture.h"

#include "DirectXHelpers.h"

#include <algorithm>
#include <stdio.h>
#include <stdexcept>

using namespace DirectX;
using namespace DX;

using Microsoft::WRL::ComPtr;

#if defined(__d3d12_h__) || defined(__d3d12_x_h__) || defined(__XBOX_D3D12_X__)
//======================================================================================
// Direct3D 12
//======================================================================================
RenderTexture::RenderTexture(DXGI_FORMAT format) noexcept :
    m_state(D3D12_RESOURCE_STATE_COMMON),
    m_srvDescriptor{},
    m_rtvDescriptor{},
    m_clearColor{},
    m_format(format),
    m_width(0),
    m_height(0)
{
}

void RenderTexture::SetDevice(_In_ ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE srvDescriptor, D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptor)
{
    if (device == m_device.Get()
        && srvDescriptor.ptr == m_srvDescriptor.ptr
        && rtvDescriptor.ptr == m_rtvDescriptor.ptr)
        return;

    if (m_device)
    {
        ReleaseDevice();
    }

    {
        D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport = { m_format, D3D12_FORMAT_SUPPORT1_NONE, D3D12_FORMAT_SUPPORT2_NONE };
        if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport))))
        {
            throw std::exception("CheckFeatureSupport");
        }

        UINT required = D3D12_FORMAT_SUPPORT1_TEXTURE2D | D3D12_FORMAT_SUPPORT1_RENDER_TARGET;
        if ((formatSupport.Support1 & required) != required)
        {
#ifdef _DEBUG
            char buff[128] = {};
            sprintf_s(buff, "RenderTexture: Device does not support the requested format (%u)!\n", m_format);
            OutputDebugStringA(buff);
#endif
            throw std::exception("RenderTexture");
        }
    }

    if (!srvDescriptor.ptr || !rtvDescriptor.ptr)
    {
        throw std::exception("Invalid descriptors");
    }

    m_device = device;

    m_srvDescriptor = srvDescriptor;
    m_rtvDescriptor = rtvDescriptor;
}

void RenderTexture::SizeResources(size_t width, size_t height)
{
    if (width == m_width && height == m_height)
        return;

    if (m_width > UINT32_MAX || m_height > UINT32_MAX)
    {
        throw std::out_of_range("Invalid width/height");
    }

    if (!m_device)
        return;

    m_width = m_height = 0;

    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(m_format,
        static_cast<UINT64>(width),
        static_cast<UINT>(height),
        1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

    D3D12_CLEAR_VALUE clearValue = { m_format, {} };
    memcpy(clearValue.Color, m_clearColor, sizeof(clearValue.Color));

    m_state = D3D12_RESOURCE_STATE_RENDER_TARGET;

    // Create a render target
    ThrowIfFailed(
        m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
            &desc,
            m_state, &clearValue,
            IID_GRAPHICS_PPV_ARGS(m_resource.ReleaseAndGetAddressOf()))
    );

    SetDebugObjectName(m_resource.Get(), L"RenderTexture RT");

    // Create RTV.
    m_device->CreateRenderTargetView(m_resource.Get(), nullptr, m_rtvDescriptor);

    // Create SRV.
    m_device->CreateShaderResourceView(m_resource.Get(), nullptr, m_srvDescriptor);

    m_width = width;
    m_height = height;
}

void RenderTexture::ReleaseDevice() noexcept
{
    m_resource.Reset();
    m_device.Reset();

    m_state = D3D12_RESOURCE_STATE_COMMON;
    m_width = m_height = 0;

    m_srvDescriptor.ptr = m_rtvDescriptor.ptr = 0;
}

void RenderTexture::TransitionTo(_In_ ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES afterState)
{
    TransitionResource(commandList, m_resource.Get(), m_state, afterState);
    m_state = afterState;
}

#else
//======================================================================================
// Direct3D 11
//======================================================================================
RenderTexture::RenderTexture(DXGI_FORMAT format) noexcept :
#if defined(_XBOX_ONE) && defined(_TITLE)
    m_fastSemantics(false),
#endif
    m_format(format),
    m_width(0),
    m_height(0)
{
}

void RenderTexture::SetDevice(_In_ ID3D11Device* device)
{
    if (device == m_device.Get())
        return;

    if (m_device)
    {
        ReleaseDevice();
    }

    {
        UINT formatSupport = 0;
        if (FAILED(device->CheckFormatSupport(m_format, &formatSupport)))
        {
            throw std::exception("CheckFormatSupport");
        }

        UINT32 required = D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_RENDER_TARGET;
        if ((formatSupport & required) != required)
        {
#ifdef _DEBUG
            char buff[128] = {};
            sprintf_s(buff, "RenderTexture: Device does not support the requested format (%u)!\n", m_format);
            OutputDebugStringA(buff);
#endif
            throw std::exception("RenderTexture");
        }
    }

    m_device = device;

#if defined(_XBOX_ONE) && defined(_TITLE)
    m_fastSemantics = (device->GetCreationFlags() & D3D11_CREATE_DEVICE_IMMEDIATE_CONTEXT_FAST_SEMANTICS) != 0;
#endif
}


void RenderTexture::SizeResources(size_t width, size_t height)
{
    if (width == m_width && height == m_height)
        return;

    if (m_width > UINT32_MAX || m_height > UINT32_MAX)
    {
        throw std::out_of_range("Invalid width/height");
    }

    if (!m_device)
        return;

    m_width = m_height = 0;

    // Create a render target
    CD3D11_TEXTURE2D_DESC renderTargetDesc(
        m_format,
        static_cast<UINT>(width),
        static_cast<UINT>(height),
        1, // The render target view has only one texture.
        1, // Use a single mipmap level.
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
        D3D11_USAGE_DEFAULT,
        0,
        1
    );

    ThrowIfFailed(m_device->CreateTexture2D(
        &renderTargetDesc,
        nullptr,
        m_renderTarget.ReleaseAndGetAddressOf()
    ));

    SetDebugObjectName(m_renderTarget.Get(), "RenderTexture RT");

    // Create RTV.
    CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc(D3D11_RTV_DIMENSION_TEXTURE2D, m_format);

    ThrowIfFailed(m_device->CreateRenderTargetView(
        m_renderTarget.Get(),
        &renderTargetViewDesc,
        m_renderTargetView.ReleaseAndGetAddressOf()
    ));

    SetDebugObjectName(m_renderTargetView.Get(), "RenderTexture RTV");

    // Create SRV.
    CD3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc(D3D11_SRV_DIMENSION_TEXTURE2D, m_format);

    ThrowIfFailed(m_device->CreateShaderResourceView(
        m_renderTarget.Get(),
        &shaderResourceViewDesc,
        m_shaderResourceView.ReleaseAndGetAddressOf()
    ));

    SetDebugObjectName(m_shaderResourceView.Get(), "RenderTexture SRV");

    m_width = width;
    m_height = height;
}


void RenderTexture::ReleaseDevice() noexcept
{
    m_renderTargetView.Reset();
    m_shaderResourceView.Reset();
    m_renderTarget.Reset();

    m_device.Reset();

    m_width = m_height = 0;
}

#if defined(_XBOX_ONE) && defined(_TITLE)
void RenderTexture::EndScene(_In_ ID3D11DeviceContextX* context)
{
    if (m_fastSemantics)
    {
        context->FlushGpuCacheRange(
            D3D11_FLUSH_ENSURE_CB0_COHERENCY
            | D3D11_FLUSH_COLOR_BLOCK_INVALIDATE
            | D3D11_FLUSH_TEXTURE_L1_INVALIDATE
            | D3D11_FLUSH_TEXTURE_L2_INVALIDATE,
            nullptr, D3D11_FLUSH_GPU_CACHE_RANGE_ALL);
        context->DecompressResource(m_renderTarget.Get(), 0, nullptr,
            m_renderTarget.Get(), 0, nullptr,
            m_format, D3D11X_DECOMPRESS_PROPAGATE_COLOR_CLEAR);
    }
}
#endif

#endif

void RenderTexture::SetWindow(const RECT& output)
{
    // Determine the render target size in pixels.
    auto width = size_t(std::max<LONG>(output.right - output.left, 1));
    auto height = size_t(std::max<LONG>(output.bottom - output.top, 1));

    SizeResources(width, height);
}

