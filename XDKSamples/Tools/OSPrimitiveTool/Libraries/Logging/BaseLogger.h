//--------------------------------------------------------------------------------------
// BaseLogger.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>

namespace ATG
{
    namespace DebugLog
    {
        class BaseLogger
        {
        protected:
            std::thread *m_outputThread;
            std::mutex m_queueCrit;
            std::vector<std::wstring> m_outputQueue[2];
            uint32_t m_currentOutputQueue;
            std::atomic<uint32_t> m_killFlag;

            void SaveLogThread(void);
            virtual void DumpQueue(uint32_t queueIndex) = 0;
            virtual void ShutdownLogger();
            virtual void StartupLogger();

        public:
            BaseLogger(const BaseLogger& rhs) = delete;
            BaseLogger &operator= (const BaseLogger& rhs) = delete;
            BaseLogger();
            virtual ~BaseLogger();

            void Log(const std::wstring& logLine);
        };
    }
}
