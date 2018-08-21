//--------------------------------------------------------------------------------------
// TransientCache.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "TransientResource.h"

namespace ATG
{
    using namespace Microsoft::WRL;

    struct PageRef;

    //--------------------------------------------------
    // CachedResource

    enum class BindFlags
    {
        None = 0,
        SRV = 0x1,
        UAV = 0x2,
        RTV = 0x4,
        DSV = 0x8,
    };
    inline BindFlags operator|(BindFlags l, BindFlags r) { return BindFlags(size_t(l) | size_t(r)); }
    inline BindFlags operator&(BindFlags l, BindFlags r) { return BindFlags(size_t(l) & size_t(r)); }
    inline bool HasFlag(BindFlags state, BindFlags test) { return size_t(state & test) != 0; }


    //--------------------------------------------------
    // TransientDesc

    struct TransientDesc
    {
        D3D12_RESOURCE_DESC d3dDesc;
        D3D12_CLEAR_VALUE   clear;
        BindFlags           flags;

		int getPageCount() const;
    };


    //--------------------------------------------------
    // CachedResource

    struct CachedResource
    {
        VirtualMemPtr               address;
        int                         pageCount;
        ComPtr<ID3D12Resource>      resource;
        bool                        hasMetadata;

        size_t                      frameNumber;
        std::vector<PageRef>        pages;

        D3D12_CPU_DESCRIPTOR_HANDLE RTV;
        D3D12_CPU_DESCRIPTOR_HANDLE DSV;
        D3D12_CPU_DESCRIPTOR_HANDLE SRV;
        D3D12_CPU_DESCRIPTOR_HANDLE UAV;

        D3D12_GPU_VIRTUAL_ADDRESS GpuAddress() const { return reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS>(address.get()); }
    };


    //--------------------------------------------------
    // TransientCache

    class TransientCache
    {
    public:
        TransientCache(ID3D12Device* device);
    
        void Uninitialize();
        void NextFrame();

        CachedResource* Get(ResourceHandle handle);
        CachedResource& GetChecked(ResourceHandle handle) { assert(handle != ResourceHandle::Invalid); return m_cache.at(handle.key)[handle.index]; }

        // Finds or creates an available resource matching the resource description.
        ResourceHandle Create(const TransientDesc& desc);

    private:
        void AcquireResource(const TransientDesc& desc, size_t& index, size_t& key);
        CachedResource CreateResource(const TransientDesc& desc);

    private:
        // Hashed TransientDesc (size_t) to array of cached resource instances (only allocated once per frame.)
        using Cache = std::unordered_map<size_t, std::vector<CachedResource>>;

    private:
        ID3D12Device*   m_device;
        size_t          m_frameNumber = 0;
        Cache           m_cache;

        // Descriptor heaps for the cached resource's associated views.
        DirectX::DescriptorPile m_rtvHeap; // RTV
        DirectX::DescriptorPile m_dsvHeap; // DSV
        DirectX::DescriptorPile m_resHeap; // CBV_SRV_UAV
    };
}
