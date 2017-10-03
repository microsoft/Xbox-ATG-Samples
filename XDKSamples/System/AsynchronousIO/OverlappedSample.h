//--------------------------------------------------------------------------------------
// OverlappedSample.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once
#include <cstdint>
#include <thread>
#include <atomic>
#include <vector>
#include <array>
#include <OSLockable.h>

class OverlappedSample
{
public:
    static const uint32_t c_maxRequestsInFlight = 12;
    static const uint32_t c_numDataFileSizes = 10;
    static const uint32_t c_numReads = 10;
    enum class OverlappedType
    {
        NULL_TYPE,
        CREATING_FILES,     // create and open each of the data files
        EVENT,              // use the event pattern for overlapped I/O, each request has an associated event object that can be used with WaitForSingleObject
        QUERY,              // use the query pattern for overlapped I/O, call GetOverlappedResult to check on a request until it is complete
        ALERTABLE,          // use the alert pattern for overlapped I/O, a completion callback is used and called by the OS when the thread is in an alertable state
    };

private:

    //////////////////////////////////////////////////////////////////////////
    //
    // Shared data and interfaces between each overlapped read pattern
    //
    //////////////////////////////////////////////////////////////////////////
    typedef std::pair<uint32_t, uint32_t> ReadFileIndex;    // first is which size file, second is which file within size
    typedef std::pair<uint32_t, uint32_t> DataFileSizePair; // first is size of the file, second is how many files of that size
    typedef std::pair<uint32_t, uint32_t> ReadSizePair;     // first is how many bytes to read, second is how many times to perform this read
    struct PendingOverlap
    {
        bool m_inUse;
        ReadFileIndex m_readFile;
        uint32_t m_requestedReadSize;
        OVERLAPPED m_overlappedObject;              // all OVERLAPPED objects must remain valid during the entire time the request in flight, they can be reused though
    };

    void *m_readBuffer;                             // one buffer is shared for all read requests since this sample doesn't actually care about the loaded data
    uint32_t m_numRequestsInFlight;                 // how many pending requests are currently active
    std::atomic<bool> m_typeDone;                   // set by the worker thread when it's done loading data so the sample can switch to the next one
    std::thread *m_workThread;                      // thread performing the read requests and checking for completed requests
    OverlappedType m_currentType;                   // which type of pattern is currently running

    HANDLE m_events[c_maxRequestsInFlight];                     // preallocated events to use for event and query based patterns
    PendingOverlap m_pendingOverlap[c_maxRequestsInFlight];     // control data for each pending request, reused for new pending requests

    std::array<DataFileSizePair, c_numDataFileSizes> m_dataFileSizes;   // first is size of the file, second is how many files of that size
    std::array<ReadSizePair, c_numReads> m_readSizes;                   // first is how many bytes to read, second is how many times to perform this read

    std::vector<std::vector<HANDLE>> m_openFiles;       // 2d array for all of the open files, based on [file size][index at that size]

    uint32_t m_totalReadSize;                       // sum of the size of all reads, used for internal validation
    uint32_t m_totalNumReads;                       // sum of the total number of reads, used for internal validation

    bool Init();                                    // allocate the events and memory buffers
    void CloseFiles();                              // cleanup the open files
    bool StartIndividualType();                     // create the proper thread based on which type of overlapped pattern is being used
    void ShutdownCurrentType();                     // cleanup the executing thread
    void CreateAndOpenFilesThreadProc();            // using the data in m_dataFileSizes and m_numDataFilesPerSize create the data files then open them for testing

    uint32_t InitializeBaseOverlappedBlock(uint32_t readSize);              // find and setup an unused PendingOverlap block with data shared by all patterns
    uint32_t FindOpenOverlappedBlock();                                     // returns an unused member of m_pendingOverlap
    ReadFileIndex PickReadFile(uint32_t readSize);                          // find a valid file to use for a specific read size, is large enough
    uint32_t PickReadLocation(ReadFileIndex readFile, uint32_t readSize);   // find a valid location in the requested file that can be used for the requested read size

    //////////////////////////////////////////////////////////////////////////
    //
    // Data and Interfaces unique to using a wait pattern on overlapped I/O
    //
    //////////////////////////////////////////////////////////////////////////
    void EventTypeThreadProc();
    void WaitForEventOverlappedToFinish();

    //////////////////////////////////////////////////////////////////////////
    //
    // Data and Interfaces unique to using a query pattern on overlapped I/O
    //
    //////////////////////////////////////////////////////////////////////////
    void QueryTypeThreadProc();
    void QueryForOverlappedFinished();

    //////////////////////////////////////////////////////////////////////////
    //
    // Data and Interfaces unique to using an alertable/callback pattern on overlapped I/O
    //
    //////////////////////////////////////////////////////////////////////////
    friend void __stdcall FileIOCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);
    void AlertableTypeThreadProc();
    void AlertableCompletionRoutine(uint32_t errorCode, uint32_t numberOfBytesTransfered, OVERLAPPED *overlapped);

public:
    OverlappedSample();
    ~OverlappedSample();

    bool Update();
    std::wstring GetCurrentTypeString() const;
};
