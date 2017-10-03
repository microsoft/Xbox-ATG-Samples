//--------------------------------------------------------------------------------------
// AlertableOverlapped.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "OverlappedSample.h"

//************************************
// Method:    FileIOCompletionRoutine
// Parameter: DWORD dwErrorCode
// Parameter: DWORD dwNumberOfBytesTransfered
// Parameter: LPOVERLAPPED lpOverlapped
// Notes:     function passed to the OS as the callback for the alertable overlapped I/O pattern
//************************************
void __stdcall FileIOCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
{
    // the hEvent field is not used with the alertable pattern, it can used by the title for any data
    // in this case we use it to hold a pointer to the class instance that issued the request
    // another useful piece of information might be the index of the request instead, this avoid having to compare against all pending requests
    OverlappedSample *sample = reinterpret_cast<OverlappedSample*>(lpOverlapped->hEvent);
    sample->AlertableCompletionRoutine(dwErrorCode, dwNumberOfBytesTransfered, lpOverlapped);
}

//************************************
// Method:    AlertableCompletionRoutine
// Parameter: uint32_t errorCode
// Parameter: uint32_t numberOfBytesTransfered
// Parameter: OVERLAPPED * overlapped
// Notes:     FileIOComplettionRoutine is passed to the OS as a callback, that function then calls into this class member function to resolve the read
//************************************
void OverlappedSample::AlertableCompletionRoutine(uint32_t errorCode, uint32_t numberOfBytesTransfered, OVERLAPPED *overlapped)
{
    // these parameters are only used in asserts and will give warning for other build configurations
    // just always mark them as unreferenced to avoid the warning and thus error in the build
    UNREFERENCED_PARAMETER(errorCode);
    UNREFERENCED_PARAMETER(numberOfBytesTransfered);

    for (uint32_t i = 0; i < c_maxRequestsInFlight; i++)
    {
        // since we are using the hEvent field to point to the instance of OverlappedSample we have to query against all pending requests to find the one that finished
        if (&(m_pendingOverlap[i].m_overlappedObject) == overlapped)
        {
            assert(errorCode == ERROR_SUCCESS);
            assert(m_pendingOverlap[i].m_requestedReadSize == numberOfBytesTransfered);
            m_pendingOverlap[i].m_inUse = false;
            m_numRequestsInFlight--;
            break;
        }
    }
}

//************************************
// Method:    AlertableTypeThreadProc
// Notes:     thread procedure used by the alertable overlapped I/O read pattern
//************************************
void OverlappedSample::AlertableTypeThreadProc()
{
    uint32_t totalReads = 0;
    uint32_t totalReadSize = 0;
    for (uint32_t readIndex = 0; readIndex < c_numReads; readIndex++)
    {
        for (uint32_t currentRead = 0; currentRead < m_readSizes[readIndex].second; )
        {
            if (m_numRequestsInFlight < c_maxRequestsInFlight)
            {
                totalReads++;
                totalReadSize += m_readSizes[readIndex].first;
                currentRead++;

                uint32_t overlapIndex = InitializeBaseOverlappedBlock(m_readSizes[readIndex].first);

                // the hEvent in the overlapped object is not used with completion routines, it can be used for whatever data the title desires
                // in this samples case it's used to pass the instance of OverlappedSample that issued the request
                m_pendingOverlap[overlapIndex].m_overlappedObject.hEvent = this;
                ReadFileIndex readFile = m_pendingOverlap[overlapIndex].m_readFile;

                // ReadFileEx is used for only the alertable pattern, it will always call the completion routine when the requests completes, is canceled, or fails
                // it is possible for the completion routine to be called before this function returns so make sure all data is setup correctly before calling
                // the same read buffer is used for all pending requests for this sample because we don't actually care about the data read from the file
                assert(ReadFileEx(m_openFiles[readFile.first][readFile.second], m_readBuffer, m_pendingOverlap[overlapIndex].m_requestedReadSize, &(m_pendingOverlap[overlapIndex].m_overlappedObject), FileIOCompletionRoutine));
                m_numRequestsInFlight++;
            }
            else    // for the alertable pattern at least one thread in the title must be an alertable state for the completion routine to be called
            {       // this can be done using SleepEx, WaitForSingleObjectEx, or WaitForMultipleObjectsEx with the last parameter set to TRUE
                    // if an overlapped request with an attached completion routine completes during the time the thread is suspended it will resume
                    // the completion routine will be executed on the thread in question and then the thread will continue to sleep or wait
                SleepEx(1, TRUE);
            }
        }
    }
    while (m_numRequestsInFlight)   // all of the requests have to created, wait for the rest of the unfinished to complete
    {
        SleepEx(1, TRUE);
    }

    assert(totalReads == m_totalNumReads);
    assert(totalReadSize == m_totalReadSize);
    m_typeDone = true;
}
