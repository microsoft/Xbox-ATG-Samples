//--------------------------------------------------------------------------------------
// MemoryBank.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

// Creates and manages a memory bank.
// Can either support a basic single memory bank
// A shared memory bank where multiple virtual pages are mapped to the same physical page
// A bank that supports page swapping or rotating. Where virtual addresses are moved between multiple physical addresses
class MemoryBank
{
private:
    enum class BankType
    {
        UNDEFINED,
        BASIC_BANK,
        SHARED_BANK,
        ROTATE_BANK,
    };

    // mapping physical pages to virtual pages requires either 64k or 4MB pages for both
    static const size_t c_SIZE_PER_LARGE_PHYSICAL_PAGE = (64 * 1024);
    static const size_t c_SIZE_PER_4MB_PHYSICAL_PAGE = (4 * 1024 * 1024);

    BankType m_bankType;
    void **m_banks;         // virtual address of each bank in this group
    size_t m_numberOfBanks;
    size_t m_bankSize;

    // used for mapping physical pages
    size_t m_numberOfPhysicalBanks;
    size_t **m_physicalPageArray;           // list of 64k pages for each bank
    size_t m_numberOfPhysicalPagesPerBank;  // physical pages are maintained as 64k pages

public:
    MemoryBank(const MemoryBank&) = delete;
    MemoryBank& operator=(const MemoryBank&) = delete;
    MemoryBank(MemoryBank&& rhs) = default;

    MemoryBank() :m_banks(nullptr), m_physicalPageArray(nullptr), m_numberOfPhysicalBanks(0), m_numberOfPhysicalPagesPerBank(0), m_numberOfBanks(0), m_bankSize(0), m_bankType(BankType::UNDEFINED) {}
    ~MemoryBank() { ReleaseBank(); }

    // creates a single basic memory bank with the option to create it at a system defined address or an address of choice
    // There is not underlying track of the physical pages since this is a basic memory bank created through VirtualAlloc
    bool CommitBank(size_t bankSize, uintptr_t baseAddressDesired = 0)
    {
        assert(!m_banks);
        assert(!m_physicalPageArray);
        try
        {
            // The basic memory bank only supports one bank, there is no sharing or rotating
            m_numberOfBanks = 1;
            m_banks = new void *[m_numberOfBanks];
            m_bankSize = bankSize;
            // If a specific address is desired then the address range needs to be reserved first, it's not possible to reserve and commit in the same call
            m_banks[0] = VirtualAlloc(reinterpret_cast<void *> (baseAddressDesired), bankSize, MEM_RESERVE, PAGE_READWRITE);
            if (m_banks[0] == nullptr)
                throw std::logic_error("Failed to reserve virtual memory when creating basic bank");

            // Once the address is reserved it can be committed.
            m_banks[0] = VirtualAlloc(m_banks[0], bankSize, MEM_COMMIT, PAGE_READWRITE);
            if (m_banks[0] == nullptr)
                throw std::logic_error("Failed to commit virtual memory when creating basic bank");
        }
        catch (const std::exception& /*except*/)
        {
            return false;
        }
        m_bankType = BankType::BASIC_BANK;
        return true;
    }

