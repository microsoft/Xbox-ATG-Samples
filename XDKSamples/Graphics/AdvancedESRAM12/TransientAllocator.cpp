//--------------------------------------------------------------------------------------
// TransientAllocator.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "TransientAllocator.h"


using namespace Microsoft::WRL;

namespace
{
    constexpr uint32_t c_esramTokenMask = 0x80000000;

    inline bool isEsramToken(uint32_t token) { return (token & c_esramTokenMask) != 0; }
    inline int getCount(uint32_t token) { return int(token & ~c_esramTokenMask); }
}

namespace ATG
{
    uint32_t esramToken(uint32_t count) { return count | c_esramTokenMask; }
    uint32_t dramToken(uint32_t count) { return count & ~c_esramTokenMask; }

    //--------------------------------------------------
    // TransientAllocator

    TransientAllocator::TransientAllocator(ID3D12Device* device, uint64_t maxSizeBytes, bool useEsram)
        : m_useEsram(SupportsEsram() && useEsram)
        , m_cache(device)
    { 
        maxSizeBytes = std::max(Mebibytes(8ull), maxSizeBytes);

        void* address = VirtualAlloc(nullptr, maxSizeBytes, MEM_GRAPHICS | MEM_LARGE_PAGES | MEM_RESERVE, PAGE_READWRITE | PAGE_WRITECOMBINE);
        assert(address != nullptr);

        m_virtualAddress.reset(address);

        m_esram = std::make_unique<PageAllocatorEsram>(device, address, m_useEsram ? c_esramSizeBytes : 0);
        m_dram = std::make_unique<PageAllocatorDram>(device, m_useEsram ? static_cast<uint8_t*>(address) + c_esramSizeBytes : address, maxSizeBytes);
    }

    void TransientAllocator::Uninitialize()
    {
        m_esram.reset();
        m_dram.reset();
        m_virtualAddress.reset();

        m_cache.Uninitialize();
    }

    void TransientAllocator::NextFrame()
    {
        // Reset the page pools in the page allocators to 'full'.
        m_esram->NextFrame();
        m_dram->NextFrame();

        // Let the resource cache know we've advanced frame.
        m_cache.NextFrame();

        // Reset our cache flush state.
        m_flushState = D3D12XBOX_FLUSH_NONE;
    }

    TransientResource TransientAllocator::AcquireEsram(ID3D12GraphicsCommandList* list, const TransientDesc& desc, D3D12_RESOURCE_STATES initialState, const wchar_t* name)
    {
        uint32_t token = esramToken(~0u);
        return Acquire(list, desc, initialState, &token, 1, name);
    }

    TransientResource TransientAllocator::AcquireDram(ID3D12GraphicsCommandList* list, const TransientDesc& desc, D3D12_RESOURCE_STATES initialState, const wchar_t* name)
    {
        return Acquire(list, desc, initialState, nullptr, 0, name);
    }

    TransientResource TransientAllocator::Acquire(ID3D12GraphicsCommandList* list, const TransientDesc& desc, D3D12_RESOURCE_STATES initialState, uint32_t* tokens, int tokenCount, const wchar_t* name)
    {
        assert(list != nullptr);

        // Find or create the new resource from the resource cache.
        ResourceHandle handle = m_cache.Create(desc);
        CachedResource& res = m_cache.GetChecked(handle);

        // Allocate the pages from ESRAM & DRAM allocators.
        AllocatePages(res.pageCount, res.pages, tokens, tokenCount);
        GeneratePageMappings(res.address.get(), res.pages);

        // If this is the first allocate after a discard operation, insert a flush into the pipeline to eliminate
        // the chance that the aliased memory may be used before the previous resource is finished with it.
        if (m_flushState != D3D12XBOX_FLUSH_NONE)
        {
            list->FlushPipelineX(m_flushState, 0, D3D12XBOX_FLUSH_RANGE_ALL);

            m_flushState = D3D12XBOX_FLUSH_NONE;
        }

        // Transition the new resource to the specified state.
        D3D12_RESOURCE_STATES defaultState = HasFlag(desc.flags, BindFlags::DSV) ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_RENDER_TARGET;
        if (initialState != defaultState)
        {
            D3D12_RESOURCE_BARRIER barrier;
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = res.resource.Get();
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = defaultState;
            barrier.Transition.StateAfter = initialState;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

            list->ResourceBarrier(1, &barrier);
        }

#if _DEBUG
        res.resource->SetName(name != nullptr ? name : L"Untitled");
#else
        UNREFERENCED_PARAMETER(name);
#endif

        // Return the resource.
        return TransientResource {
            handle,
            res.resource.Get(),
            res.RTV,
            res.DSV,
            res.SRV,
            res.UAV,
        };
    }

