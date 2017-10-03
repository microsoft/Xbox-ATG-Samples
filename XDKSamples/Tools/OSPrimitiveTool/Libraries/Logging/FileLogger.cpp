//--------------------------------------------------------------------------------------
// FileLogger.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "FileLogger.h"

#if defined(_XBOX_ONE)
#include <xdk.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <direct.h>
#endif

using namespace ATG;
using namespace DebugLog;

FileLogger::FileLogger() : BaseLogger()
{
    m_baseFileName = L"";
    m_append = false;
    m_streamFile = nullptr;
}
FileLogger::FileLogger(const std::wstring& location, bool append, bool includeCompilerVersion) : BaseLogger()
{
    m_baseFileName = location;
    m_append = append;
    m_includeCompilerVersion = includeCompilerVersion;
    StartupLogger();
}

FileLogger::~FileLogger()
{
    ShutdownLogger();
}

void FileLogger::OpenLogFile()
{
    std::wstring fullFileName;
#ifdef _XBOX_ONE
    fullFileName = L"d:\\";
#else
    _mkdir("Logs");
    fullFileName = L"Logs\\";
#endif

    fullFileName += m_baseFileName;
    if (m_includeCompilerVersion)
    {
        wchar_t temp[32];
        _itow_s<32>(_MSC_FULL_VER, temp, 10);
        fullFileName += L"_";
        fullFileName += temp;
    }
    fullFileName += L".txt";
    m_fullFileName = fullFileName;
    std::ios_base::openmode fred = std::ios_base::out;
    if (m_append)
        fred += std::ios_base::app;
    m_streamFile = new std::basic_fstream<wchar_t>(m_fullFileName, fred);
}

void FileLogger::ShutdownLogger()
{
    BaseLogger::ShutdownLogger();
    delete m_streamFile;
    m_streamFile = nullptr;
}

void FileLogger::StartupLogger()
{
    OpenLogFile();
    BaseLogger::StartupLogger();
}

bool FileLogger::ResetLogFile(const std::wstring& location, bool append, bool includeCompilerVersion)
{
    ShutdownLogger();
    m_baseFileName = location;
    m_append = append;
    m_includeCompilerVersion = includeCompilerVersion;
    StartupLogger();
    return true;
}

void FileLogger::DumpQueue(uint32_t queueIndex)
{
    std::vector<std::wstring>::iterator iter, endIter;

    endIter = m_outputQueue[queueIndex].end();
    for (iter = m_outputQueue[queueIndex].begin(); iter != endIter; ++iter)
    {
        *m_streamFile << iter->c_str();
        *m_streamFile << std::endl;
    }
    m_outputQueue[queueIndex].clear();
}