    // Creates a shared memory bank. Each bank shares the same backing physical pages
    // Shared banks can be created with adjacent virtual addresses.
    //   This is very useful for creating ring buffers without the need to break memory copies across boundaries.
    bool CommitSharedBanks(size_t bankSize, size_t numberOfBanks = 1, bool adjacentBanks = false, uintptr_t baseAddressDesired = 0)
    {
        try
        {
            m_numberOfBanks = numberOfBanks;
            m_numberOfPhysicalBanks = 1;
            m_numberOfPhysicalPagesPerBank = bankSize / c_SIZE_PER_LARGE_PHYSICAL_PAGE;
            m_physicalPageArray = new size_t *[1];
            m_physicalPageArray[0] = new size_t[m_numberOfPhysicalPagesPerBank];

            size_t actualPagesAllocated = m_numberOfPhysicalPagesPerBank;
            // The first step is to allocate the physical pages for the bank.
            // Physical pages need to be allocated as either 64k or 4MB contiguous pages. However the OS will return them as an array of 64k page addresses
            if (!AllocateTitlePhysicalPages(GetCurrentProcess(), bankSize < c_SIZE_PER_4MB_PHYSICAL_PAGE ? MEM_LARGE_PAGES : MEM_4MB_PAGES, &actualPagesAllocated, m_physicalPageArray[0]))
            {
                delete[] m_physicalPageArray;
                m_physicalPageArray = nullptr;
                throw std::logic_error("Failed to allocate physical pages when creating shared banks");
            }
            // It's possible when allocating physical pages that a smaller amount is returned. Consider this a failure for this sample
            if (actualPagesAllocated < m_numberOfPhysicalPagesPerBank)
            {
                FreeTitlePhysicalPages(GetCurrentProcess(), actualPagesAllocated, m_physicalPageArray[0]);
                m_physicalPageArray[0] = nullptr;
                throw std::logic_error("Failed to allocate the requested number of physical pages when creating shared banks");
            }
            m_bankSize = actualPagesAllocated * (bankSize < c_SIZE_PER_4MB_PHYSICAL_PAGE ? c_SIZE_PER_LARGE_PHYSICAL_PAGE : c_SIZE_PER_4MB_PHYSICAL_PAGE);
            m_banks = new void *[m_numberOfBanks];
            memset(m_banks, 0, sizeof(uintptr_t) * m_numberOfBanks);

            // If the virtual banks have been requested to be adjacent then a single virtual address range needs to be reserved
            if (adjacentBanks)
            {
                void *baseVirtualAddress;
                // reserve the full virtual address range, it can be reserved at a known location or let the OS decide
                baseVirtualAddress = VirtualAlloc(reinterpret_cast<void *> (baseAddressDesired), m_bankSize*m_numberOfBanks, MEM_RESERVE + (bankSize < c_SIZE_PER_4MB_PHYSICAL_PAGE ? MEM_LARGE_PAGES : MEM_4MB_PAGES), PAGE_READWRITE);
                if (baseVirtualAddress == nullptr)
                    throw std::logic_error("Failed to reserve virtual memory when creating basic bank");
                for (uint32_t i = 0; i < m_numberOfBanks; i++)
                {
                    void *bankVirtualAddress = (reinterpret_cast<char*>(baseVirtualAddress)) + (m_bankSize * i);
                    // map each of the physical pages into a section of the virtual address range, physical pages can be mapped multiple times
                    m_banks[i] = MapTitlePhysicalPages(bankVirtualAddress, m_numberOfPhysicalPagesPerBank, m_bankSize < c_SIZE_PER_4MB_PHYSICAL_PAGE ? MEM_LARGE_PAGES : MEM_4MB_PAGES, PAGE_READWRITE, m_physicalPageArray[0]);
                    if (m_banks[i] == nullptr)
                    {
                        throw std::logic_error("Failed to map the physical pages when creating adjacent shared banks");
                    }
                }
            }
            else
            {
                for (uint32_t i = 0; i < m_numberOfBanks; i++)
                {
                    m_banks[i] = MapTitlePhysicalPages(nullptr, m_numberOfPhysicalPagesPerBank, bankSize < c_SIZE_PER_4MB_PHYSICAL_PAGE ? MEM_LARGE_PAGES : MEM_4MB_PAGES, PAGE_READWRITE, m_physicalPageArray[0]);
                    if (m_banks[i] == nullptr)
                    {
                        throw std::logic_error("Failed to map the physical pages when creating shared banks");
                    }
                }
            }
        }
        catch (const std::exception& /*except*/)
        {
            ReleaseBank();
            return false;
        }
        m_bankType = BankType::SHARED_BANK;
        return true;
    }

