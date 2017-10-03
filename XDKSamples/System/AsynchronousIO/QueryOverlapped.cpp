//--------------------------------------------------------------------------------------
// QueryOverlapped.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "OverlappedSample.h"

//************************************
// Method:    QueryForOverlappedFinished
// Notes:     goes through and queries the current pending file operations to see which are completed
//************************************
void OverlappedSample::QueryForOverlappedFinished()
{
    for (uint32_t i = 0; i < c_maxRequestsInFlight; i++)
    {
        if (m_pendingOverlap[i].m_inUse)
        {
            DWORD actuallyTransferred;
            ReadFileIndex file = m_pendingOverlap[i].m_readFile;
            // the query pattern uses GetOverlappedResult directly as opposed to WaitFor###Object on an event
            // this might be useful for a title that is performing other work on a file loading thread
            // it can quickly go through and check the status of pending requests and continue doing other work if none have completed
            // at any point in time the title can check the status of a particular request, with the final parameter FALSE this can be fairly quick
            if (GetOverlappedResult(m_openFiles[file.first][file.second], &m_pendingOverlap[i].m_overlappedObject, &actuallyTransferred, FALSE))
            {
                assert(m_pendingOverlap[i].m_requestedReadSize == actuallyTransferred);
                m_pendingOverlap[i].m_inUse = false;
                m_numRequestsInFlight--;
            }
            else    // if GetOverlappedResult returns FALSE that means either an error or the data is not available yet
            {       // for this sample we only handle the case where data is not available yet
                assert(GetLastError() == ERROR_IO_INCOMPLETE);
            }

            // always make sure the event being used for the query pattern is not signaled to avoid false notification before reusing it
            ResetEvent(m_pendingOverlap[i].m_overlappedObject.hEvent);
        }
    }
}

//************************************
// Method:    QueryTypeThreadProc
// Notes:     thread procedure used for the query overlapped I/O pattern
//************************************
void OverlappedSample::QueryTypeThreadProc()
{
    uint32_t totalReads = 0;
    uint32_t totalReadSize = 0;
    for (uint32_t readIndex = 0; readIndex < m_readSizes.size (); readIndex++)
    {
        for (uint32_t currentRead = 0; currentRead < m_readSizes[readIndex].second; )
        {
            if (m_numRequestsInFlight < c_maxRequestsInFlight)
            {
                totalReads++;
                totalReadSize += m_readSizes[readIndex].first;
                currentRead++;

                uint32_t overlapIndex = InitializeBaseOverlappedBlock(m_readSizes[readIndex].first);

                // need to use an event with query as well as with the wait pattern
                // this is without the event the call to GetOverlappedResults will use notification based on the file handle itself
                // with multiple requests in flight for a file this can result in confusion on exactly which request finished
                m_pendingOverlap[overlapIndex].m_overlappedObject.hEvent = m_events[overlapIndex];

                ReadFileIndex readFile = m_pendingOverlap[overlapIndex].m_readFile;
                DWORD actuallyTransferred;

                // the variable that receives the number of bytes actually read is always useful when calling ReadFile even for overlapped operations
                // if the data is available immediately it will contain how much was read and avoid another call to GetOverlappedResult to query for the value
                // the same read buffer is used for all pending requests for this sample because we don't actually care about the data read from the file
                if (!ReadFile(m_openFiles[readFile.first][readFile.second], m_readBuffer, m_pendingOverlap[overlapIndex].m_requestedReadSize, &actuallyTransferred, &(m_pendingOverlap[overlapIndex].m_overlappedObject)))
                {
                    // if the OS actually starts an asynchronous operation then the error ERROR_IO_PENDING will be returned
                    // this is not an actual error even though ReadFile returned false
                    assert(GetLastError() == ERROR_IO_PENDING);
                    m_numRequestsInFlight++;
                }
                else    // asynchronous call to ReadFile was converted to a synchronous call by the OS, data is available now
                {       // this won't happen on Xbox due to FILE_FLAG_NO_BUFFERED, the small file cache, and no read ahead
                        // it's recommended to include this check for titles that also execute on PC where it can happen especially due to read ahead
                    assert(actuallyTransferred == m_pendingOverlap[overlapIndex].m_requestedReadSize);
                    m_pendingOverlap[overlapIndex].m_inUse = false;
                }
            }
            else
            {
                QueryForOverlappedFinished();
            }
        }
    }
    while (m_numRequestsInFlight)   // all of the requests have to created, wait for the rest of the unfinished to complete
    {
        QueryForOverlappedFinished();
    }

    assert(totalReads == m_totalNumReads);
    assert(totalReadSize == m_totalReadSize);

    m_typeDone = true;
}
