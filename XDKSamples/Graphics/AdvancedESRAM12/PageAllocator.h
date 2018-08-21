//--------------------------------------------------------------------------------------
// PageAllocator.h
//
// An allocator that maps a virtual memory space to physical pages and allocates memory 
// at a page-level granularity. This uses a templated type called a 'MappingPolicy' 
// which must provide the following interface:
// 
// Must contain static members:
//      static constexpr int s_policyId;        -- Identifier uniquely specifying the policy type.
//      static constexpr int s_blockPageCount;  -- Number of memory pages within a page block.
//      static constexpr int s_blockSizeBytes;  -- Number of bytes within a block (shortcut for s_blockPageCount * c_pageSizeBytes.)
//
// Must contain constructor:
//      MappingPolicy(size_t maxSizeBytes);
//
// Must implement methods:
//      bool CanExpand(int newBlockCount);                                                  -- Determines whether the allocator is able to expand to another block.
//      void InitBlock(ID3D12Device* device, void* address, int index, PageBlock& block);   -- Initializes a new page block.
//      void DeinitBlock(ID3D12Device* device, void* address, int index, PageBlock& block); -- Deinitializes a previously initialized page block.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

namespace ATG
{
    //
    // Represents a range of integral values.
    //
    struct Range
    {
    public:
        bool Precedes(const Range& subsequent) const;
        bool Succeeds(const Range& precedent) const;

        static bool MergeLeft(Range& left, const Range& right);
        static bool MergeRight(const Range& left, Range& right);

        static bool Disjoint(const Range& r0, const Range& r1);
        static bool Adjacent(const Range& left, const Range& right);

    public:
        int start;
        int count;
    };


    //
    // Represents a reference to a specific range of a page block.
    //
    struct PageRef
    {
        void*   baseVirtualAddress;
        int     blockPageCount;

        int     blockId;
        Range   range;
    };


    //
    // Manages the page ranges within a constantly allocated & freed block of pages.
    //
    struct PageBlock
    {
    public:
        // Returns whether the page block has fully allocated its range of pages.
        bool IsExhausted() const { return ranges.empty(); }
        // Returns whether the page block is available and fully unallocated.
        bool IsFresh() const { return ranges.size() == 1 && ranges[0].count == pagePoolCount; }
        // Attempts to allocate 'count' pages in a contiguous range.
        bool AllocateRange(int count, Range& outRange);
        // Frees a page range back to the allocator.
        void FreeRange(const Range& range);
        // Resets the allocator back to fully unallocated.
        void Reset();

    public:
        int                 pagePoolCount;
        HANDLE              pagePoolHandle;
        std::vector<Range>  ranges;
    };


    //
    // An allocator that maps a virtual memory space to physical pages and allocates memory at a page-level granularity.
    // 
    template <typename MappingPolicy>
    class PageAllocator
    {
    public:
        PageAllocator(ID3D12Device* device, void* baseVirtualAddress, size_t maxSizeBytes);
        ~PageAllocator();

        PageAllocator(const PageAllocator&) = delete;
        PageAllocator(PageAllocator&&) = default;

        PageAllocator& operator=(const PageAllocator&) = delete;
        PageAllocator& operator=(PageAllocator&&) = default;

    public:
        //
        //  Uninitializes all blocks. Required step before destruction.
        //
        void Uninitialize();

        //
        //  Attempts to allocate the specified number of pages from the allocator. Expands the allocator if necessary/possible to accomodate the requested page count.
        //
        //  Param pageCount - number of pages to attempt to allocate.
        //  Param pageRefs - (out) vector on which to append references to newly-allocated pages.
        //  Param remaining - (out_opt) if supplied, writes the remaining number of pages that weren't able to be satisfied.
        //
        void Allocate(int count, std::vector<PageRef>& pageRefs, int* remaining = nullptr);

        //
        //  Releases any pages allocated by the page allocator from the supplied page reference array.
        //
        //  Param pageRefs - vector of references to allocated pages.
        //
        void Release(const std::vector<PageRef>& pageRefs);

        //
        //  Resets blocks to fully unallocated and nullifies all page references.
        //
        void NextFrame();

        //
        //  Discards mapped page blocks that haven't been used since the previous 'Clean()' call.
        //  This allows an application to reclaim memory if a potentially memory-expensive operation 
        //  is performed rarely.
        //
        void Clean();

        //
        //  Determines the page ranges of a list of page references that originated from this allocator.
        //
        void GetRanges(const std::vector<PageRef>& pageRefs, std::vector<Range>& ranges);
        
    private:
        void* GetBlockAddress(int index) const;
        bool Expand(int blockCount);
        
    private:
        MappingPolicy           m_policy;
        ID3D12Device*           m_device;
        void*                   m_baseVirtualAddress;

        int                     m_highMark = 0;
        int                     m_freePageCount = 0;
        int                     m_blockCount = 0;
        std::vector<PageBlock>  m_pools;
    };


    //
    // Implementation of the PageAllocator's MappingPolicy that maps its virtual address space to blocks 
    // of ESRAM. Can only allocate up to 512- 64-KB pages (32 MB)
    //
    class EsramMappingPolicy
    {
    public:
        static constexpr int s_policyId = 0x0;
        static constexpr int s_blockPageCount = c_esramPageCount;
        static constexpr int s_blockSizeBytes = s_blockPageCount * c_pageSizeBytes;

    public:
        EsramMappingPolicy(size_t maxSizeBytes) 
            : m_enabled(maxSizeBytes > 0) 
        { }

    public:
        bool CanExpand(int newBlockCount) { return m_enabled && newBlockCount == 1; }
        void InitBlock(ID3D12Device* device, void* address, int index, PageBlock& block);
        void DeinitBlock(ID3D12Device* device, void* address, int index, PageBlock& block);

    private:
        bool m_enabled;
    };


    //
    // Implementation of the PageAllocator's MappingPolicy template that maps its virtual address space 
    // to blocks of DRAM. Fully expandable up to the constraints of the minimum of the system available 
    // physical memory and the supplied virtual address space.
    //
    class DramMappingPolicy
    {
    public:
        static constexpr int s_policyId = 0x1;
        static constexpr int s_blockPageCount = c_dramBlockPageCount;
        static constexpr int s_blockSizeBytes = s_blockPageCount * c_pageSizeBytes;

    public:
        DramMappingPolicy(size_t maxSizeBytes)
            : m_maxSizeBytes(maxSizeBytes)
            , m_pageCache()
        { }

        bool CanExpand(int newBlockCount);
        void InitBlock(ID3D12Device* device, void* address, int index, PageBlock& block);
        void DeinitBlock(ID3D12Device* device, void* address, int index, PageBlock& block);

    private:
        size_t                  m_maxSizeBytes;
        std::vector<ULONG_PTR>  m_pageCache;
    };


    // Less verbose shortcut definitions
    using PageAllocatorEsram = PageAllocator<EsramMappingPolicy>;
    using PageAllocatorDram = PageAllocator<DramMappingPolicy>;
}

#include "PageAllocator.inl"