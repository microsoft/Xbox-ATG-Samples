//--------------------------------------------------------------------------------------
// BankSwitchingMemory.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "MemoryDemo.h"

namespace ATG
{
    bool MemoryDemo::TestBankSwitching()
    {
        try
        {
            m_memoryBank.ReleaseBank();
            if (!m_memoryBank.CommitRotateBanks(c_memoryBankSize * sizeof(uint32_t), 2))
                throw std::logic_error("Unable to commit rotated banks");
            uint32_t *bank0 = reinterpret_cast<uint32_t *> (m_memoryBank.get(0));
            uint32_t *bank1 = reinterpret_cast<uint32_t *> (m_memoryBank.get(1));
            for (uint32_t i = 0; i < c_memoryBankSize; i++)
            {
                bank0[i] = i;
                bank1[i] = (1 << 24) + i;
            }

            m_memoryBank.SwapBanks(0, 1);

            for (uint32_t i = 0; i < c_memoryBankSize; i++)
            {
                if (bank1[i] != i)
                    throw std::logic_error("Bank switching failed with incorrect data in the original first bank");
                if (bank0[i] != ((1 << 24) + i))
                    throw std::logic_error("Bank switching failed with incorrect data in the original second bank");
            }
        }
        catch (const std::logic_error& what)
        {
            OutputDebugStringA(what.what());
            m_memoryBank.ReleaseBank();
            return false;
        }
        m_memoryBank.ReleaseBank();
        return true;
    }
}
