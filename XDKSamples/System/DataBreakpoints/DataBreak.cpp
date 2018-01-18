//--------------------------------------------------------------------------------------
// DataBreak.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "DataBreak.h"
#include <thread>
#include "windows.h"

namespace ATG
{
    namespace DataBreak
    {
        struct DataBreakThreadParams
        {
            HANDLE threadHandle;                // Which thread to set the breakpoint on
            uintptr_t address;                  // The location in memory for the breakpoint
            DEBUG_REGISTER debugRegister;       // Which of the four register slots to modify
            ADDRESS_SIZE addressSize;           // How many bytes for the range used by the breakpoint
            ADDRESS_OPERATION addressOperation; // Should this be an execution, read/write, or write breakpoint
            bool addDebugUsage;                 // Add the breakpoint or clear the breakpoint
            bool success;                       // Was the operation successful
        };

        // The debug registers can only be accessed from Ring 0 which not available to user mode code
        // The trick is to get the kernel to modify the registers for user mode code.
        // This is done through modifying the thread context. The context can only be modified when the thread is suspended
        // This sample creates a unique thread to perform the modification and DataBreakThread is the thread function
        void DataBreakThread(DataBreakThreadParams& params)
        {
            CONTEXT threadContext;
            threadContext.ContextFlags = CONTEXT_DEBUG_REGISTERS;           // We're only interested in the debug registers
            params.success = false;
            if (SuspendThread(params.threadHandle) == ((DWORD)-1))          // The thread must be suspended to query its context without getting bogus data
                return;

            if (GetThreadContext(params.threadHandle, &threadContext) == 0)
            {
                ResumeThread(params.threadHandle);
                return;
            }

            // debug registers 0,1,2,3 hold the address for slot 1,2,3,4
            // debug register 7 contains the control data for how to interpret each slot

            if (!params.addDebugUsage)                          // Clearing the debug register slot
            {
                switch (params.debugRegister)                   // This code uses local breakpoints which are bits 0,2,4,6 for slot 1,2,3,4
                {                                               // On a task switch these flags are swapped with the value from the new task
                case DEBUG_REGISTER::REGISTER_1:                // Bits 1,3,5,7 represent global breakpoint mode. This enables breakpoints for all tasks
                    threadContext.Dr7 &= ~(1 << 0);             // However Windows sanitizes these flags when setting the thread context and clears the global bits
                    threadContext.Dr0 = 0;
                    break;
                case DEBUG_REGISTER::REGISTER_2:
                    threadContext.Dr7 &= ~(1 << 2);
                    threadContext.Dr1 = 0;
                    break;
                case DEBUG_REGISTER::REGISTER_3:
                    threadContext.Dr7 &= ~(1 << 4);
                    threadContext.Dr2 = 0;
                    break;
                case DEBUG_REGISTER::REGISTER_4:
                    threadContext.Dr7 &= ~(1 << 6);
                    threadContext.Dr3 = 0;
                    break;
                }
            }
            else
            {
                if (params.addressOperation == ADDRESS_OPERATION::EXECUTION)    // all execution breakpoints require length 1 for later flags
                    params.addressSize = ADDRESS_SIZE::SIZE_1;

                switch (params.addressSize)                     // The address for a breakpoint needs to be aligned to the requested size
                {
                case ADDRESS_SIZE::SIZE_2:
                    params.address &= ~0x01;
                    break;
                case ADDRESS_SIZE::SIZE_4:
                    params.address &= ~0x03;
                    break;
                case ADDRESS_SIZE::SIZE_8:
                    params.address &= ~0x07;
                    break;
                }

                threadContext.Dr6 = 0;                              // debug register 6 represents the status when an exception is fired. These flags should be cleared in this case
                threadContext.Dr7 |= (1 << 8);                      // Bits 8 tells the processor to report the exact instruction that triggered the breakpoint for local breakpoints
                uint32_t registerIndex = 0;
                switch (params.debugRegister)                       // This code uses local breakpoints which are bits 0,2,4,6 for slot 1,2,3,4
                {                                                   // On a task switch these flags are swapped with the value from the new task
                case DEBUG_REGISTER::REGISTER_1:                    // Bits 1,3,5,7 represent global breakpoint mode. This enables breakpoints for all tasks
                    threadContext.Dr7 |= (1 << 0);                  // However Windows sanitizes these flags when setting the thread context and clears the global bits
                    threadContext.Dr0 = (DWORD64)params.address;
                    registerIndex = 0;
                    break;
                case DEBUG_REGISTER::REGISTER_2:
                    threadContext.Dr7 |= (1 << 2);
                    threadContext.Dr1 = (DWORD64)params.address;
                    registerIndex = 1;
                    break;
                case DEBUG_REGISTER::REGISTER_3:
                    threadContext.Dr7 |= (1 << 4);
                    threadContext.Dr2 = (DWORD64)params.address;
                    registerIndex = 2;
                    break;
                case DEBUG_REGISTER::REGISTER_4:
                    threadContext.Dr7 |= (1 << 6);
                    threadContext.Dr3 = (DWORD64)params.address;
                    registerIndex = 3;
                    break;
                }

                threadContext.Dr7 &= ~((3 << 18) << (registerIndex * 4));       // Bits 18-19, 22-23, 26-27, 30-31 represent the address length for slot 1,2,3,4
                switch (params.addressSize)
                {
                case ADDRESS_SIZE::SIZE_1:
                    threadContext.Dr7 |= ((0 << 18) << (registerIndex * 4));
                    break;
                case ADDRESS_SIZE::SIZE_2:
                    threadContext.Dr7 |= ((1 << 18) << (registerIndex * 4));
                    break;
                case ADDRESS_SIZE::SIZE_4:
                    threadContext.Dr7 |= ((3 << 18) << (registerIndex * 4));
                    break;
                case ADDRESS_SIZE::SIZE_8:
                    threadContext.Dr7 |= ((2 << 18) << (registerIndex * 4));
                    break;
                }
                threadContext.Dr7 &= ~((3 << 16) << (registerIndex * 4));       // Bits 16-17, 20-21, 24-25, 28-29 represent the type of breakpoint for slot 1,2,3,4
                switch (params.addressOperation)
                {
                case ADDRESS_OPERATION::EXECUTION:
                    threadContext.Dr7 |= ((0 << 16) << (registerIndex * 4));
                    break;
                case ADDRESS_OPERATION::WRITE:
                    threadContext.Dr7 |= ((1 << 16) << (registerIndex * 4));
                    break;
                case ADDRESS_OPERATION::READ_WRITE:
                    threadContext.Dr7 |= ((3 << 16) << (registerIndex * 4));
                    break;
                }
            }

            // only if we were able to set the new thread context then it was a success
            if (SetThreadContext(params.threadHandle, &threadContext) != 0)
                params.success = true;
            ResumeThread(params.threadHandle);
        }

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
        bool SetHardwareBreakPointForThread(HANDLE thread, void *address, DEBUG_REGISTER debugRegister, ADDRESS_OPERATION addressOperation, ADDRESS_SIZE addressSize)
        {
            DataBreakThreadParams params;
            params.threadHandle = thread;
            params.address = reinterpret_cast<uintptr_t> (address);
            params.addDebugUsage = true;
            params.debugRegister = debugRegister;
            params.addressSize = addressSize;
            params.addressOperation = addressOperation;
            params.success = false;
            if (GetCurrentThreadId() == GetThreadId(thread)) // Setting the breakpoint for the current thread requires another thread to perform the action
            {
                std::thread *dataBreakThread;

                dataBreakThread = new std::thread(DataBreakThread, std::ref(params));
                dataBreakThread->join();
            }
            else
            {
                DataBreakThread(params);
            }
            return params.success;
        }

        //************************************
        // Method:    ClearHardwareBreakPointForThread
        // FullName:  ATG::DataBreak::ClearHardwareBreakPointForThread
        // Returns:   bool indicating success or failure
        // Parameter: void * thread Which thread to clear the breakpoint for, can be the current thread
        // Parameter: DEBUG_REGISTER debugRegister Which debug register to use on the processor, only 4 hardware breakpoints are supported
        //************************************
        bool ClearHardwareBreakPointForThread(HANDLE thread, DEBUG_REGISTER debugRegister)
        {
            DataBreakThreadParams params;

            params.threadHandle = thread;
            params.address = 0;
            params.addDebugUsage = false;
            params.debugRegister = debugRegister;
            params.addressSize = ADDRESS_SIZE::SIZE_1;
            params.addressOperation = ADDRESS_OPERATION::READ_WRITE;
            params.success = false;
            if (GetCurrentThreadId() == GetThreadId(thread)) // clearing the breakpoint for the current thread requires another thread to perform the action
            {
                std::thread *dataBreakThread;

                dataBreakThread = new std::thread(DataBreakThread, std::ref(params));
                dataBreakThread->join();
            }
            else
            {
                DataBreakThread(params);
            }
            return params.success;
        }
    }
}
