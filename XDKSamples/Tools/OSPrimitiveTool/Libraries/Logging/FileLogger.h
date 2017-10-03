//--------------------------------------------------------------------------------------
// FileLogger.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <cstdint>
#include <fstream>
#include "BaseLogger.h"

namespace ATG
{
    namespace DebugLog
    {
        class FileLogger : public BaseLogger
        {
        protected:
            std::basic_fstream<wchar_t> *m_streamFile;
            std::wstring m_baseFileName;
            std::wstring m_fullFileName;
            bool m_append;
            bool m_includeCompilerVersion;

            virtual void ShutdownLogger();
            virtual void StartupLogger();

            virtual void DumpQueue(uint32_t queueIndex);
            void OpenLogFile();

        public:
            FileLogger(const FileLogger& rhs) = delete;
            FileLogger &operator= (const FileLogger& rhs) = delete;
            FileLogger();
            FileLogger(const std::wstring& location, bool append, bool includeCompilerVersion = false);
            virtual ~FileLogger();
            bool ResetLogFile(const std::wstring& location, bool append, bool includeCompilerVersion = false);
        };
    }
}
