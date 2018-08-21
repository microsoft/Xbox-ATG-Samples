//--------------------------------------------------------------------------------------
// PageAllocator.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "PageAllocator.h"

namespace ATG
{
    //--------------------------------------------------
    // PageRange

    bool Range::Precedes(const Range& subsequent) const
    {
        assert(Disjoint(*this, subsequent)); // No page range should intersect each other.

        return start + count <= subsequent.start;
    }

    bool Range::Succeeds(const Range& precedent) const
    {
        // Equivalent to a reversed 'Precedes' test.
        return precedent.Precedes(*this);
    }

    bool Range::MergeLeft(Range& left, const Range& right)
    {
        assert(Disjoint(left, right));

        if (left.start + left.count == right.start)
        {
            left.count += right.count;
            return true;
        }

        return false;
    }

    bool Range::MergeRight(const Range& left, Range& right)
    {
        if (left.start + left.count == right.start)
        {
            right.start = left.start;
            right.count += left.count;
            return true;
        }

        return false;
    }

    bool Range::Disjoint(const Range& r0, const Range& r1)
    {
        // Ensure this range is either entirely on the left or the right side.
        return (r0.start + r0.count) <= r1.start || r0.start >= (r1.start + r1.count);
    }

    bool Range::Adjacent(const Range& left, const Range& right)
    {
        assert(Disjoint(left, right));
        return (left.start + left.count) == right.start;
    }


    //--------------------------------------------------
    // PageBlock
    
    bool PageBlock::AllocateRange(int count, Range& outRange)
    {
        if (IsExhausted())
        {
            return false;
        }

        auto& range = ranges[0];

        // Allocate pages from the current range.
        int allocStart = range.start;
        int allocCount = std::min(count, range.count);

        // Perform the allocation.
        count -= allocCount;
        range.start += allocCount;
        range.count -= allocCount;

        // Erase the range if it just became exhausted.
        if (range.count == 0)
        {
            ranges.erase(ranges.begin());
        }

        outRange.start = allocStart;
        outRange.count = allocCount;
        return true;
    }

    void PageBlock::FreeRange(const Range& range)
    {
        // Find the correct location for the released range within the block.
        int i = 0;
        while (i < ranges.size() && ranges[i].Precedes(range))
        {
            ++i;
        }

        // Determine whether to merge with existing block, or create a new range.
        bool merged = false;
        if (i != 0)
        {
            // Try merging with the range to the left.
            merged = Range::MergeLeft(ranges[i - 1], range);
        }

        if (i != ranges.size())
        {
            // Try merging with the range to the right.
            merged = merged || Range::MergeRight(range, ranges[i]);
        }

        if (!merged)
        {
            // No merge occurred, insert a new range.
            ranges.insert(ranges.begin() + i, range);
        }
    }

    void PageBlock::Reset()
    {
        ranges.resize(1);
        ranges[0] = { 0, pagePoolCount };
    }


    //------------------------------------
    // EsramMappingPolicy
    
    void EsramMappingPolicy::InitBlock(ID3D12Device* device, void* address, int index, PageBlock& block)
    {
        UNREFERENCED_PARAMETER(index);

        // Map the virtual range to the entirety of ESRAM.
        UINT pages[s_blockPageCount];
        for (int i = 0; i < s_blockPageCount; ++i)
        {
            pages[i] = i;
        }
        DX::ThrowIfFailed(D3DMapEsramMemory(D3D11_MAP_ESRAM_LARGE_PAGES, address, s_blockPageCount, pages));

        // Create the new page pool for the block.
        HANDLE handle = {};
        D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS>(address);
        DX::ThrowIfFailed(device->RegisterPagePoolX(gpuAddress, s_blockPageCount, &handle));

        block.pagePoolHandle = handle;
        block.Reset();
    }

    void EsramMappingPolicy::DeinitBlock(ID3D12Device* device, void* address, int index, PageBlock& block)
    {
        UNREFERENCED_PARAMETER(index);

        // Release the page pool.
        device->UnregisterPagePoolX(block.pagePoolHandle);
        block.pagePoolHandle = NULL;
        block.ranges.clear();

        DX::ThrowIfFailed(D3DUnmapEsramMemory(D3D11_MAP_ESRAM_LARGE_PAGES, address, s_blockPageCount));
    }


    //------------------------------------
    // DramMappingPolicy
    
    bool DramMappingPolicy::CanExpand(int newBlockCount)
    {
        return s_blockSizeBytes * newBlockCount <= m_maxSizeBytes;
    }

    void DramMappingPolicy::InitBlock(ID3D12Device* device, void* address, int index, PageBlock& block)
    {
        // Allocate some memory to maintain list of allocated physical pages - needed to release the pages.
        int start = index * s_blockPageCount;

        int end = start + s_blockPageCount;
        if (end > m_pageCache.size())
        {
            m_pageCache.resize(end);
        }

        // Allocate physical memory
        ULONG_PTR count = c_dramBlockPageCount;
        PULONG_PTR pagePtr = m_pageCache.data() + start;

        BOOL success = AllocateTitlePhysicalPages(GetCurrentProcess(), MEM_LARGE_PAGES | MEM_GRAPHICS, &count, pagePtr);
        if (!success)
        {
            throw std::exception("AllocateTitlePhysicalPages failed to allocate physical pages!");
        }

        // Map the block's virtual address space to the physical system pages.
        void* blockAddress = MapTitlePhysicalPages(address, count, MEM_LARGE_PAGES | MEM_GRAPHICS, PAGE_READWRITE | PAGE_WRITECOMBINE, pagePtr);
        if (blockAddress == nullptr)
        {
            throw std::exception("MapTitlePhysicalPages failed to map physical pages!");
        }

        // Create the new page pool for the block.
        HANDLE handle = {};
        DX::ThrowIfFailed(device->RegisterPagePoolX(reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS>(blockAddress), c_dramBlockPageCount, &handle));

        block.pagePoolHandle = handle;
        block.Reset();
    }

    void DramMappingPolicy::DeinitBlock(ID3D12Device* device, void* address, int index, PageBlock& block)
    {
        UNREFERENCED_PARAMETER(index);

        // Release the page pool.
        device->UnregisterPagePoolX(block.pagePoolHandle);

        // Decommit the memory.
        VirtualFree(address, s_blockSizeBytes, MEM_DECOMMIT);

        // Free the physical pages back to the OS.
        PULONG_PTR pagePtr = m_pageCache.data() + (m_pageCache.size() - s_blockPageCount);
        FreeTitlePhysicalPages(GetModuleHandle(NULL), s_blockPageCount, pagePtr);
    }
}
