//--------------------------------------------------------------------------------------
// DataBreakpointTest.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "DataBreakpointTest.h"
#include "DataBreak.h"
#include "algorithm"
#include "dbghelp.h"
#include <mutex>

DataBreakpointTest::DataBreakpointTest() :
    m_testSuccessful(false)
    , m_testThread(nullptr)
    , m_shutdownThread(false)
{
}

// Test function for the execution breakpoint. This function cannot be inlined otherwise there would be no entry point to use as the address
__declspec (noinline) uint32_t ExecuteTestFunction()
{
    uint32_t temporaryVariableToForceCode;
    temporaryVariableToForceCode = 6;
    return temporaryVariableToForceCode;
}

LONG WINAPI GenerateDump(EXCEPTION_POINTERS* pExceptionPointers)
{
    static ATG::CriticalSectionLockable mutex(100);

    std::lock_guard<ATG::CriticalSectionLockable> mutexGuard(mutex);   // MiniDumpWriteDump is single threaded, must synchronize concurrent calls

    BOOL bMiniDumpSuccessful;
    HANDLE hDumpFile;
    MINIDUMP_EXCEPTION_INFORMATION ExpParam;

    std::string fileName("t:\\dataBreak.dmp");

    hDumpFile = CreateFileA(fileName.c_str(), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

    ExpParam.ThreadId = GetCurrentThreadId();
    ExpParam.ExceptionPointers = pExceptionPointers;
    ExpParam.ClientPointers = TRUE;

    // MiniDumpWriteDump will not actually use the exception pointers record for a single step exception
    // It will include the stack all the way down to GetThreadContext during the call to MiniDumpWriteDump
    // You will find the offending code in the call stack, but it will be further up.
    // Both SEH and the unhandled exception filter are called in the context of the thread with exception.
    bMiniDumpSuccessful = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpNormal, &ExpParam, NULL, NULL);

    CloseHandle(hDumpFile);

    return EXCEPTION_EXECUTE_HANDLER;
}

uint32_t DataBreakpointTest::TestThreadFunction()
{
    std::atomic<uint32_t> tempVariable(0);
    for (;;)
    {
        m_startTestEvent.wait();
        if (m_shutdownThread)
            return tempVariable;

        m_testSuccessful = false;
        __try
        {
            switch (m_currentTest)
            {
            case WhichTest::ExecutionTest:
                tempVariable = ExecuteTestFunction();
                break;
            case WhichTest::ReadTest:
                m_writeFailVariable = 5;
                break;
            case WhichTest::ReadWriteTest:
                tempVariable = m_readWriteFailVariable;
                break;
            }
        }
        __except (GenerateDump(GetExceptionInformation()))
        {
            m_testSuccessful = true;
        }
        m_testDoneEvent.signal();
    }
}

bool DataBreakpointTest::RunTest(WhichTest test)
{
    if (m_testThread == nullptr)
    {
        m_testThread = new std::thread(&DataBreakpointTest::TestThreadFunction, this);

        // setup a read/write breakpoint in slot 1 on m_readWriteFailVariable on the test thread
        SetHardwareBreakPointForThread(m_testThread->native_handle(), &m_readWriteFailVariable, ATG::DataBreak::DEBUG_REGISTER::REGISTER_1, ATG::DataBreak::ADDRESS_OPERATION::READ_WRITE, ATG::DataBreak::ADDRESS_SIZE::SIZE_4);

        // setup a write breakpoint in slot 2 on m_writeFailVariable on the test thread, reading will not trigger the breakpoint
        SetHardwareBreakPointForThread(m_testThread->native_handle(), &m_writeFailVariable, ATG::DataBreak::DEBUG_REGISTER::REGISTER_2, ATG::DataBreak::ADDRESS_OPERATION::WRITE, ATG::DataBreak::ADDRESS_SIZE::SIZE_4);

        // setup an execution breakpoint for when ExecuteTestFunction was called, however the real function entry point is needed
        unsigned char *funcEntry = reinterpret_cast<unsigned char *> (ExecuteTestFunction);

        // incremental linking works through a jump table. The caller calls to an address that just has a jump instruction to the real function entry point
        // we need to detect this and correct the address to find the real function entry for this sample
#ifdef _DEBUG                       // going to assume incremental linking is enabled for this sample only in debug builds
        if (*funcEntry == 0xe9)     // a good chance this is the jmp instruction for the jump table
        {
            uint32_t *offset = reinterpret_cast<uint32_t *> (funcEntry + 1);
            funcEntry += *offset;           // this is a relative jump and not an absolute one
            funcEntry += 5;                 // however it's relative to the address after the jump instruction which is 5 bytes long
        }
        else
        {
            assert(false);                  // wasn't what we expected for the jump table and the sample, it's safe to continue past this, the breakpoint for execution may or may not work.
        }
#else       // This sample has incremental linking disabled for release builds so there should not be a jump table
        assert(*funcEntry != 0xe9);
#endif
        SetHardwareBreakPointForThread(m_testThread->native_handle(), reinterpret_cast<void *> (funcEntry), ATG::DataBreak::DEBUG_REGISTER::REGISTER_3, ATG::DataBreak::ADDRESS_OPERATION::EXECUTION, ATG::DataBreak::ADDRESS_SIZE::SIZE_4);
    }
    OutputDebugStringW(L"Did a test");

    m_currentTest = test;
    m_testSuccessful = false;

    m_startTestEvent.signal();          // start the test and wait for it to complete
    m_testDoneEvent.wait();
    return m_testSuccessful;
}