    // Creates a set of banks that can be rotated or bank swapped, virtual addresses can be swapped between each physical bank
    // This is very useful to remove memory copies in certain patterns
    bool CommitRotateBanks(size_t bankSize, size_t numberOfBanks)
    {
        try
        {
            m_numberOfBanks = numberOfBanks;
            m_numberOfPhysicalBanks = numberOfBanks;
            m_numberOfPhysicalPagesPerBank = bankSize / c_SIZE_PER_LARGE_PHYSICAL_PAGE;
            m_physicalPageArray = new size_t *[m_numberOfPhysicalBanks];
            memset(m_physicalPageArray, 0, sizeof(uintptr_t) * m_numberOfPhysicalBanks);

            for (uint32_t i = 0; i < m_numberOfBanks; i++)
            {
                size_t actualPagesAllocated = m_numberOfPhysicalPagesPerBank;
                m_physicalPageArray[i] = new size_t[m_numberOfPhysicalPagesPerBank];
                // The first step is to allocate the physical pages for the bank.
                // Physical pages need to be allocated as either 64k or 4MB contiguous pages. However the OS will return them as an array of 64k page addresses
                if (!AllocateTitlePhysicalPages(GetCurrentProcess(), bankSize < c_SIZE_PER_4MB_PHYSICAL_PAGE ? MEM_LARGE_PAGES : MEM_4MB_PAGES, &actualPagesAllocated, m_physicalPageArray[i]))
                {
                    throw std::logic_error("Failed to allocate physical pages when creating rotating banks");
                }
                // It's possible when allocating physical pages that a smaller amount is returned. Consider this a failure for this sample
                if (actualPagesAllocated < m_numberOfPhysicalPagesPerBank)
                {
                    FreeTitlePhysicalPages(GetCurrentProcess(), actualPagesAllocated, m_physicalPageArray[i]);
                    m_physicalPageArray[i] = nullptr;
                    throw std::logic_error("Failed to allocate requested number of physical pages when creating rotating banks");
                }
                m_bankSize = actualPagesAllocated * (bankSize < c_SIZE_PER_4MB_PHYSICAL_PAGE ? c_SIZE_PER_LARGE_PHYSICAL_PAGE : c_SIZE_PER_4MB_PHYSICAL_PAGE);
            }
            m_banks = new void *[m_numberOfBanks];
            for (uint32_t i = 0; i < m_numberOfBanks; i++)
            {
                // allocate a virtual address range for each physical bank
                m_banks[i] = MapTitlePhysicalPages(nullptr, m_numberOfPhysicalPagesPerBank, bankSize < c_SIZE_PER_4MB_PHYSICAL_PAGE ? MEM_LARGE_PAGES : MEM_4MB_PAGES, PAGE_READWRITE, m_physicalPageArray[i]);
                if (m_banks[i] == nullptr)
                    throw std::logic_error("Failed to map the physical pages when creating rotating banks");
            }
        }
        catch (const std::exception& /*except*/)
        {
            ReleaseBank();
            return false;
        }
        m_bankType = BankType::ROTATE_BANK;
        return true;
    }

    void ReleaseBank()
    {
        if (m_banks)
        {
            for (size_t i = 0; i < m_numberOfBanks; i++)
            {
                if (m_banks[i])  // it's possible for this to fail for adjacent pages, however they are all released with the first page
                    VirtualFree(m_banks[i], 0, MEM_RELEASE);
            }
            delete[] m_banks;
            m_banks = nullptr;
        }

        if (m_physicalPageArray)
        {
            for (size_t i = 0; i < m_numberOfPhysicalBanks; i++)
            {
                if (m_physicalPageArray[i])
                    FreeTitlePhysicalPages(GetCurrentProcess(), m_numberOfPhysicalPagesPerBank, m_physicalPageArray[i]);
            }
            delete[] m_physicalPageArray;
            m_physicalPageArray = nullptr;
        }
    }

