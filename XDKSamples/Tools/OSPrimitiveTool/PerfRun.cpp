//--------------------------------------------------------------------------------------
// PerfRun.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"

#include "PerfRun.h"
#include "Logging\FileLogger.h"

PerfRun::PerfRun() :
    m_startTestRun(false)
    , m_receiverReady(false)
    , m_senderReady(false)
    , m_receiverDone(false)
    , m_senderDone(false)
    , m_workerShutdown(false)
    , m_senderThread(nullptr)
    , m_receiverThread(nullptr)
    , m_acquireLogfile(nullptr)
    , m_releaseLogFile(nullptr)
{
    m_semaphore = CreateSemaphoreEx(nullptr, 1, 1, nullptr, 0, SEMAPHORE_ALL_ACCESS);
    m_event = CreateEvent(nullptr, false, true, nullptr);
    m_mutex = CreateMutex(nullptr, false, nullptr);
    InitializeSRWLock(&m_srw);
    InitializeCriticalSection(&m_critSection);
    InitializeConditionVariable(&m_conditionVariable);
    m_waitAddress = 0;

    KernelFuncs[PERFORM_SEMAPHORE] = KernelFuncPair(std::bind(ReleaseSemaphore, m_semaphore, 1, nullptr), std::bind(WaitForSingleObject, m_semaphore, INFINITE));
    KernelFuncs[PERFORM_EVENT] = KernelFuncPair(std::bind(SetEvent, m_event), std::bind(WaitForSingleObject, m_event, INFINITE));
    KernelFuncs[PERFORM_MUTEX] = KernelFuncPair(std::bind(ReleaseMutex, m_mutex), std::bind(WaitForSingleObject, m_mutex, INFINITE));
    KernelFuncs[PERFORM_SRW] = KernelFuncPair(std::bind(ReleaseSRWLockExclusive, &m_srw), std::bind(AcquireSRWLockExclusive, &m_srw));
    KernelFuncs[PERFORM_CRITICAL_SECTION] = KernelFuncPair(std::bind(LeaveCriticalSection, &m_critSection), std::bind(EnterCriticalSection, &m_critSection));
    KernelFuncs[PERFORM_WAIT_ADDRESS] = KernelFuncPair(nullptr, nullptr);
    KernelFuncs[PERFORM_CONDITION_SRW] = KernelFuncPair(std::bind(ReleaseSRWLockExclusive, &m_srw), std::bind(AcquireSRWLockExclusive, &m_srw));
    KernelFuncs[PERFORM_CONDITION_CS] = KernelFuncPair(std::bind(LeaveCriticalSection, &m_critSection), std::bind(EnterCriticalSection, &m_critSection));
}

PerfRun::~PerfRun()
{
    if (m_senderThread)
        m_senderThread->join();
    if (m_receiverThread)
        m_receiverThread->join();
    delete m_senderThread;
    delete m_receiverThread;
    CloseHandle(m_semaphore);
    CloseHandle(m_event);
    CloseHandle(m_mutex);
    DeleteCriticalSection(&m_critSection);
    delete m_acquireLogfile;
    delete m_releaseLogFile;
}

void PerfRun::OpenLogFiles(bool noContention)
{
    if (m_acquireLogfile == nullptr)
    {
        std::wstring logName;
        if (noContention)
        {
            logName = L"Primitive_NoContention";
        }
        else
        {
            logName = L"Primitive_Contention";
        }
        m_acquireLogfile = new ATG::DebugLog::FileLogger(logName, false);
        {
            std::wstring textBuffer;
            textBuffer = L"Run,Average,Median,Minimum,Maximum,STD,Average us,Median us,Minimum us,Maximum us,STD us,Timings";
            m_acquireLogfile->Log(textBuffer);
        }
    }
    if (m_releaseLogFile == nullptr)
    {
        std::wstring releaseName;
        if (noContention)
        {
            releaseName = L"Primitive_Release_NoContention";
        }
        else
        {
            releaseName = L"Primitive_Release_Contention";
        }
        m_releaseLogFile = new ATG::DebugLog::FileLogger(releaseName, false);
        {
            std::wstring textBuffer;
            textBuffer = L"Run,Average,Median,Minimum,Maximum,STD,Average us,Median us,Minimum us,Maximum us,STD us,Timings";
            m_releaseLogFile->Log(textBuffer);
        }
    }
}

