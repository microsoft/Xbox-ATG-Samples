//--------------------------------------------------------------------------------------
// OverlappedSample.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "OverlappedSample.h"
#include <functional>

OverlappedSample::OverlappedSample() :
    m_currentType(OverlappedType::NULL_TYPE)
    , m_workThread(nullptr)
    , m_typeDone(true)
    , m_numRequestsInFlight(0)
    , m_dataFileSizes {
        DataFileSizePair(50 * 1024 * 1024, 2),
        DataFileSizePair(10 * 1024 * 1024, 4),
        DataFileSizePair(5 * 1024 * 1024, 6),
        DataFileSizePair(1 * 1024 * 1024, 20),
        DataFileSizePair(768 * 1024, 50),
        DataFileSizePair(512 * 1024, 100),
        DataFileSizePair(256 * 1024, 200),
        DataFileSizePair(128 * 1024, 300),
        DataFileSizePair(64 * 1024, 500),
        DataFileSizePair(32 * 1024, 500)
    }
    , m_readSizes{
        ReadSizePair(5 * 1024 * 1024, 2),
        ReadSizePair(1 * 1024 * 1024, 4),
        ReadSizePair(768 * 1024, 6),
        ReadSizePair(512 * 1024, 20),
        ReadSizePair(256 * 1024, 50),
        ReadSizePair(128 * 1024, 100),
        ReadSizePair(64 * 1024, 200),
        ReadSizePair(32 * 1024, 300),
        ReadSizePair(16 * 1024, 500),
        ReadSizePair(4 * 1024, 500)
    }
    , m_totalReadSize(0)
    , m_totalNumReads(0)
{
    // all non-buffered (which overlapped requires) reads require the size of the read to be a multiple of 4k
    for (uint32_t i = 0; i < c_numReads; i++)
    {
        m_readSizes[i].first &= ~4095;
        m_totalNumReads += m_readSizes[i].second;
        m_totalReadSize += m_readSizes[i].first * m_readSizes[i].second;
    }

    // all non-buffered (which overlapped requires) reads require the location to be a multiple of 4k
    // set the size of the files to be a multiple of 4k, this simplifies some of the math in the sample
    for (uint32_t i = 0; i < c_numDataFileSizes; i++)
    {
        m_dataFileSizes[i].first &= ~4095;
    }
    memset(m_pendingOverlap, 0xff, sizeof(m_pendingOverlap));
    for (uint32_t i = 0; i < c_maxRequestsInFlight; i++)
    {
        m_events[i] = INVALID_HANDLE_VALUE;
    }
}

OverlappedSample::~OverlappedSample()
{
    ShutdownCurrentType();
    CloseFiles();
    for (uint32_t i = 0; i < c_maxRequestsInFlight; i++)
    {
        if (m_events[i] != INVALID_HANDLE_VALUE)
        {
            CloseHandle(m_events[i]);
        }
    }
    VirtualFree(m_readBuffer, 0, MEM_DECOMMIT);
}

//************************************
// Method:    CloseFiles
// Notes:     Cleanup all of the open files, used when shutting down the sample
//************************************
void OverlappedSample::CloseFiles()
{
    auto endOuterIter = m_openFiles.end();
    for (auto outerIter = m_openFiles.begin(); outerIter != endOuterIter; ++outerIter)
    {
        auto endInnerIter = outerIter->end();
        for (auto innerIter = outerIter->begin(); innerIter != endInnerIter; ++innerIter)
        {
            CloseHandle(*innerIter);
        }
    }
}

//************************************
// Method:    Init
// Returns:   bool - success or failure
// Notes:     Allocates the main memory buffer and allocates the cache of event objects
//************************************
bool OverlappedSample::Init()
{
    m_readBuffer = VirtualAlloc(nullptr, m_dataFileSizes[0].first, MEM_COMMIT, PAGE_READWRITE);
    if (!m_readBuffer)
        return false;
    for (uint32_t i = 0; i < c_maxRequestsInFlight; i++)
    {
        // use manual reset events for overlapped requests, this guarantees against missing finished notifications.
        m_events[i] = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        if (m_events[i] == INVALID_HANDLE_VALUE)
            return false;
        // since there are only c_maxRequestsInFlight at a time we can bind event objects to overlapped objects
        m_pendingOverlap[i].m_overlappedObject.hEvent = m_events[i];
        m_pendingOverlap[i].m_inUse = false;
    }
    return true;
}

//************************************
// Method:    FindOpenOverlappedBlock
// Returns:   uint32_t - an unused entry in the pending request list
//************************************
uint32_t OverlappedSample::FindOpenOverlappedBlock()
{
    for (uint32_t i = 0; i < c_maxRequestsInFlight; i++)
    {
        if (!m_pendingOverlap[i].m_inUse)
            return i;
    }
    return UINT32_MAX;
}

