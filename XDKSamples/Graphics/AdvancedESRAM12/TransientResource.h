//--------------------------------------------------------------------------------------
// TransientResource.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

namespace ATG
{
    //
    // A handle used to uniquely identify a transient resource.
    //
    struct ResourceHandle
    {
        size_t key;
        size_t generation;
        size_t index;

        static const ResourceHandle Invalid;
    };
    inline bool operator==(const ResourceHandle& h0, const ResourceHandle& h1) { return memcmp(&h0, &h1, sizeof(h0)) == 0; }
    inline bool operator!=(const ResourceHandle& h0, const ResourceHandle& h1) { return !(h0 == h1); }
    

    //
    //  A transient resource
    //
    struct TransientResource
    {
        ResourceHandle  handle;
        ID3D12Resource* resource;

        D3D12_CPU_DESCRIPTOR_HANDLE  RTV;
        D3D12_CPU_DESCRIPTOR_HANDLE  DSV;
        D3D12_CPU_DESCRIPTOR_HANDLE  SRV;
        D3D12_CPU_DESCRIPTOR_HANDLE  UAV;
    };
}
