//--------------------------------------------------------------------------------------
// BaseLogger.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "BaseLogger.h"

using namespace ATG;
using namespace DebugLog;

BaseLogger::BaseLogger()
{
    m_outputQueue[0].reserve(1000);
    m_outputQueue[1].reserve(1000);
    m_currentOutputQueue = 0;
    m_outputThread = nullptr;
}

BaseLogger::~BaseLogger()
{
    ShutdownLogger();
}

void BaseLogger::StartupLogger()
{
    m_killFlag.store(0);
    m_outputThread = new std::thread(&BaseLogger::SaveLogThread, this);
}

void BaseLogger::ShutdownLogger()
{
    if (m_outputThread)
    {
        m_killFlag.store(1);
        m_outputThread->join();
        DumpQueue(m_currentOutputQueue);
        DumpQueue(!m_currentOutputQueue);
        m_outputThread = nullptr;
    }
}

void BaseLogger::Log(const std::wstring& logLine)
{
    std::lock_guard<std::mutex> lg(m_queueCrit);
    m_outputQueue[m_currentOutputQueue].emplace(m_outputQueue[m_currentOutputQueue].end(), logLine);
}

void BaseLogger::SaveLogThread(void)
{
    while (m_killFlag.load() == 0)
    {
        uint32_t outputQueue = m_currentOutputQueue;
        {
            std::lock_guard<std::mutex> lg(m_queueCrit);
            m_currentOutputQueue = !m_currentOutputQueue;
        }
        DumpQueue(outputQueue);
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}
