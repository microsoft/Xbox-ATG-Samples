//--------------------------------------------------------------------------------------
// MemoryDemo.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "MemoryBank.h"

namespace ATG
{
    // Basic binary tree being used as test data
    struct TestData
    {
        bool m_isLeftNode;                  // flag used during validation
        uint32_t m_randomData;              // Just a block of random data used during validation, used as a scratch space for writing
        uint32_t m_treeLevel;               // The level within the tree for this node
        size_t m_leftIndex, m_rightIndex;   // Index within the memory block for the left and right nodes, used to fixup addresses when loaded from disk
        TestData *m_left, *m_right;         // Pointers to the left and right node
    };

    enum class MemoryBankDemoTests
    {
        MEMORY_BANK_RANDOM_ADDRESS, // Standard block of memory where the OS decides the address
        MEMORY_BANK_FIXED_ADDRESS,  // Block of memory where the title chooses the base address for the block of memory
        BANK_SWITCHING,             // Several blocks of memory that are page swapped and validated
        SHARED_ADDRESS,             // A single block of physical memory with adjacent virtual addresses used as a ring buffer
        READ_ONLY_MEMORY_BANK,      // A block of memory where the sample attempts to write to read-only memory
    };

    class MemoryDemo
    {
    private:
        static const size_t c_memoryBankSize = 128 * 1024;
        static const uint32_t c_maxTestDataTreeLevel = 10;
        static const size_t c_testDataAllocatorSize = 1 << c_maxTestDataTreeLevel;
        static char *c_fixedTestDataFilename;
        static char *c_randomTestDataFilename;

        MemoryBank m_memoryBank;
        TestData *m_rootNode;

        bool TestRandomAddress();
        bool TestFixedAddress();
        bool TestReadOnlyBank();
        bool TestBankSwitching();
        bool TestSharedAddress();
        bool ValidateData(const TestData *curNode, bool isLeft, uint32_t treeLevel);
        bool ShuffleRandomData(TestData *curNode);

        bool InternalCreateTestData(TestData *curNode, uint32_t curTreeLevel, TestData* stackAllocatorBase, size_t& stackAllocatorIndex);

        bool LoadFixedAddress();
        bool LoadRandomAddress();
        void FixupRandomAddresses(TestData *curNode, TestData* stackAllocatorBase);

    public:
        MemoryDemo();
        ~MemoryDemo();

        bool CreateFixedTestDataFile();
        bool CreateRandomTestDataFile();

        MemoryDemo(const MemoryDemo&) = delete;
        MemoryDemo& operator=(const MemoryDemo&) = delete;
        MemoryDemo(MemoryDemo&& rhs) = default;

        bool RunTest(MemoryBankDemoTests whichTest);
    };
}
