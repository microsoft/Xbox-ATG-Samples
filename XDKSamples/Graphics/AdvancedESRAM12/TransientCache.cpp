//--------------------------------------------------------------------------------------
// TransientCache.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "TransientCache.h"

#include "Hash.h"
#include "PageAllocator.h"

using namespace ATG;
using namespace Microsoft::WRL;

namespace std
{
    // Hash template 
    template <> 
    struct hash<TransientDesc>
    {
        size_t operator()(const TransientDesc& x) const
        {
            uint8_t digest[MD5_DIGEST_LENGTH] = {};
            DX::ThrowIfFailed(MD5Checksum(&x, sizeof(x), digest));
            return *(size_t*)digest;
        }
    };
}


namespace
{
    //--------------------------------------------
    // Helper functions

    bool IsDepthFormat(DXGI_FORMAT format)
    {
        return format == DXGI_FORMAT_D16_UNORM
            || format == DXGI_FORMAT_D16_UNORM_S8_UINT
            || format == DXGI_FORMAT_D24_UNORM_S8_UINT
            || format == DXGI_FORMAT_D32_FLOAT
            || format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    }

    template <typename T>
    size_t Hash(const T& val)
    {
        return std::hash<T>()(val);
    }

    bool IsMetadataPlane(const XG_PLANE_LAYOUT& layout)
    {
        return layout.Usage == XG_PLANE_USAGE_COLOR_MASK
            || layout.Usage == XG_PLANE_USAGE_FRAGMENT_MASK
            || layout.Usage == XG_PLANE_USAGE_HTILE
#if _XDK_VER >= 0x3F6803F3 /* XDK Edition 170600 */
            || layout.Usage == XG_PLANE_USAGE_DELTA_COLOR_COMPRESSION
#endif
            ;
    }

    bool HasMetadata(XG_RESOURCE_LAYOUT layout)
    {
        for (auto& plane : layout.Plane)
        {
            if (IsMetadataPlane(plane))
            {
                return true;
            }
        }

        return false;
    }

    XG_RESOURCE_LAYOUT CreateLayout(D3D12_RESOURCE_DESC& desc, BindFlags flags)
    {
        // Always use 64KB alignment.
        desc.Alignment = UINT64(c_pageSizeBytes);

        // Calculate tile mode.
        XG_TILE_MODE tileMode;
        if (HasFlag(flags, BindFlags::DSV))
        {
            assert(IsDepthFormat(desc.Format));

            XG_TILE_MODE stencilTileMode;

#if _XDK_VER >= 0x3F6803F3 /* XDK Edition 170600 */
            XGComputeOptimalDepthStencilTileModes(
                XG_FORMAT(desc.Format),
                UINT32(desc.Width),
                desc.Height,
                desc.DepthOrArraySize,
                desc.SampleDesc.Count,
                (desc.Flags & D3D12XBOX_RESOURCE_FLAG_DENY_COMPRESSION_DATA) == 0,
                FALSE,
                (desc.Flags & D3D12XBOX_RESOURCE_FLAG_FORCE_TEXTURE_COMPATIBILITY) != 0,
                &tileMode,
                &stencilTileMode);
#else
            XGComputeOptimalDepthStencilTileModes(
                XG_FORMAT(desc.Format),
                UINT32(desc.Width),
                desc.Height,
                desc.DepthOrArraySize,
                desc.SampleDesc.Count,
                (desc.Flags & D3D12XBOX_RESOURCE_FLAG_DENY_COMPRESSION_DATA) == 0,
                &tileMode,
                &stencilTileMode);
#endif
        }
        else
        {
            UINT bindFlags = 0;
            if (HasFlag(flags, BindFlags::RTV)) bindFlags |= XG_BIND_RENDER_TARGET;
            if (HasFlag(flags, BindFlags::SRV)) bindFlags |= XG_BIND_SHADER_RESOURCE;
            if (HasFlag(flags, BindFlags::UAV)) bindFlags |= XG_BIND_UNORDERED_ACCESS;

            tileMode = XGComputeOptimalTileMode(
                XG_RESOURCE_DIMENSION(desc.Dimension),
                XG_FORMAT(desc.Format),
                UINT32(desc.Width),
                desc.Height,
                desc.DepthOrArraySize,
                desc.SampleDesc.Count,
                bindFlags);
        }
        desc.Layout = D3D12_TEXTURE_LAYOUT(0x100 | tileMode);

        // Generate an XG_RESOURCE_DESC from the resource properties.
        XG_RESOURCE_DESC xgDesc =
        {
            XG_RESOURCE_DIMENSION(desc.Dimension),
            desc.Alignment,
            desc.Width,
            desc.Height,
            desc.DepthOrArraySize,
            desc.MipLevels,
            XG_FORMAT(desc.Format),
            XG_SAMPLE_DESC{ desc.SampleDesc.Count, desc.SampleDesc.Quality },
            XG_TEXTURE_LAYOUT(desc.Layout),
            XG12_RESOURCE_MISC_FLAG(desc.Flags)
        };

        // Use the XG Memory library to calculate the resource layout.
        ComPtr<XGTextureAddressComputer> computer;
        DX::ThrowIfFailed(XGCreateTextureComputer(&xgDesc, &computer));

        XG_RESOURCE_LAYOUT layout;
        computer->GetResourceLayout(&layout);

        return layout;
    }
}


namespace ATG
{
    int TransientDesc::getPageCount() const
    {
        D3D12_RESOURCE_DESC copy = d3dDesc;
        XG_RESOURCE_LAYOUT layout = CreateLayout(copy, flags);

        return (int)DivRoundUp(layout.SizeBytes, UINT64(c_pageSizeBytes));
    }