void PerfRun::RunTests(uint32_t senderCore, uint32_t receiverCore, TestType whichTest, bool noContention, uint32_t idleWorkers)
{
    OpenLogFiles(noContention);
    m_startTestRun = false;
    m_receiverReady = false;
    m_senderReady = false;
    m_receiverDone = false;
    m_senderDone = false;
    m_senderThread = nullptr;
    m_receiverThread = nullptr;
    uint64_t processAffinityMask;
    uint64_t systemAffinityMask;
    GetProcessAffinityMask(GetCurrentProcess(), &processAffinityMask, &systemAffinityMask);
    SetThreadAffinityMask(GetCurrentThread(), processAffinityMask & ~(1 << senderCore) & ~(1 << receiverCore));

    m_workerShutdown = false;
    if (idleWorkers > 0)
    {
        m_workerThreads.clear();
        for (uint32_t i = 0; i < s_MaxCore; i++)
        {
            for (uint32_t j = 0; j < idleWorkers; j++)
            {
                m_workerThreads.push_back(new std::thread(&PerfRun::WorkerThread, this, i, THREAD_PRIORITY_NORMAL, ((i == senderCore) || (i == receiverCore))));
            }
        }
    }

    if (noContention)
    {
        static std::function<void(PerfRun *, TestType whichTest, uint32_t, uint32_t)> NoContentionFunc[] =
        {
            &PerfRun::ReceiveKernelFunctionNoContention,
            &PerfRun::ReceiveKernelFunctionNoContention,
            &PerfRun::ReceiveKernelFunctionNoContention,
            &PerfRun::ReceiveKernelFunctionNoContention,
            &PerfRun::ReceiveKernelFunctionNoContention,
            &PerfRun::ReceiveWaitAddressFunctionNoContention,
            &PerfRun::ReceiveCVFunctionNoContention,
            &PerfRun::ReceiveCVFunctionNoContention,
        };

        receiverCore = senderCore;
        m_receiverThread = new std::thread(NoContentionFunc[whichTest], this, whichTest, receiverCore, THREAD_PRIORITY_TIME_CRITICAL);
    }
    else
    {
        typedef std::pair<std::function<void(PerfRun *, TestType whichTest, uint32_t, uint32_t)>, std::function<void(PerfRun *, TestType whichTest, uint32_t, uint32_t)>> ContentionPair;
        static ContentionPair ContentionFunc[] =
        {
            ContentionPair(&PerfRun::SendKernelFunction,&PerfRun::ReceiveKernelFunction),
            ContentionPair(&PerfRun::SendKernelFunction,&PerfRun::ReceiveKernelFunction),
            ContentionPair(&PerfRun::SendKernelFunction,&PerfRun::ReceiveKernelFunction),
            ContentionPair(&PerfRun::SendKernelFunction,&PerfRun::ReceiveKernelFunction),
            ContentionPair(&PerfRun::SendKernelFunction,&PerfRun::ReceiveKernelFunction),
            ContentionPair(&PerfRun::SendWaitAddressFunction,&PerfRun::ReceiveWaitAddressFunction),
            ContentionPair(&PerfRun::SendCVFunction,&PerfRun::ReceiveCVFunction),
            ContentionPair(&PerfRun::SendCVFunction,&PerfRun::ReceiveCVFunction),
        };

        m_senderThread = new std::thread(ContentionFunc[whichTest].first, this, whichTest, senderCore, THREAD_PRIORITY_TIME_CRITICAL);
        m_receiverThread = new std::thread(ContentionFunc[whichTest].second, this, whichTest, receiverCore, THREAD_PRIORITY_TIME_CRITICAL);
    }

    for (uint32_t i = 0; i < c_NumTestLoops; i++)
    {
        m_memoryRaceTimings[i] = 0;
        m_releaseTimings[i] = 0;
    }

    m_startTestRun = true;
    if (!noContention)
        m_senderThread->join();
    m_receiverThread->join();
    delete m_senderThread;
    delete m_receiverThread;
    m_senderThread = nullptr;
    m_receiverThread = nullptr;
    m_workerShutdown = true;
    WakeByAddressAll(&m_workerShutdown);
    for (auto thread : m_workerThreads)
    {
        thread->join();
        delete thread;
    }
    m_workerThreads.clear();

    {
        std::wstring textBuffer;
        std::wstring releaseTextBuffer;
        wchar_t tempStr[64];
        swprintf_s(tempStr, 64, L"%s %d-%d-%d,,,,,,,,,,,", ConvertTestTypeToString(whichTest).c_str(), senderCore, receiverCore, idleWorkers);
        textBuffer = tempStr;
        releaseTextBuffer = tempStr;
        for (uint32_t k = 0; k < c_NumTestLoops; k++)
        {
            uint64_t value = m_memoryRaceTimings[k];
            swprintf_s(tempStr, 64, L"%lld,", value);
            textBuffer += tempStr;

            value = m_releaseTimings[k];
            swprintf_s(tempStr, 64, L"%lld,", value);
            releaseTextBuffer += tempStr;
        }
        if (m_acquireLogfile)
            m_acquireLogfile->Log(textBuffer);
        if (m_releaseLogFile)
            m_releaseLogFile->Log(releaseTextBuffer);
    }
}