    // swap the virtual address for two physical banks
    bool SwapBanks(size_t bankIndex1, size_t bankIndex2)
    {
        assert(m_bankType == BankType::ROTATE_BANK);
        assert(bankIndex1 <= m_numberOfBanks);
        assert(bankIndex2 <= m_numberOfBanks);
        try
        {
            void *bankAddress1 = m_banks[bankIndex1];
            void *bankAddress2 = m_banks[bankIndex2];
            // The first step is to release the virtual addresses used by the two physical banks
            // This does not actually release the physical memory because it was allocated through AllocateTitlePhysicalPages
            // Since the physical address is still allocated it's contents are not changed
            if (!VirtualFree(bankAddress1, 0, MEM_RELEASE))
                throw std::logic_error("Failed to release bank 1 in swap banks");
            if (!VirtualFree(bankAddress2, 0, MEM_RELEASE))
                throw std::logic_error("Failed to release bank 2 in swap banks");
            for (uint32_t i = 0; i < m_numberOfPhysicalPagesPerBank; i++)
            {
                std::swap(m_physicalPageArray[bankIndex1][i], m_physicalPageArray[bankIndex2][i]);
            }

            // Remap the two physical banks with the swapped virtual addresses.
            void *newBank1Address = MapTitlePhysicalPages(bankAddress1, m_numberOfPhysicalPagesPerBank, m_bankSize < c_SIZE_PER_4MB_PHYSICAL_PAGE ? MEM_LARGE_PAGES : MEM_4MB_PAGES, PAGE_READWRITE, m_physicalPageArray[bankIndex1]);
            if (newBank1Address != m_banks[bankIndex1])
                throw std::logic_error("Failed to remap bank 1 in swap banks");
            void *newBank2Address = MapTitlePhysicalPages(bankAddress2, m_numberOfPhysicalPagesPerBank, m_bankSize < c_SIZE_PER_4MB_PHYSICAL_PAGE ? MEM_LARGE_PAGES : MEM_4MB_PAGES, PAGE_READWRITE, m_physicalPageArray[bankIndex2]);
            if (newBank2Address != m_banks[bankIndex2])
                throw std::logic_error("Failed to remap bank 2 in swap banks");
        }
        catch (const std::exception /*except*/)
        {
            ReleaseBank();
            return false;
        }
        return true;
    }

    // In many cases it's useful to convert a block of memory to read-only
    // This is useful for static data that is created once and then doesn't change throughout it's lifetime
    // Any attempt to change the memory will result in an immediate exception
    // When using shared banks (multiple virtual addresses to the same physical address) each virtual address can have different protection flags
    // This means when accessing memory through one address it's read/write, however it's read-only through a different address.
    bool LockBank(size_t bankIndex = SIZE_MAX)  // SIZE_MAX means lock all banks
    {
        assert(m_banks);
        if (bankIndex == SIZE_MAX)
        {
            bool toret = true;
            for (size_t i = 0; i < m_numberOfBanks; i++)
            {
                DWORD oldProtect;
                if (m_banks[i])
                {
                    if (VirtualProtect(m_banks[i], m_bankSize, PAGE_READONLY, &oldProtect) == 0)
                        toret = false;
                }
            }
            return toret;
        }
        else
        {
            assert(bankIndex <= m_numberOfBanks);
            DWORD oldProtect;
            if (m_banks[bankIndex] == nullptr)
                return false;
            return VirtualProtect(m_banks[bankIndex], m_bankSize, PAGE_READONLY, &oldProtect) != 0;
        }
    }

    operator void * () const { assert(m_banks); return m_banks[0]; }
    void *get(size_t bankIndex = 0) const { assert(bankIndex <= m_numberOfBanks); return m_banks[bankIndex]; }
};