    void TransientAllocator::Release(ID3D12GraphicsCommandList* list, const TransientResource& resource, D3D12_RESOURCE_STATES finalState)
    {
        Release(list, resource.handle, finalState);
    }

    void TransientAllocator::Release(ID3D12GraphicsCommandList* list, ResourceHandle handle, D3D12_RESOURCE_STATES finalState)
    {
        assert(list != nullptr);

        CachedResource& res = m_cache.GetChecked(handle);

        // Transition the resource back to its default state before discard.
        D3D12_RESOURCE_STATES defaultState = res.DSV.ptr != 0 ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_RENDER_TARGET;
        if (finalState != defaultState)
        {
            D3D12_RESOURCE_BARRIER barrier;
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = res.resource.Get();
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = finalState;
            barrier.Transition.StateAfter = defaultState;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

            list->ResourceBarrier(1, &barrier);
        }

        // Relinquish the pages back to the allocators.
        m_esram->Release(res.pages);
        m_dram->Release(res.pages);


        //--------------------------------------------------------------------------------------------------
        // Determine which shaders and caches should be flushed based on the discarded resource's usage.

        // RTVs are attached to pixel shaders and writes through the color block for both color & meta data.
        if (res.RTV.ptr != 0)
        {
            m_flushState |= D3D12XBOX_FLUSH_BOP_PS_PARTIAL;
            m_flushState |= D3D12XBOX_FLUSH_BOP_COLOR_BLOCK_DATA;

            // Only need to flush the metadata if the resource has metadata.
            if (res.hasMetadata)
            {
                m_flushState |= D3D12XBOX_FLUSH_BOP_COLOR_BLOCK_META;
            }
        }

        // DSVs are attached to pixel shaders and writes through the depth block for both depth & meta data.
        if (res.DSV.ptr != 0)
        {
            m_flushState |= D3D12XBOX_FLUSH_BOP_PS_PARTIAL;
            m_flushState |= D3D12XBOX_FLUSH_BOP_DEPTH_BLOCK_DATA;

            // Only need to flush the metadata if the resource has metadata.
            if (res.hasMetadata)
            {
                m_flushState |= D3D12XBOX_FLUSH_BOP_DEPTH_BLOCK_META;
            }
        }

        // UAVs can be attached to both pixel & compute shaders and writes through the L1 and L2 caches.
        if (res.UAV.ptr != 0)
        {
            m_flushState |= D3D12XBOX_FLUSH_BOP_PS_PARTIAL | D3D12XBOX_FLUSH_BOP_CS_PARTIAL;
            m_flushState |= D3D12XBOX_FLUSH_BOP_TEXTURE_L2_INVALIDATE;
        }

        // SRVs can be read from both pixel & compute shaders.
        if (res.SRV.ptr != 0)
        {
            m_flushState |= D3D12XBOX_FLUSH_BOP_PS_PARTIAL | D3D12XBOX_FLUSH_BOP_CS_PARTIAL;
        }
    }

    void TransientAllocator::Finalize(ID3D12CommandQueue* queue)
    {
        assert(queue != nullptr);

        // Fix up step:
        // An index offset was stored in a pointer variable during command list recording 
        // since vector reallocation could occur which would invalidate a pointer. As 
        // recording has concluded it's safe to convert the indices back to pointers.
        for (auto& block : m_pools)
        {
            block.pBatches = m_batches.data() + reinterpret_cast<size_t>(block.pBatches);

            for (int i = 0; i < block.batchCount; ++i)
            {
                auto& batch = block.pBatches[i];
                batch.pRanges = m_ranges.data() + reinterpret_cast<size_t>(batch.pRanges);
            }
        }

        // Execute the page mapping copy calls on the queue - one call for each page pool.
        for (auto& block : m_pools)
        {
            queue->CopyPageMappingsBatchX(
                UINT(block.batchCount),
                block.pBatches,
                reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS>(block.pagePoolAddress),
                UINT(block.pagePoolCount),
                D3D12XBOX_PAGE_MAPPING_FLAG_NONE);
        }

        // Reset block, batch, and range lists.
        m_pools.clear();
        m_batches.clear();
        m_ranges.clear();
    }

    void TransientAllocator::GetEsramRanges(ResourceHandle handle, std::vector<Range>& ranges)
    {
        m_esram->GetRanges(m_cache.GetChecked(handle).pages, ranges);
    }

