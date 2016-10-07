//--------------------------------------------------------------------------------------
// DescriptorPile.h
//
// A wrapper to help sharing a descriptor heap. This class removes the need for a 
// global enumeration of all SRVs used in a sample. The pile is statically sized and will
// throw an exception if it becomes full.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once
#include <memory>
#include <DescriptorHeap.h>

namespace ATG
{
    class DescriptorPile
    {
       public:
           using IndexType = size_t;
           static const IndexType INVALID_INDEX = size_t(-1);

           DescriptorPile(
               _In_ ID3D12Device* device,
               _In_ D3D12_DESCRIPTOR_HEAP_TYPE type,
               _In_ D3D12_DESCRIPTOR_HEAP_FLAGS flags,
               size_t initialSize)
               : m_top(0)
           {
               m_heap = std::make_unique<DirectX::DescriptorHeap>(device, type, flags, initialSize);
           }

           // Return the start of a batch of descriptors, throws exception if no room
           IndexType Allocate()
           {
               IndexType start, end;
               AllocateRange(1, start, end);
               
               return start;
           }

           // Return the start of a batch of descriptors, throws exception if no room
           void AllocateRange(size_t numDescriptors, _Out_ IndexType& start, _Out_ IndexType& end)
           {
               // make sure we didn't allocate zero
               if (numDescriptors == 0)
               {
                   throw std::out_of_range("Can't allocate zero slots on DescriptorPile");
               }

               // get the current top
               start = m_top;

               // increment top with new request
               m_top += numDescriptors;
               end = m_top;

               // make sure we have enough room
               if (m_top >= m_heap->Count())
               {
                   throw std::out_of_range("DescriptorPile can't allocate more descriptors");
               }
           }

           D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(_In_ size_t index) const
           {
               return m_heap->GetGpuHandle(index);
           }

           D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(_In_ size_t index) const
           {
               return m_heap->GetCpuHandle(index);
           }

           ID3D12DescriptorHeap* Heap() const { return m_heap->Heap(); }

    private:
        std::unique_ptr<DirectX::DescriptorHeap> m_heap;
        IndexType                                m_top;
    };
}