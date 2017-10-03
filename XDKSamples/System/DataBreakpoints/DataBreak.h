//--------------------------------------------------------------------------------------
// DataBreak.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <cstdint>
namespace ATG
{
    namespace DataBreak
    {
        enum class ADDRESS_SIZE {
            SIZE_1,                 // 1 byte
            SIZE_2,                 // 2 bytes
            SIZE_4,                 // 4 bytes
            SIZE_8                  // 8 bytes
        };
        enum class ADDRESS_OPERATION
        {
            EXECUTION,              // A specific address was accessed for execution. This only applies to the first byte of a full instruction
            READ_WRITE,             // An address was either read from or written to
            WRITE,                  // An address was written to
        };

        enum class DEBUG_REGISTER   // There are only four slots available for hardware breakpoints
        {
            REGISTER_1,
            REGISTER_2,
            REGISTER_3,
            REGISTER_4,
        };

        //************************************
        // Method:    SetHardwareBreakPointForThread
        // FullName:  ATG::DataBreak::SetHardwareBreakPointForThread
        // Returns:   bool indicating success or failure
        // Parameter: void * thread Which thread to set the breakpoint for, can be the current thread, Windows HANDLE object
        // Parameter: void * address The location in memory of the breakpoint
        // Parameter: DEBUG_REGISTER debugRegister Which debug register slot to use on the processor, only 4 hardware breakpoints are supported
        // Parameter: ADDRESS_OPERATION addressOperation What type of operation to break on
        // Parameter: ADDRESS_SIZE addressSize How big is the memory location in questions, 1 byte, 2 bytes, etc.
        //************************************
        bool SetHardwareBreakPointForThread(void *thread, void *address, DEBUG_REGISTER debugRegister, ADDRESS_OPERATION addressOperation, ADDRESS_SIZE addressSize);

        //************************************
        // Method:    ClearHardwareBreakPointForThread
        // FullName:  ATG::DataBreak::ClearHardwareBreakPointForThread
        // Returns:   bool indicating success or failure
        // Parameter: void * thread Which thread to clear the breakpoint for, can be the current thread
        // Parameter: DEBUG_REGISTER debugRegister Which debug register to use on the processor, only 4 hardware breakpoints are supported
        //************************************
        bool ClearHardwareBreakPointForThread(void *thread, DEBUG_REGISTER debugRegister);
    }
}