    void TransientAllocator::AllocatePages(int pageCount, std::vector<PageRef>& pageRefs, uint32_t* tokens, int tokenCount)
    {
        // Only use ESRAM is specified.
        if (m_useEsram && tokens != nullptr)
        {
            int remaining = 0;

            // Allocate until we're out of tokens, we've allocated all pages, or our ESRAM allocator is exhausted.
            for (int i = 0; i < tokenCount && pageCount > 0 && remaining == 0; ++i)
            {
                uint32_t numPages = std::min(pageCount, getCount(tokens[i]));

                if (isEsramToken(tokens[i]))
                {
                    m_esram->Allocate(numPages, pageRefs, &remaining);
                }
                else
                {
                    m_dram->Allocate(numPages, pageRefs, &remaining);
                }

                pageCount -= (numPages - remaining);
            }
        }

        // Grab the remaining pages from DRAM.
        m_dram->Allocate(pageCount, pageRefs);
    }

    void TransientAllocator::GeneratePageMappings(void* address, const std::vector<PageRef>& pageRefs)
    {
        //------------------------------------------------------------------------------------------------
        // Prepare the page mapping structures for the ID3D12CommandQueue::CopyPageMappingsBatchX(...) call.
        // 
        // Bit of an intimidating procedure for what it actually accomplishes. Just loops over the referenced
        // memory pages, creates a new page pool mapping if the page pool hasn't been referenced yet, creates a
        // new batch if the resource at 'address' doesn't have an existing mapping into the page pool yet, 
        // and appends the new page ranges.
        //
        // We know that this is the first and only time the resource will be referenced this frame, and that pages 
        // are allocated in a known order (ESRAM -> DRAM 0 -> DRAM 1 -> etc.) so we use that knowledge to accelerate 
        // our search & insertion.

        size_t offset = 0; // Current byte offset into the resource's virtual memory space.

        int poolIndex = 0;
        int batchIndex = -1;

        for (const PageRef& page : pageRefs)
        {
            // Since pages are always allocated in order of page pools, we need only iterate 
            // forward from the current entry to find the next referenced page pool.
            for (; poolIndex < m_pools.size(); ++poolIndex)
            {
                if (m_pools[poolIndex].pagePoolAddress == page.baseVirtualAddress)
                    break;

                // Since this resource is encountering a new page pool we know a batch
                // hasn't yet been created for it.
                batchIndex = -1;
            }

            // If we didn't find an existing entry for this page's pool we must create a new one.
            if (poolIndex == m_pools.size())
            {
                PagePoolMapping& pool = *m_pools.emplace(m_pools.end());
                pool.pagePoolAddress   = page.baseVirtualAddress;
                pool.pagePoolCount     = page.blockPageCount;
                pool.pBatches          = reinterpret_cast<D3D12XBOX_PAGE_MAPPING_BATCH*>(m_batches.size()); // Temporary index-based reference since the vector may reallocate.
                pool.batchCount        = 0;
            }
            PagePoolMapping& pool = m_pools[poolIndex];

            // If this is the first page referenced in this page pool.
            if (batchIndex < 0)
            {
                batchIndex = (int)reinterpret_cast<size_t>(pool.pBatches) + pool.batchCount;

                D3D12XBOX_PAGE_MAPPING_BATCH& batch = *m_batches.emplace(m_batches.begin() + batchIndex);
                batch.DestinationAddress    = reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS>(address) + offset;
                batch.pRanges               = reinterpret_cast<D3D12XBOX_PAGE_MAPPING_RANGE*>(m_ranges.size());  // Temporary index-based reference since the vector may reallocate.
                batch.RangeCount            = 0;

                ++pool.batchCount;

                // Since a new batch was inserted into the batch vector, we've shifted the batches of all blocks after this one.
                for (int i = poolIndex + 1; i < m_pools.size(); ++i)
                {
                    m_pools[i].pBatches = reinterpret_cast<D3D12XBOX_PAGE_MAPPING_BATCH*>(reinterpret_cast<size_t>(m_pools[i].pBatches) + 1);
                }
            }

            // Add the page range to the list.
            D3D12XBOX_PAGE_MAPPING_RANGE& range = *m_ranges.emplace(m_ranges.end());
            range.RangeType             = D3D12XBOX_PAGE_MAPPING_RANGE_TYPE_INCREMENTING_PAGE_INDICES;
            range.PageCount             = page.range.count;
            range.StartPageIndexInPool  = page.range.start;

            ++m_batches[batchIndex].RangeCount;

            // Add the byte offset of the page
            offset += range.PageCount * c_pageSizeBytes;
        }
    }
}