//************************************
// Method:    PickReadFile
// Returns:   ReadFileIndex - pair used as index into 2d array m_openFiles
// Parameter: uint32_t readSize - How many bytes need to be available in the file
//************************************
OverlappedSample::ReadFileIndex OverlappedSample::PickReadFile(uint32_t readSize)
{
    ReadFileIndex toret(UINT32_MAX, UINT32_MAX);

    assert(m_openFiles.size());
    for (toret.first = 0; toret.first < c_numDataFileSizes; toret.first++)
    {
        if (m_dataFileSizes[toret.first].first < readSize)
        {
            toret.first--;
            break;
        }
    }
    if (toret.first != 0)
        toret.first = std::rand() % toret.first;
    assert(m_openFiles[toret.first].size());
    toret.second = std::rand() % m_openFiles[toret.first].size();
    return toret;
}

//************************************
// Method:    PickReadLocation
// Returns:   uint32_t - the base location in the file to start the read
// Parameter: ReadFileIndex readFile - which file to read from
// Parameter: uint32_t readSize - size of the read request
//************************************
uint32_t OverlappedSample::PickReadLocation(ReadFileIndex readFile, uint32_t readSize)
{
    assert(readFile.first != UINT32_MAX);
    assert(readFile.second != UINT32_MAX);
    assert(m_dataFileSizes[readFile.first].first >= readSize);

    if (m_dataFileSizes[readFile.first].first == readSize)
        return 0;

    return (std::rand() % (m_dataFileSizes[readFile.first].first - readSize)) & ~4095;   // require 4k alignment for file location for overlapped
}

//************************************
// Method:    InitializeBaseOverlappedBlock
// Returns:   uint32_t - which index into m_pendingOverlap was setup
// Notes:     setup a pending overlap block with common data for each pattern
//            will also pick the file and read size for this request
//            all patterns use an OVERLAPPED structure which contains the read location
//************************************
uint32_t OverlappedSample::InitializeBaseOverlappedBlock(uint32_t readSize)
{
    // all non-buffered (which overlapped requires) reads require the size of the read to be a multiple of 4k
    assert((readSize % 4096) == 0);
    uint32_t overlapIndex = FindOpenOverlappedBlock();
    assert(overlapIndex != UINT32_MAX);
    ReadFileIndex readFile = PickReadFile(readSize);
    uint32_t readLocation = PickReadLocation(readFile, readSize);

    // all non-buffered (which overlapped requires) reads require the location to be a multiple of 4k
    assert((readLocation % 4096) == 0);
    m_pendingOverlap[overlapIndex].m_inUse = true;
    m_pendingOverlap[overlapIndex].m_readFile = readFile;
    m_pendingOverlap[overlapIndex].m_requestedReadSize = readSize;

    // By default all fields of the OVERLAPPED structure should be zero except for the Offset for the read in the file
    // hEvent has different meanings based on the pattern being used and is set by the caller of this function
    m_pendingOverlap[overlapIndex].m_overlappedObject.Internal = 0;
    m_pendingOverlap[overlapIndex].m_overlappedObject.InternalHigh = 0;
    m_pendingOverlap[overlapIndex].m_overlappedObject.Offset = readLocation;
    m_pendingOverlap[overlapIndex].m_overlappedObject.OffsetHigh = 0;

    return overlapIndex;
}

