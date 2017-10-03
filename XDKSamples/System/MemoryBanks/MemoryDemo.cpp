//--------------------------------------------------------------------------------------
// MemoryDemo.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "MemoryDemo.h"

namespace ATG
{
    MemoryDemo::MemoryDemo() :
        m_rootNode(nullptr)
    {
    }
    MemoryDemo::~MemoryDemo()
    {
    }

    bool MemoryDemo::RunTest(MemoryBankDemoTests whichTest)
    {
        __try   // With all of the memory access in these tests it's possible for a bad pointer dereference
        {       // we are building with /EHsc which means C++ try/catch will not catch OS exceptions like null pointer dereferences.
                // these types of exceptions require SEH to catch
            switch (whichTest)
            {
            case MemoryBankDemoTests::MEMORY_BANK_RANDOM_ADDRESS:
                return TestRandomAddress();
            case MemoryBankDemoTests::MEMORY_BANK_FIXED_ADDRESS:
                return TestFixedAddress();
            case MemoryBankDemoTests::READ_ONLY_MEMORY_BANK:
                return TestReadOnlyBank();
            case MemoryBankDemoTests::BANK_SWITCHING:
                return TestBankSwitching();
            case MemoryBankDemoTests::SHARED_ADDRESS:
                return TestSharedAddress();
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            switch (whichTest)
            {
            case MemoryBankDemoTests::MEMORY_BANK_RANDOM_ADDRESS:
                OutputDebugStringA("Testing random address bank threw an exception\n");
                break;
            case MemoryBankDemoTests::MEMORY_BANK_FIXED_ADDRESS:
                OutputDebugStringA("Testing fixed address bank threw an exception\n");
                break;
            case MemoryBankDemoTests::READ_ONLY_MEMORY_BANK:
                OutputDebugStringA("Testing read only bank threw an exception\n");
                break;
            case MemoryBankDemoTests::BANK_SWITCHING:
                OutputDebugStringA("Testing bank switching threw an exception\n");
                break;
            case MemoryBankDemoTests::SHARED_ADDRESS:
                OutputDebugStringA("Testing shared address bank threw an exception\n");
                break;
            }
        }
        return false;
    }

    // run through the entire binary tree validating all of the data
    bool MemoryDemo::ValidateData(const TestData *curNode, bool isLeft, uint32_t treeLevel)
    {
        __try   // There could be random pointer dereferencing in here on bad data
        {       // we are building with /EHsc which means C++ try/catch will not catch OS exceptions like null pointer dereferences.
                // these types of exceptions require SEH to catch
            if (curNode->m_isLeftNode != isLeft)
                return false;
            if (curNode->m_treeLevel != treeLevel)
                return false;
            if (curNode->m_left)
            {
                if (!ValidateData(curNode->m_left, true, treeLevel + 1))
                    return false;
            }
            if (curNode->m_right)
            {
                if (!ValidateData(curNode->m_right, false, treeLevel + 1))
                    return false;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            OutputDebugStringA("Validating data threw an exception\n");
            return false;
        }
        return true;
    }

    // go through each node and create new random data in the node
    bool MemoryDemo::ShuffleRandomData(TestData *curNode)
    {
        __try   // There could be random pointer dereferencing in here on bad data
        {       // we are building with /EHsc which means C++ try/catch will not catch OS exceptions like null pointer dereferences.
                // these types of exceptions require SEH to catch
            curNode->m_randomData = std::rand();
            if ((curNode->m_left) && !ShuffleRandomData(curNode->m_left))
                return false;
            if ((curNode->m_right) && !ShuffleRandomData(curNode->m_right))
                return false;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            OutputDebugStringA("shuffling random data threw an exception\n");
            return false;
        }
        return true;
    }

    // recursively create the binary tree
    bool MemoryDemo::InternalCreateTestData(TestData *curNode, uint32_t curTreeLevel, TestData* stackAllocator, size_t& stackAllocatorIndex)
    {
        curNode->m_randomData = std::rand();
        if (curTreeLevel == c_maxTestDataTreeLevel)
        {
            curNode->m_leftIndex = curNode->m_rightIndex = SIZE_MAX;
            curNode->m_left = curNode->m_right = nullptr;
            return true;
        }

        TestData *leftNode = &(stackAllocator[stackAllocatorIndex++]);
        leftNode->m_isLeftNode = true;
        leftNode->m_treeLevel = curTreeLevel;
        curNode->m_leftIndex = stackAllocatorIndex - 1;

        TestData *rightNode = &(stackAllocator[stackAllocatorIndex++]);
        rightNode->m_isLeftNode = false;
        rightNode->m_treeLevel = curTreeLevel;
        curNode->m_rightIndex = stackAllocatorIndex - 1;

        if (!InternalCreateTestData(leftNode, curTreeLevel + 1, stackAllocator, stackAllocatorIndex))
            return false;
        if (!InternalCreateTestData(rightNode, curTreeLevel + 1, stackAllocator, stackAllocatorIndex))
            return false;
        curNode->m_left = leftNode;
        curNode->m_right = rightNode;
        return true;
    }
}