    //------------------------------------------------
    // TransientCache

    TransientCache::TransientCache(ID3D12Device* device)
        : m_device(device)
        , m_rtvHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 256)
        , m_dsvHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 256)
        , m_resHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 256)
    { }

    void TransientCache::Uninitialize()
    {
        m_cache.clear();
    }

    void TransientCache::NextFrame()
    {
        ++m_frameNumber;
    }

    CachedResource* TransientCache::Get(ResourceHandle handle)
    {
        // Find the resource list for the resource type.
        auto it = m_cache.find(handle.key);
        if (it == m_cache.end())
        {
            return nullptr;
        }

        // Ensure that the referenced resource index is valid.
        auto& resourceList = it->second;
        if (handle.index >= resourceList.size())
        {
            return nullptr;
        }

        // Ensure that the referenced resource was allocated this frame.
        auto& resource = resourceList[handle.index];
        if (handle.generation != m_frameNumber)
        {
            return nullptr;
        }

        return &resource;
    }

    ResourceHandle TransientCache::Create(const TransientDesc& resKey)
    {
        assert(resKey.flags != BindFlags::None);
        if (resKey.flags == BindFlags::None)
        {
            return ResourceHandle::Invalid;
        }

        size_t key, index;
        AcquireResource(resKey, index, key);

        return ResourceHandle{ key, m_frameNumber, index };
    }

    void TransientCache::AcquireResource(const TransientDesc& desc, size_t& index, size_t& key)
    {
        // Generate the key.
        key = Hash(desc);

        // Determine whether any resource of this specification has been created already.
        auto mapIt = m_cache.find(key);
        if (mapIt == m_cache.end())
        {
            // Nope, create a cache list for the new resource type.
            mapIt = m_cache.emplace(std::make_pair(key, std::vector<CachedResource>{})).first;
        }
        auto& instanceList = mapIt->second;

        // Check for an existing instance that hasn't been used this frame yet.
        auto listIt = std::find_if(instanceList.begin(), instanceList.end(), [&](auto& x) { return x.frameNumber < m_frameNumber; });
        if (listIt == instanceList.end())
        {
            // No free instances available - create a new one to satisfy the request.
            listIt = instanceList.insert(instanceList.end(), CreateResource(desc));
        }

        // Update the frame number to reflect our usage this frame and ensure old page list is clear.
        listIt->frameNumber = m_frameNumber;
        listIt->pages.clear();

        index = listIt - instanceList.begin();
    }

    CachedResource TransientCache::CreateResource(const TransientDesc& desc)
    {
        // Determine the layout properties
        D3D12_RESOURCE_DESC d3dDesc = desc.d3dDesc;

        if (HasFlag(desc.flags, BindFlags::RTV)) d3dDesc.Flags = d3dDesc.Flags | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if (HasFlag(desc.flags, BindFlags::DSV)) d3dDesc.Flags = d3dDesc.Flags | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        if (HasFlag(desc.flags, BindFlags::UAV)) d3dDesc.Flags = d3dDesc.Flags | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        XG_RESOURCE_LAYOUT layout = CreateLayout(d3dDesc, desc.flags);

        // Allocate virtual memory for the resource.
        void* address = VirtualAlloc(nullptr, layout.SizeBytes, MEM_GRAPHICS | MEM_LARGE_PAGES | MEM_RESERVE, PAGE_READWRITE | PAGE_WRITECOMBINE);

        // Create a resource on top of the virtual memory without committing pages.
        ComPtr<ID3D12Resource> resource;
        D3D12_RESOURCE_STATES defaultState = HasFlag(desc.flags, BindFlags::DSV) ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_RENDER_TARGET;
        m_device->CreatePlacedResourceX(reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS>(address), &d3dDesc, defaultState, &desc.clear, IID_GRAPHICS_PPV_ARGS(resource.GetAddressOf()));

        // Create our cached version of the resource to be used from here on out.
        CachedResource res = {};
        res.address.reset(address);
        res.pageCount = (int)DivRoundUp(layout.SizeBytes, UINT64(c_pageSizeBytes));
        res.resource = resource;
        res.hasMetadata = HasMetadata(layout);

        //--------------------------------------------
        // Create the resource views specified in the transient resource desc.

        // Render Target View
        if (HasFlag(desc.flags, BindFlags::RTV))
        {
            size_t index = m_rtvHeap.Allocate();
            res.RTV = m_rtvHeap.GetCpuHandle(index);

            m_device->CreateRenderTargetView(resource.Get(), nullptr, res.RTV);
        }

        // Depth Stencil View
        if (HasFlag(desc.flags, BindFlags::DSV))
        {
            size_t index = m_dsvHeap.Allocate();
            res.DSV = m_dsvHeap.GetCpuHandle(index);

            m_device->CreateDepthStencilView(resource.Get(), nullptr, res.DSV);
        }

        // Shader Resource View
        if (HasFlag(desc.flags, BindFlags::SRV))
        {
            size_t index = m_resHeap.Allocate();
            res.SRV = m_resHeap.GetCpuHandle(index);

            m_device->CreateShaderResourceView(resource.Get(), nullptr, res.SRV);
        }
        
        // Unordered Access View
        if (HasFlag(desc.flags, BindFlags::UAV))
        {
            size_t index = m_resHeap.Allocate();
            res.UAV = m_resHeap.GetCpuHandle(index);

            m_device->CreateUnorderedAccessView(resource.Get(), nullptr, nullptr, res.UAV);
        }

        return res;
    }
}
