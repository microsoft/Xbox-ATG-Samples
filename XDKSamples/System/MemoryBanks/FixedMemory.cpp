//--------------------------------------------------------------------------------------
// FixedMemory.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "MemoryDemo.h"

namespace ATG
{
    char *MemoryDemo::c_fixedTestDataFilename = "t:\\staticMemoryFixedTestData.dat";

    // Load the binary tree data from disk and then validate the data
    // The binary tree is allocated from the single bank allocated at a fixed address
    bool MemoryDemo::TestFixedAddress()
    {
        try
        {
            m_memoryBank.ReleaseBank();
            if (!LoadFixedAddress())
                throw std::logic_error("Unable to load fixed address data");
            if (!ValidateData(m_rootNode, false, 0))
                throw std::logic_error("Failed to validate fixed address data");
        }
        catch (const std::logic_error what)
        {
            OutputDebugStringA(what.what());
            m_memoryBank.ReleaseBank();
            return false;
        }
        return true;
    }
    // load the binary tree from disk, since the tree is allocated from one memory bank a single read from disk is enough
    // Since the memory bank is allocated at the same address as the data was saved there is no need to fixup the internal addresses
    // They are already correct since data was loaded back into the same base address as it was saved
    bool MemoryDemo::LoadFixedAddress()
    {
        FILE *file = nullptr;
        try
        {
            fopen_s(&file, c_fixedTestDataFilename, "rb");
            if (!file)
                throw std::logic_error("unable to open data file\n");
            uintptr_t savedAddress;
            if (fread(&savedAddress, sizeof(savedAddress), 1, file) != 1)
                throw std::logic_error("unable to read fixed data file\n");

            if (!m_memoryBank.CommitBank(c_testDataAllocatorSize * sizeof(TestData), savedAddress))
                throw std::logic_error("unable to commit memory at fixed memory address\n");
            if (m_memoryBank.get() != reinterpret_cast<void *> (savedAddress))
                throw std::logic_error("committed memory at the wrong address in Loading fixed data\n");

            if (fread(m_memoryBank, sizeof(TestData), c_testDataAllocatorSize, file) != c_testDataAllocatorSize)
                throw std::logic_error("unable to read fixed data file\n");
            fclose(file);
            m_rootNode = reinterpret_cast<TestData *> (m_memoryBank.get());
        }
        catch (const std::exception& what)
        {
            OutputDebugStringA(what.what());
            if (file)
                fclose(file);
            m_memoryBank.ReleaseBank();
            m_rootNode = nullptr;
            return false;
        }
        return true;
    }

    bool MemoryDemo::CreateFixedTestDataFile()
    {
        FILE *file = nullptr;
        TestData *stackAllocator = nullptr;
        try
        {
            // Allocate a block of memory and let the OS pick it's location
            stackAllocator = reinterpret_cast<TestData *> (VirtualAlloc(nullptr, sizeof(TestData) * c_testDataAllocatorSize, MEM_RESERVE, PAGE_READWRITE));
            if (!stackAllocator)
                return false;
            stackAllocator = reinterpret_cast<TestData *> (VirtualAlloc(stackAllocator, sizeof(TestData) * c_testDataAllocatorSize, MEM_COMMIT, PAGE_READWRITE));
            // Save off the address for later loading of the data

            size_t stackAllocatorIndex = 0;
            TestData *rootNode = &(stackAllocator[stackAllocatorIndex++]);
            if (!InternalCreateTestData(rootNode, 1, stackAllocator, stackAllocatorIndex))
                throw std::logic_error("unable to create test tree\n");

            fopen_s(&file, c_fixedTestDataFilename, "wb");
            if (!file)
                throw std::logic_error("unable to create data file\n");
            if (fwrite(&stackAllocator, sizeof(stackAllocator), 1, file) != 1)
                throw std::logic_error("unable to save fixed data file\n");
            if (fwrite(stackAllocator, sizeof(TestData), c_testDataAllocatorSize, file) != c_testDataAllocatorSize)
                throw std::logic_error("unable to save fixed data file\n");
            fclose(file);
            VirtualFree(stackAllocator, 0, MEM_RELEASE);
        }
        catch (const std::exception& what)
        {
            OutputDebugStringA(what.what());
            if (file)
                fclose(file);
            if (stackAllocator)
                VirtualFree(stackAllocator, 0, MEM_RELEASE);
            return false;
        }

        return true;
    }
}