void PerfRun::WorkerThread(uint32_t core, int32_t priority, bool suspend)
{
    HANDLE curThread = GetCurrentThread();
    SetThreadAffinityMask(curThread, 1ULL << core);
    SetThreadPriorityBoost(curThread, true);
    SetThreadPriority(curThread, priority);
    while (!m_workerShutdown)
    {
        if (suspend)
        {
            bool waitValue = false;
            WaitOnAddress(&m_workerShutdown, &waitValue, 1, INFINITE);
        }
        else
        {
            SwitchToThread();
        }
    }
}

void PerfRun::SendKernelFunction(TestType whichTest, uint32_t core, int32_t priority)
{
    HANDLE curThread = GetCurrentThread();
    while (!m_startTestRun);
    SetThreadAffinityMask(curThread, 1ULL << core);
    SetThreadPriorityBoost(curThread, true);
    SetThreadPriority(curThread, priority);

    KernelFuncs[whichTest].second();
    for (uint32_t i = 0; i < c_NumTestLoops; i++)
    {
        uint64_t startTime = 0;
        uint64_t endTime = 0;
        uint32_t tscTime = 0;
        uint64_t deltaTime = 0;

        m_senderReady = true;
        while (!m_receiverReady);
        startTime = __rdtscp(&tscTime);
        endTime = __rdtscp(&tscTime);
        while ((endTime - startTime) < c_SendDelay) // need to give time for the receive thread to actually suspend waiting on the object.
        {
            endTime = __rdtscp(&tscTime);
        }
        m_calcedDelay = endTime - startTime;
        startTime = __rdtscp(&tscTime);
        KernelFuncs[whichTest].first();
        endTime = __rdtscp(&tscTime);
        deltaTime = endTime - startTime;
        m_releaseTimings[i] = deltaTime;

        while (!m_receiverDone) {}
        m_senderReady = false;
        m_senderDone = true;

        KernelFuncs[whichTest].second();
        m_senderDone = false;
    }
    KernelFuncs[whichTest].first();
}

void PerfRun::ReceiveKernelFunction(TestType whichTest, uint32_t core, int32_t priority)
{
    HANDLE curThread = GetCurrentThread();

    while (!m_startTestRun);
    SetThreadAffinityMask(curThread, 1ULL << core);
    SetThreadPriorityBoost(curThread, true);
    SetThreadPriority(curThread, priority);

    for (uint32_t loopCount = 0; loopCount < c_NumTestLoops; loopCount++)
    {
        m_receiverReady = true;
        while (!m_senderReady);
        uint64_t startTime = 0;
        uint64_t endTime = 0;
        uint64_t deltaTime = 0;
        uint32_t tscTime = 0;
        startTime = __rdtscp(&tscTime);
        KernelFuncs[whichTest].second();
        endTime = __rdtscp(&tscTime);
        m_receiverReady = false;
        m_receiverDone = true;
        while (!m_senderDone) {}
        deltaTime = endTime - startTime;
        m_memoryRaceTimings[loopCount] = (deltaTime < m_calcedDelay) ? 0 : deltaTime - m_calcedDelay;
        m_memoryRaceTimingsRaw[loopCount] = deltaTime;
        m_memoryRaceTimingsCalcedDelta[loopCount] = m_calcedDelay;
        m_receiverDone = false;
        KernelFuncs[whichTest].first();
    }
}

