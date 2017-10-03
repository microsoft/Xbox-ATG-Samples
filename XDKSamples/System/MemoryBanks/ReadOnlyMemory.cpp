//--------------------------------------------------------------------------------------
// ReadOnlyMemory.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "MemoryDemo.h"

// The code related to testing the read only address sample
namespace ATG
{
    bool MemoryDemo::TestReadOnlyBank()
    {
        m_memoryBank.ReleaseBank();
        if (!m_memoryBank.CommitSharedBanks(c_memoryBankSize, 2))
            return false;
        m_memoryBank.LockBank(1);
        __try       // This block should not throw an exception since the data should be writeable.
        {           // we are building with /EHsc which means C++ try/catch will not catch OS exceptions like null pointer dereferences.
                    // these types of exceptions require SEH to catch
            memset(m_memoryBank.get(0), 1, c_memoryBankSize);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            m_memoryBank.ReleaseBank();
            OutputDebugStringA("trying to write to read-write block of shared bank failed, it should not\n");
            return false;
        }

        bool successfullTest = false;
        __try       // This should throw an exception since the data block is now read-only
        {           // we are building with /EHsc which means C++ try/catch will not catch OS exceptions
                    // like this write access violation, so we use SEH instead to catch it
            char *bankBaseAddress = static_cast<char *> (m_memoryBank.get(1));
            for (uint32_t i = 0; i < c_memoryBankSize; i++)
            {
                bankBaseAddress[i] = 1;     //should throw access violation exception
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            successfullTest = true;
        }
        if (!successfullTest)
            throw std::logic_error("trying to write to read-only block of shared bank succeeded which is should not");
        return true;
    }
}
