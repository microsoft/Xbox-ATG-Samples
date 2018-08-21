//--------------------------------------------------------------------------------------
// PageAllocator.inl
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

namespace
{
    // Helpers for creating/parsing Block ID bits.
    constexpr int c_blockIdBits = 31;
    constexpr int c_allocatorIdMask = (~0) << c_blockIdBits;
    constexpr int c_blockIndexMask = ~c_allocatorIdMask;

    // Returns the policy identifier used to allocate the block (ESRAM or DRAM.)
    inline int GetPolicyId(int blockId)
    {
        return (blockId & c_allocatorIdMask) >> c_blockIdBits;
    }

    // Returns the index of the block in the allocator.
    inline int GetBlockIndex(int blockId)
    {
        return blockId & c_blockIndexMask;
    }

    // Creates an identifier given a policy and block index.
    inline int GetBlockId(int policyId, int blockIndex)
    {
        return (policyId << c_blockIdBits) | blockIndex;
    }

    // Helper functions
    inline bool IsAligned(void* mem, size_t alignment)
    {
        return (size_t(mem) % alignment) == 0;
    }

    inline bool IsPageAligned(void* mem)
    {
        return IsAligned(mem, c_pageSizeBytes);
    }
}

namespace ATG
{
    //--------------------------------------------------
    // PageAllocator

    template<typename MappingPolicy>
    PageAllocator<MappingPolicy>::PageAllocator(ID3D12Device* device, void* baseVirtualAddress, size_t maxSizeBytes)
        : m_policy(maxSizeBytes)
        , m_device(device)
        , m_baseVirtualAddress(baseVirtualAddress)
    {
        static_assert(MappingPolicy::s_blockPageCount > 0, "Block page count must be a valid size.");

        assert(IsPageAligned(baseVirtualAddress));
    }

    template<typename MappingPolicy>
    PageAllocator<MappingPolicy>::~PageAllocator()
    {
        Uninitialize();
    }

    template<typename MappingPolicy>
    void PageAllocator<MappingPolicy>::Uninitialize()
    {
        // Uninitialize all blocks the allocator is holding.
        for (int i = m_blockCount - 1; i >= 0; --i)
        {
            m_policy.DeinitBlock(m_device, GetBlockAddress(i), i, m_pools[i]);
            m_freePageCount -= MappingPolicy::s_blockPageCount;
        }

        m_blockCount = 0;
    }

    template<typename MappingPolicy>
    bool PageAllocator<MappingPolicy>::Expand(int blockCount)
    {
		assert(blockCount >= 0);

        // Determine if the allocator can satisfy the request.
        for (int i = 0; i < blockCount; ++i)
        {
            if (!m_policy.CanExpand(m_blockCount + 1))
            {
                return false;
            }
        }

        int index = m_blockCount;
        m_blockCount += blockCount;

        // Update the block container size if necessary.
        if (m_blockCount > m_pools.size())
        {
            m_pools.resize(m_blockCount);
        }

        // Initialize all the new blocks.
        for (int i = index; i < m_blockCount; ++i)
        {
            m_pools[i].pagePoolCount = MappingPolicy::s_blockPageCount;

            m_policy.InitBlock(m_device, GetBlockAddress(i), i, m_pools[i]);
            m_freePageCount += MappingPolicy::s_blockPageCount;
        }

        // Update the high water mark.
        m_highMark = std::max(m_highMark, m_blockCount);

        return true;
    }

    template<typename MappingPolicy>
    void PageAllocator<MappingPolicy>::Allocate(int count, std::vector<PageRef>& pageRefs, int* remaining)
    {
		assert(count >= 0);
        int remainingPages = count;

        // Determine whether we should allocate more page blocks for the request.
        if (m_freePageCount < remainingPages)
        {
            int newBlockCount = DivRoundUp(remainingPages - m_freePageCount, MappingPolicy::s_blockPageCount);
            Expand(newBlockCount);
        }

        // Iterate through the blocks allocating pages as available.
        for (int i = 0; i < m_blockCount && remainingPages > 0; ++i)
        {
            auto& block = m_pools[i];

            // Keep allocating page ranges from the block until we're done allocating, or the block is exhausted.
            Range range = {};
            while (remainingPages > 0 && block.AllocateRange(remainingPages, range))
            {
                remainingPages -= range.count;
                m_freePageCount -= range.count;

                // Populate the new reference to the allocated memory block.
                PageRef ref = {};
                ref.baseVirtualAddress = GetBlockAddress(i);
                ref.blockPageCount = MappingPolicy::s_blockPageCount;
                ref.blockId = GetBlockId(MappingPolicy::s_policyId, i);
                ref.range = range;

                pageRefs.push_back(ref);
            }
        }

        // Pass back the remaining number of pages if requested.
        if (remaining != nullptr) 
            *remaining = remainingPages;
    }

    template<typename MappingPolicy>
    void PageAllocator<MappingPolicy>::Release(const std::vector<PageRef>& pageRefs)
    {
        // Iterate through the released pages freeing up pages from this allocator as necessary.
        for (auto& ref : pageRefs)
        {
            // Ensure the page ref came from this page allocator.
            if (GetPolicyId(ref.blockId) != MappingPolicy::s_policyId) 
                continue;

            int blockIndex = GetBlockIndex(ref.blockId);
            m_pools[blockIndex].FreeRange(ref.range);

            m_freePageCount += ref.range.count;
        }
    }

    template<typename MappingPolicy>
    void PageAllocator<MappingPolicy>::NextFrame()
    {
        m_freePageCount = m_blockCount * MappingPolicy::s_blockPageCount;
        std::for_each(m_pools.begin(), m_pools.end(), [](auto& b) { b.Reset(); });
    }

    template <typename MappingPolicy>
    void PageAllocator<MappingPolicy>::Clean()
    {
        for (int i = m_blockCount - 1; i > m_highMark; --i)
        {
            m_policy.InitBlock(m_device, GetBlockAddress(index), index, m_pools[index]);
            m_freePageCount -= m_blockPageCount;
        }

        m_blockCount = m_highMark + 1;
        m_highMark = -1;
    }

    template <typename MappingPolicy>
    void PageAllocator<MappingPolicy>::GetRanges(const std::vector<PageRef>& pageRefs, std::vector<Range>& ranges)
    {
        ranges.clear();

        for (auto& ref : pageRefs)
        {
            if (GetPolicyId(ref.blockId) == MappingPolicy::s_policyId)
            {
                ranges.push_back(ref.range);
            }
        }
    }

    template<typename MappingPolicy>
    void* PageAllocator<MappingPolicy>::GetBlockAddress(int index) const
    {
        return static_cast<uint8_t*>(m_baseVirtualAddress) + MappingPolicy::s_blockSizeBytes * index;
    }
}