void PerfRun::SendWaitAddressFunction(TestType /*whichTest*/, uint32_t core, int32_t priority)
{
    HANDLE curThread = GetCurrentThread();
    while (!m_startTestRun);
    SetThreadAffinityMask(curThread, 1ULL << core);
    SetThreadPriority(curThread, priority);
    SetThreadPriorityBoost(curThread, true);

    for (uint32_t i = 0; i < c_NumTestLoops; i++)
    {
        uint64_t startTime = 0;
        uint64_t endTime = 0;
        uint32_t tscTime = 0;
        uint64_t deltaTime = 0;

        m_senderReady = true;
        while (!m_receiverReady);
        m_senderDone = false;
        startTime = __rdtscp(&tscTime);
        endTime = __rdtscp(&tscTime);
        while ((endTime - startTime) < c_SendDelay)   // need to let receiver actually suspend before doing the release
        {
            endTime = __rdtscp(&tscTime);
        }
        m_calcedDelay = endTime - startTime;
        m_waitAddress++;
        startTime = __rdtscp(&tscTime);
        WakeByAddressSingle(&m_waitAddress);
        endTime = __rdtscp(&tscTime);
        deltaTime = endTime - startTime;
        m_releaseTimings[i] = deltaTime;

        while (!m_receiverDone) {}
        m_senderReady = false;
        m_senderDone = true;
    }
}

void PerfRun::ReceiveWaitAddressFunction(TestType /*whichTest*/, uint32_t core, int32_t priority)
{
    HANDLE curThread = GetCurrentThread();

    while (!m_startTestRun);
    SetThreadAffinityMask(curThread, 1ULL << core);
    SetThreadPriority(curThread, priority);
    SetThreadPriorityBoost(curThread, true);

    for (uint32_t loopCount = 0; loopCount < c_NumTestLoops; loopCount++)
    {
        m_receiverReady = true;
        while (!m_senderReady);
        uint64_t startTime = 0;
        uint64_t endTime = 0;
        uint64_t deltaTime = 0;
        uint32_t tscTime = 0;
        uint64_t currentAddressValue = m_waitAddress;
        startTime = __rdtscp(&tscTime);
        WaitOnAddress(&m_waitAddress, &currentAddressValue, 8, INFINITE);
        endTime = __rdtscp(&tscTime);
        m_receiverReady = false;
        m_receiverDone = true;
        while (!m_senderDone) {}
        deltaTime = endTime - startTime;
        m_memoryRaceTimings[loopCount] = (deltaTime < m_calcedDelay) ? 0 : deltaTime - m_calcedDelay;
        m_memoryRaceTimingsRaw[loopCount] = deltaTime;
        m_memoryRaceTimingsCalcedDelta[loopCount] = m_calcedDelay;
        m_receiverDone = false;
    }
}

void PerfRun::SendCVFunction(TestType whichTest, uint32_t core, int32_t priority)
{
    HANDLE curThread = GetCurrentThread();
    while (!m_startTestRun);
    SetThreadAffinityMask(curThread, 1ULL << core);
    SetThreadPriority(curThread, priority);
    SetThreadPriorityBoost(curThread, true);

    KernelFuncs[whichTest].second();
    for (uint32_t i = 0; i < c_NumTestLoops; i++)
    {
        uint64_t startTime = 0;
        uint64_t endTime = 0;
        uint32_t tscTime = 0;
        uint64_t deltaTime = 0;

        m_senderReady = true;
        while (!m_receiverReady);
        KernelFuncs[whichTest].first();
        startTime = __rdtscp(&tscTime);
        endTime = __rdtscp(&tscTime);
        while ((endTime - startTime) < c_SendDelay)   // need to let receiver actually suspend before doing the release
        {
            endTime = __rdtscp(&tscTime);
        }
        m_calcedDelay = endTime - startTime;
        startTime = __rdtscp(&tscTime);
        WakeConditionVariable(&m_conditionVariable);
        endTime = __rdtscp(&tscTime);
        deltaTime = endTime - startTime;
        m_releaseTimings[i] = deltaTime;

        while (!m_receiverDone) {}
        m_senderReady = false;
        m_senderDone = true;

        KernelFuncs[whichTest].second();
        m_senderDone = false;
    }
    KernelFuncs[whichTest].first();
}

