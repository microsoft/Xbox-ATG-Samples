//--------------------------------------------------------------------------------------
// WaitOverlapped.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "OverlappedSample.h"

//************************************
// Method:    WaitForEventOverlappedToFinish
// Notes:     Main routine that waits for any event pattern overlapped I/O to complete
//************************************
void OverlappedSample::WaitForEventOverlappedToFinish()
{
    // check until none are finished
    bool foundFirstOne = false;
    for (;;)
    {
        if (m_numRequestsInFlight == 0)
            return;

        // we only want to wait for one request to finish versus all of the pending however multiple could complete at the same time
        // because of this we need to sit in a loop calling WaitForMultipleObjects until none are marked as finished
        // so have a timeout on the wait of either 0 or forever depending on if we're waiting for the initial request to complete
        // we actually can use the entire array of cached events. the events not currently bound to an I/O operation will not signal
        // so we won't get spurious wakeups.
        DWORD waitRet = WaitForMultipleObjects(c_maxRequestsInFlight, m_events, FALSE, foundFirstOne ? 0 : INFINITE);

        // none are complete, however at least one completed during this call since the first call uses a timeout of 0
        // returning allows EventTypeThreadProc to issue more requests up to c_maxRequestsInFlight
        if (waitRet == WAIT_TIMEOUT)
            return;
        foundFirstOne = true;
        uint32_t whichFinished = waitRet - WAIT_OBJECT_0;
        assert(whichFinished < c_maxRequestsInFlight);
        DWORD actuallyTransferred;
        ReadFileIndex file = m_pendingOverlap[whichFinished].m_readFile;

        // now that we know which request finished we need to check that all data was actually read which is what GetOverlappedResult will return
        // we don't need GetOverlappedResult to wait for the request to finish so pass FALSE for the final parameter
        // it is possible that other requests finished as well by the time this thread resumed from the WaitForMultipleObjects
        // an option would be to use GetOverlappedResult to iterate through all pending operations, you would do the entire list since any of the operations could
        // have finished by the time the thread gets to this section. could also just do from the signaled to the end and pick up the others the next time through the loop
        // this sample explicitly just catches one at a time through the loop since this is focusing on using WaitForMultipleObjects to get notification.
        // see QueryOverlapped.cpp for a demonstration of iterating through the list of pending operations. the readme also has a discussion on mixing the two patterns
        if (!GetOverlappedResult(m_openFiles[file.first][file.second], &m_pendingOverlap[whichFinished].m_overlappedObject, &actuallyTransferred, FALSE))
        {
            assert(false);
        }
        else
        {
            assert(m_pendingOverlap[whichFinished].m_requestedReadSize == actuallyTransferred);
        }
        // always make sure the event being used for the wait pattern is not signaled to avoid false notification before reusing it
        ResetEvent(m_pendingOverlap[whichFinished].m_overlappedObject.hEvent);

        m_pendingOverlap[whichFinished].m_inUse = false;
        m_numRequestsInFlight--;
    }
}

//************************************
// Method:    EventTypeThreadProc
// Notes:     thread proc for event pattern overlapped I/O. keeps requesting overlapped reads while the number in flight is less than the maximum allowed
//            when the maximum is hit wait for some of them to complete
//************************************
void OverlappedSample::EventTypeThreadProc()
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

                // The wait pattern requires an associated event that can be used in WaitForSingleObject or WaitForMultipleObjects
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
            else        // at the maximum number of requests in flight so wait for some of them to complete
            {           // this could happen on any thread desired, for example one thread is constantly adding requests to a queue
                        // and another thread is waiting for requests to finish
                WaitForEventOverlappedToFinish();
            }
        }
    }
    while (m_numRequestsInFlight)   // all of the requests have to created, wait for the rest of the unfinished to complete
    {
        WaitForEventOverlappedToFinish();
    }

    assert(totalReads == m_totalNumReads);
    assert(totalReadSize == m_totalReadSize);
    m_typeDone = true;
}
