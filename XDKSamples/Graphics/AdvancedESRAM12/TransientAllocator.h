//--------------------------------------------------------------------------------------
// TransientAllocator.h
//
// Description:
//  
//
// Requirements:
// - TransientAllocator::Finalize(...) must be called BEFORE the command list is submitted to the same command queue.
// - TransientAllocator::NextFrame() must occur before the first allocation or after the last finalize event of the frame.
//
// Implementation Details:
// - A large virtual address space is reserved on initialization which serves as a staging area for mapping blocks of physical memory.
// - Two page allocators (PageAllocator.h) manage pools of 64KB pages for ESRAM & DRAM separately. DRAM pools are only allocated as required.
// - On instantiation each resource only allocates virtual memory for itself. 
// - Requested resources are mapped to physical pages allocated by the page allocators at the time of request.
// - Discarded resources immediately return their mapped pages back to the page allocators to be reallocated on subsequent requests.
// - Potentially aliased memory is detected and pipeline flushes are issued liberally to avoid simultaneous memory R/W & invalid cache accesses.
//
// An example usage scenario for this class would be:
//
//  ... frame starts ...
//  allocator->NextFrame();
//
//  auto res0 = allocator->AcquireEsram(cmdList, desc0, ...);
//  auto res1 = allocator->AcquireDram(cmdList, desc1, ...);
//
//  ... draws or compute here ...
//
//  allocator->Release(cmdList, res0, ...);
//  auto res2 = allocator->Acquire(cmdList, desc2, ...);
//
//  ... draws or compute here ...
//
//  allocator->Release(cmdList, res2, ...);
//  allocator->Release(cmdList, res1, ...);
//
//  allocator->Finalize(cmdQueue);
//  cmdQueue->ExecuteCommandLists(1, &cmdList);
//
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "PageAllocator.h"
#include "TransientResource.h"
#include "TransientCache.h"

namespace ATG
{
    //--------------------------------------------------
    // Helper functions for creating ESRAM & DRAM tokens used for specifying page allocations.
    
    // Creates a token specifying a number of ESRAM pages.
    uint32_t esramToken(uint32_t count);

    // Creates a token specifying a number of DRAM pages.
    uint32_t dramToken(uint32_t count);


    //
    // A D3D resource allocator that doles out transient (short-lived) resources. 
    //
    class TransientAllocator
    {
    public:
        TransientAllocator(ID3D12Device* device, uint64_t maxSizeBytes, bool useEsram = true);

        TransientAllocator(const TransientAllocator&) = delete;
        TransientAllocator(TransientAllocator&&) = default;

        TransientAllocator& operator=(const TransientAllocator&) = delete;
        TransientAllocator& operator=(TransientAllocator&&) = default;

    public:
        //
        //  Uninitializes the allocator, releasing all D3D and OS resources.
        //
        void Uninitialize();

        //
        //  Advances the allocator to the next frame. This is separate than Finalize, since multiple Finalize may happen multiple times per frame.
        //
        void NextFrame();

        //
        //  Allocates a D3D resource prefering ESRAM, but falling back to DRAM when exhuasted.
        //
        //  Param list - (in) command list the resource where the resource will be referenced.
        //  Param desc - descriptor for the resource to create.
        //  Param initialState - state the resource should initially be in when returned.
        //  Param name - (in_opt) name to specify as the resource's debug name.
        //
        TransientResource AcquireEsram(ID3D12GraphicsCommandList* list, const TransientDesc& desc, D3D12_RESOURCE_STATES initialState, const wchar_t* name = nullptr);

        //
        //  Allocates a D3D resource completely in DRAM.
        //
        //  Param list - (in) command list the resource where the resource will be referenced.
        //  Param desc - descriptor for the resource to create.
        //  Param initialState - state the resource should initially be in when returned.
        //  Param name - (in_opt) name to specify as the resource's debug name.
        //
        TransientResource AcquireDram(ID3D12GraphicsCommandList* list, const TransientDesc& desc, D3D12_RESOURCE_STATES initialState, const wchar_t* name = nullptr);

        //
        //  Allocates a D3D resource according to a list of tokens specifying a consecutive number of pages from either ESRAM or DRAM, 
        //  falling back to DRAM when exhausted. If no tokens are specified the allocations default to DRAM.
        //
        //  Param list - (in) command list the resource where the resource will be referenced.
        //  Param desc - descriptor for the resource to create.
        //  Param initialState - state the resource should initially be in when returned.
        //  Param tokens - (int_opt) list of tokens (created with esramToken() or dramToken()) which specify the number of pages to allocate contiguously.
        //  Param tokenCount - length of the 'token' parameter. Not read if tokens is nullptr.
        //  Param name - (in_opt) name to specify as the resource's debug name.
        //
        TransientResource Acquire(ID3D12GraphicsCommandList* list, const TransientDesc& desc, D3D12_RESOURCE_STATES initialState, uint32_t* tokens, int tokenCount, const wchar_t* name = nullptr);

        //
        //  Discards a previously allocated resource, relinquishing its pages to the allocator for subsequent use.
        //
        //  Param list - (in) command list the resource where the resource was referenced.
        //  Param resource - the resource allocated with the Create*(...) call.
        //  Param finalState - current state of the resource.
        //
        void Release(ID3D12GraphicsCommandList* list, const TransientResource& resource, D3D12_RESOURCE_STATES finalState);

        //
        //  Discards a previously allocated resource, relinquishing its pages to the allocator for subsequent use.
        //
        //  Param list - (in) command list the resource where the resource was referenced.
        //  Param handle - handle to the resource allocated with the Create*(...) call.
        //  Param finalState - current state of the resource.
        //
        void Release(ID3D12GraphicsCommandList* list, ResourceHandle handle, D3D12_RESOURCE_STATES finalState);

        //
        //  Submits its page mappings to the specified queue - a necessary step BEFORE the command list is submitted to the queue.
        //
        //  Param queue - (in) the command queue to which the command list will be submitted.
        //
        void Finalize(ID3D12CommandQueue* queue);

        //
        //  Determines the page ranges of a resource that originated from the ESRAM allocator.
        //
        void GetEsramRanges(ResourceHandle handle, std::vector<Range>& ranges);

    private:
        void AllocatePages(int pageCount, std::vector<PageRef>& pageRefs, uint32_t* tokens, int tokenCount);
        void GeneratePageMappings(void* address, const std::vector<PageRef>& pageRefs);

    private:
        VirtualMemPtr                               m_virtualAddress;
        bool                                        m_useEsram;

        // Simple cache that caches created D3D12 resources for reuse - eliminates overhead incurred by redundant resource allocations.
        TransientCache                              m_cache;

        // Separate allocators for ESRAM & DRAM to allow page-level granularity of allocations.
        std::unique_ptr<PageAllocatorEsram>         m_esram;
        std::unique_ptr<PageAllocatorDram>          m_dram;
        D3D12XBOX_FLUSH                             m_flushState = D3D12XBOX_FLUSH_NONE;

    private:
        // Represents the accumulated individual resource page mappings from a single page pool.
        struct PagePoolMapping
        {
            void*                                   pagePoolAddress;
            int                                     pagePoolCount;
            D3D12XBOX_PAGE_MAPPING_BATCH*           pBatches;
            int                                     batchCount;
        };

        // Incremental staging data structures for submitting page mapping commands to the ID3D12CommandQueue.
        std::vector<PagePoolMapping>                m_pools;
        std::vector<D3D12XBOX_PAGE_MAPPING_BATCH>   m_batches;
        std::vector<D3D12XBOX_PAGE_MAPPING_RANGE>   m_ranges;
    };
}