//************************************
// Method:    CreateAndOpenFilesThreadProc
// Notes:   create all of the data files used for the sample, will reopen the files for overlapped operation
//************************************
void OverlappedSample::CreateAndOpenFilesThreadProc()
{
    // Make sure any cruft for open files are closed before recreating and reopening the files
    CloseFiles();

    uint32_t *baseDataAddress32 = reinterpret_cast<uint32_t *> (m_readBuffer);
    // just assume the first entry is the largest.
    for (uint32_t i = 0; i < m_dataFileSizes[0].first / sizeof(uint32_t); i++)
    {
        baseDataAddress32[i] = i;
    }

    uint64_t totalFileSize = 0;

    // This sample creates all of the data files each time it's run.
    // This allows easy changes to the m_dataFileSizes and m_numDataFilesPerSize for testing purposes
    m_openFiles.resize(c_numDataFileSizes);
    for (uint32_t fileIndexSize = 0; fileIndexSize < c_numDataFileSizes; fileIndexSize++)
    {
        m_openFiles[fileIndexSize].reserve(m_dataFileSizes[fileIndexSize].second);
        for (uint32_t fileIndex = 0; fileIndex < m_dataFileSizes[fileIndexSize].second; fileIndex++)
        {
            wchar_t fileName[128];
            totalFileSize += m_dataFileSizes[fileIndexSize].first;
            swprintf_s(fileName, 128, L"d:\\dataFile_%d_%d.dat", fileIndexSize, fileIndex);
            HANDLE curFile;
            CREATEFILE2_EXTENDED_PARAMETERS params;
            memset(&params, 0, sizeof(params));
            params.dwSize = sizeof(params);
            params.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
            // It's recommended to always use FILE_FLAG_NO_BUFFERING if possible
            // This avoids the pollution of the small file cache used on Xbox and let's it store MFT data for efficient file opens
            params.dwFileFlags = FILE_FLAG_NO_BUFFERING;
            curFile = CreateFile2(fileName, GENERIC_ALL, 0, CREATE_ALWAYS, &params);
            if (curFile == INVALID_HANDLE_VALUE)
            {
                m_typeDone = true;
                return;
            }
            DWORD bytesWritten;
            if (!WriteFile(curFile, m_readBuffer, m_dataFileSizes[fileIndexSize].first, &bytesWritten, nullptr))
            {
                m_typeDone = true;      // This will exit the thread and cause the system to try again next pass
                return;
            }
            CloseHandle(curFile);

            // reopen the file for overlapped operation with FILE_FLAG_OVERLAPPED
            params.dwFileFlags = FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED;
            curFile = CreateFile2(fileName, GENERIC_ALL, 0, OPEN_EXISTING, &params);
            m_openFiles[fileIndexSize].push_back(curFile);
        }
    }

    m_typeDone = true;
}

//************************************
// Method:    ShutdownCurrentType
// Notes:   wait for the worker thread to shutdown and then clean it up
//************************************
void OverlappedSample::ShutdownCurrentType()
{
    if (m_workThread)
    {
        m_workThread->join();
        delete m_workThread;
        m_workThread = nullptr;
    }
}

//************************************
// Method:    StartIndividualType
// Returns:   bool - success or failure
// Notes:     based on m_currentType create a thread with the corresponding thread procedure
//************************************
bool OverlappedSample::StartIndividualType()
{
    m_typeDone = false;
    std::function<void(OverlappedSample *)> threadFunc;

    switch (m_currentType)
    {
    case OverlappedType::CREATING_FILES:
        threadFunc = &OverlappedSample::CreateAndOpenFilesThreadProc;
        break;
    case OverlappedType::EVENT:
        threadFunc = &OverlappedSample::EventTypeThreadProc;
        break;
    case OverlappedType::QUERY:
        threadFunc = &OverlappedSample::QueryTypeThreadProc;
        break;
    case OverlappedType::ALERTABLE:
        threadFunc = &OverlappedSample::AlertableTypeThreadProc;
        break;
    default:
        return false;
    }

    m_workThread = new std::thread(threadFunc, this);
    return true;
}

//************************************
// Method:    Update
// Returns:   bool - success or failure on the update, really only used for initialization
// Notes:     called every frame by the main sample, will rotate through the patterns as they finish
//************************************
bool OverlappedSample::Update()
{
    if (m_events[0] == INVALID_HANDLE_VALUE)
    {
        if (!Init())
            return false;
    }

    if (m_typeDone)
    {
        ShutdownCurrentType();
    }

    if (m_workThread == nullptr)
    {
        switch (m_currentType)
        {
        case OverlappedType::NULL_TYPE:
            m_currentType = OverlappedType::CREATING_FILES;
            break;
        case OverlappedType::CREATING_FILES:
            m_currentType = OverlappedType::EVENT;
            break;
        case OverlappedType::EVENT:
            m_currentType = OverlappedType::QUERY;
            break;
        case OverlappedType::QUERY:
            m_currentType = OverlappedType::ALERTABLE;
            break;
        case OverlappedType::ALERTABLE:
            m_currentType = OverlappedType::EVENT;
            break;
        }
        return StartIndividualType();
    }
    return true;
}

//************************************
// Method:    GetCurrentTypeString
// Returns:   std::wstring - matching string for the current pattern running, used for display on the screen
//************************************
std::wstring OverlappedSample::GetCurrentTypeString() const
{
    switch (m_currentType)
    {
    case OverlappedType::NULL_TYPE:
        return L"Not Started";
    case OverlappedType::CREATING_FILES:
        return L"Creating Files";
    case OverlappedType::EVENT:
        return L"Running Event Based";
    case OverlappedType::QUERY:
        return L"Running Query Based";
    case OverlappedType::ALERTABLE:
        return L"Running Alertable Based";
    }
    assert(false);
    return L"Unknown Type";
}
