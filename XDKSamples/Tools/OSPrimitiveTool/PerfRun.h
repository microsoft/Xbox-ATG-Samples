//--------------------------------------------------------------------------------------
// PerfRun.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <xdk.h>
#include <windows.h>
#include <cstdint>
#include <thread>
#include <atomic>
#include <vector>

namespace ATG
{
	namespace DebugLog
	{
		class FileLogger;
	}
}

class PerfRun
{
public:
	enum TestType
	{
		PERFORM_SEMAPHORE,
		PERFORM_EVENT,
		PERFORM_MUTEX,
		PERFORM_SRW,
		PERFORM_CRITICAL_SECTION,
		PERFORM_WAIT_ADDRESS,
		PERFORM_CONDITION_CS,
		PERFORM_CONDITION_SRW,
		NumTestTypeDefined,

		FirstTestTypeExecuted = PERFORM_SEMAPHORE,
		LastTestTypeExecuted = PERFORM_CONDITION_SRW,

		FirstTestTypeDefined = PERFORM_SEMAPHORE,
		LastTestTypeDefined = PERFORM_CONDITION_SRW,
	};
	static std::wstring ConvertTestTypeToString(TestType testType)
	{
		switch (testType)
		{
		case PERFORM_SEMAPHORE:
			return L"Semaphore";
		case PERFORM_EVENT:
			return L"Event";
		case PERFORM_SRW:
			return L"SRW";
		case PERFORM_MUTEX:
			return L"Mutex";
		case PERFORM_CRITICAL_SECTION:
			return L"Critical Section";
		case PERFORM_WAIT_ADDRESS:
			return L"WaitOnAddress";
		case PERFORM_CONDITION_CS:
			return L"Condition critical section";
		case PERFORM_CONDITION_SRW:
			return L"Condition SRW";
		}
		return L"Unknown";
	}
	static const uint32_t c_NumTestLoops = 1000;
	static const uint32_t c_SendDelay = 1000000;
	static const uint32_t s_MaxCore = 7;

private:
	std::thread *m_senderThread;
	std::thread *m_receiverThread;
	std::vector<std::thread *> m_workerThreads;
	std::atomic<bool> m_startTestRun;
	std::atomic<bool> m_senderReady;
	std::atomic<bool> m_receiverReady;
	std::atomic<bool> m_senderDone;
	std::atomic<bool> m_receiverDone;
	bool m_workerShutdown;
	std::atomic<uint64_t> m_calcedDelay;
	uint64_t m_waitAddress;

	uint64_t m_memoryRaceTimings[c_NumTestLoops];
	uint64_t m_memoryRaceTimingsRaw[c_NumTestLoops];
	uint64_t m_memoryRaceTimingsCalcedDelta[c_NumTestLoops];
	uint64_t m_releaseTimings[c_NumTestLoops];

	ATG::DebugLog::FileLogger *m_acquireLogfile;
	ATG::DebugLog::FileLogger *m_releaseLogFile;

	HANDLE m_semaphore;
	HANDLE m_event;
	HANDLE m_mutex;
	SRWLOCK m_srw;
	CRITICAL_SECTION m_critSection;
	CONDITION_VARIABLE m_conditionVariable;

	typedef std::function<void WINAPI()> KernelFuncObject;
	typedef std::pair<KernelFuncObject, KernelFuncObject> KernelFuncPair;
	KernelFuncPair KernelFuncs[NumTestTypeDefined];

	void WorkerThread(uint32_t core, int32_t priority, bool suspend);
	void SendKernelFunction(TestType whichTest, uint32_t core, int32_t priority);
	void ReceiveKernelFunction(TestType whichTest, uint32_t core, int32_t priority);

	void SendWaitAddressFunction(TestType whichTest, uint32_t core, int32_t priority);
	void ReceiveWaitAddressFunction(TestType whichTest, uint32_t core, int32_t priority);

	void SendCVFunction(TestType whichTest, uint32_t core, int32_t priority);
	void ReceiveCVFunction(TestType whichTest, uint32_t core, int32_t priority);

	void ReceiveKernelFunctionNoContention(TestType whichTest, uint32_t core, int32_t priority);

	void ReceiveWaitAddressFunctionNoContention(TestType whichTest, uint32_t core, int32_t priority);
	void ReceiveCVFunctionNoContention(TestType whichTest, uint32_t core, int32_t priority);

	void OpenLogFiles(bool noContention);

public:
	PerfRun();
	virtual ~PerfRun();

	virtual void RunTests(uint32_t senderCore, uint32_t receiverCore, TestType whichTest, bool noContention, uint32_t idleWorkers);
};