void PerfRun::ReceiveCVFunction(TestType whichTest, uint32_t core, int32_t priority)
{
    HANDLE curThread = GetCurrentThread();

    while (!m_startTestRun);
    SetThreadAffinityMask(curThread, 1ULL << core);
    SetThreadPriority(curThread, priority);
    SetThreadPriorityBoost(curThread, true);

    for (uint32_t loopCount = 0; loopCount < c_NumTestLoops; loopCount++)
    {
        m_receiverReady = true;
        while (!m_senderReady);
        uint64_t startTime = 0;
        uint64_t endTime = 0;
        uint64_t deltaTime = 0;
        uint32_t tscTime = 0;
        startTime = __rdtscp(&tscTime);
        KernelFuncs[whichTest].second();
        if (whichTest == PERFORM_CONDITION_CS)
            SleepConditionVariableCS(&m_conditionVariable, &m_critSection, INFINITE);
        else
            SleepConditionVariableSRW(&m_conditionVariable, &m_srw, INFINITE, !CONDITION_VARIABLE_LOCKMODE_SHARED);
        endTime = __rdtscp(&tscTime);
        m_receiverReady = false;
        m_receiverDone = true;
        while (!m_senderDone) {}
        deltaTime = endTime - startTime;
        m_memoryRaceTimings[loopCount] = (deltaTime < m_calcedDelay) ? 0 : deltaTime - m_calcedDelay;
        m_memoryRaceTimingsRaw[loopCount] = deltaTime;
        m_memoryRaceTimingsCalcedDelta[loopCount] = m_calcedDelay;
        m_receiverDone = false;
        KernelFuncs[whichTest].first();
    }
}

//////////////////////////////////////////////////////////////////////////
//
//
//
// The no contention functions
//
//
//////////////////////////////////////////////////////////////////////////

void PerfRun::ReceiveKernelFunctionNoContention(TestType whichTest, uint32_t core, int32_t priority)
{
    HANDLE curThread = GetCurrentThread();

    while (!m_startTestRun);
    SetThreadAffinityMask(curThread, 1ULL << core);
    SetThreadPriorityBoost(curThread, true);
    SetThreadPriority(curThread, priority);

    for (uint32_t loopCount = 0; loopCount < c_NumTestLoops; loopCount++)
    {
        uint64_t startTime = 0;
        uint64_t endTime = 0;
        uint64_t deltaTime = 0;
        uint32_t tscTime = 0;
        startTime = __rdtscp(&tscTime);
        KernelFuncs[whichTest].second();
        endTime = __rdtscp(&tscTime);
        deltaTime = endTime - startTime;
        m_memoryRaceTimings[loopCount] = deltaTime;
        startTime = __rdtscp(&tscTime);
        KernelFuncs[whichTest].first();
        endTime = __rdtscp(&tscTime);
        deltaTime = endTime - startTime;
        m_releaseTimings[loopCount] = deltaTime;
    }
}

void PerfRun::ReceiveWaitAddressFunctionNoContention(TestType /*whichTest*/, uint32_t core, int32_t priority)
{
    HANDLE curThread = GetCurrentThread();

    while (!m_startTestRun);
    SetThreadAffinityMask(curThread, 1ULL << core);
    SetThreadPriority(curThread, priority);
    SetThreadPriorityBoost(curThread, true);

    for (uint32_t loopCount = 0; loopCount < c_NumTestLoops; loopCount++)
    {
        uint64_t startTime = 0;
        uint64_t endTime = 0;
        uint64_t deltaTime = 0;
        uint32_t tscTime = 0;
        uint64_t currentAddressValue = m_waitAddress + 1;
        startTime = __rdtscp(&tscTime);
        WaitOnAddress(&m_waitAddress, &currentAddressValue, 8, INFINITE);
        endTime = __rdtscp(&tscTime);
        deltaTime = endTime - startTime;
        m_memoryRaceTimings[loopCount] = deltaTime;
        startTime = __rdtscp(&tscTime);
        WakeByAddressSingle(&m_waitAddress);
        endTime = __rdtscp(&tscTime);
        deltaTime = endTime - startTime;
        m_releaseTimings[loopCount] = deltaTime;
    }
}

void PerfRun::ReceiveCVFunctionNoContention(TestType /*whichTest*/, uint32_t core, int32_t priority)
{
    HANDLE curThread = GetCurrentThread();

    while (!m_startTestRun);
    SetThreadAffinityMask(curThread, 1ULL << core);
    SetThreadPriority(curThread, priority);
    SetThreadPriorityBoost(curThread, true);

    // These tests don't really make sense. Condition variables need another thread to make the condition true
    // If the condition was previously true then the thread would never sleep on the condition variable.
    // This is due to the locking wrapper used by the condition variable which should be locked before checking the state of the condition
    for (uint32_t loopCount = 0; loopCount < c_NumTestLoops; loopCount++)
    {
        m_releaseTimings[loopCount] = 0;
    }
}
