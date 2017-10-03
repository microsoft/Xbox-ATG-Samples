//--------------------------------------------------------------------------------------
// SharedMemory.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "MemoryDemo.h"

// The code related to testing the shared address sample
namespace ATG
{
    bool MemoryDemo::TestSharedAddress()
    {
        __try   // There could be random pointer dereferencing in here on bad data
        {       // we are building with /EHsc which means C++ try/catch will not catch OS exceptions like null pointer dereferences.
                // these types of exceptions require SEH to catch
            m_memoryBank.ReleaseBank();
            m_memoryBank.CommitSharedBanks(c_memoryBankSize, 2, false);
            memset(m_memoryBank.get(0), 1, c_memoryBankSize);
            if (memcmp(m_memoryBank.get(0), m_memoryBank.get(1), c_memoryBankSize) != 0)
            {
                OutputDebugStringA("shared memory banks are not equal\n");
                return false;
            }

            m_memoryBank.ReleaseBank();
            m_memoryBank.CommitSharedBanks(c_memoryBankSize * sizeof(uint32_t), 2, true);

            uint32_t *baseAddress = reinterpret_cast<uint32_t *> (m_memoryBank.get(0));
            //write to memory so that is writes off the end of the buffer and into start due to the shared adjacent addresses
            for (uint32_t i = 0; i < c_memoryBankSize; i++)
            {
                baseAddress[i + (c_memoryBankSize / 2)] = i;
            }
            for (uint32_t i = 0; i < c_memoryBankSize; i++)
            {
                if (baseAddress[i] != ((i + (c_memoryBankSize / 2)) % c_memoryBankSize))
                {
                    OutputDebugStringA("Testing adjacent shared memory banks did not wrap correctly\n");
                    return false;
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            OutputDebugStringA("Testing shared memory banks threw an exception\n");
            return false;
        }

        return true;
    }
}
