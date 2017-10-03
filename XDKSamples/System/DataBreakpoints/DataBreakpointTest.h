//--------------------------------------------------------------------------------------
// DataBreakpointTest.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>

#include "OSLockable.h"

class DataBreakpointTest
{
public:
    enum class WhichTest
    {
        ExecutionTest,
        ReadTest,
        ReadWriteTest
    };
private:

    ATG::EventLockable<false, false> m_startTestEvent;      // used to signal a test should start on the test thread
    ATG::EventLockable<false, false> m_testDoneEvent;       // used to notify the main thread the test was completed
    std::atomic<bool> m_testSuccessful;                     // was the last test run successful
    std::atomic<bool> m_shutdownThread;                     // used to tell the test thread to exit, m_startTestEvent needs to be signalled as well
    std::thread *m_testThread;
    WhichTest m_currentTest;

    uint32_t m_readWriteFailVariable;                       // testing variables
    uint32_t m_writeFailVariable;

    uint32_t TestThreadFunction();

public:
    DataBreakpointTest();
    DataBreakpointTest(const DataBreakpointTest& rhs) = delete;
    DataBreakpointTest(const DataBreakpointTest&& rhs) = delete;
    DataBreakpointTest& operator=(const DataBreakpointTest& rhs) = delete;

    bool RunTest(WhichTest test);
};
