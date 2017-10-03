//--------------------------------------------------------------------------------------
// File: CallStack.h
//
// Diagnostic helpers for working with callstacks
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright(c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <vector>
#include <stdexcept>

namespace CallStack
{
    // Capture the backtrace for the current thread
    // Parameters:
    //  addresses -- receives the return addresses from each stack frame in the current call stack
    //  modules  -- receives the module handle corresponding to the return address for each stack frame
    //  framesToCapture -- indicates the number of frames to capture
    // Return value:
    //  Returns the number of frames that were captured
    // Note: the caller must allocate enough space in addresses and modules to accomodate the number of
    //       frames specified by framesToCapture
    size_t CaptureBackTraceFromCurrentThread(void** addresses, HMODULE *modules, size_t framesToCapture);

    // Capture the backtrace starting with the provided context record.
    // Parameters:
    //  contextRecord: address of a context record (register data)
    //  addresses -- receives the return addresses from each stack frame in the current call stack
    //  modules  -- receives the module handle corresponding to the return address for each stack frame
    //  framesToCapture -- indicates the number of frames to capture
    // Return value:
    //  Returns the number of frames that were captured
    // Note: the caller must allocate enough space in addresses and modules to accomodate the number of
    //       frames specified by framesToCapture
    size_t CaptureBackTraceFromContext(CONTEXT *contextRecord, void** addresses, HMODULE* modules, size_t framesToCapture);

    // Count the number of stack frames in the callstack starting with the provided context record.
    // Parameters:
    //  contextRecord: address of a context record (register data)
    // Return value:
    //  Returns the number of frames
    // Remarks:
    //   This is provided, for "completeness", so that you may allocate memory for a backtrace. I.e first you call GetFrameCountFromContext,
    //   then you allocate space for that many addresses and module handles, finally, you call CaptureBackTraceFromContext.
    //   Bear in mind that it is not quite that easy!:
    //   In particular, you must "freeze" the thread corresponding to the provided context record during the entire process.
    //   Put differently, you cannot let the thread add and remove stack frames (otherwise the count will be wrong.) This
    //   becomes problematic because in the case when the thread is holding a lock to the heap, then if you try to allocate
    //   from the same heap it will cause a deadlock.
    //   The takeaway is that you will need a separate allocation mechanism (e.g. a "debug heap") if you are going to try
    //   to use this pattern. An alternative, simpler, approach is to just set asside some static memory that is "big enough"
    //   for most callstacks. This is the strategy used by the BackTrace class below.
    size_t GetFrameCountFromContext(CONTEXT *contextRecord);

    // Utility for capturing a backtrace
    class BackTrace
    {
    public:
        BackTrace() = default;

        // Data for a stack frame
        struct frame_data
        {
            frame_data()
                : moduleHandle(0)
                , relativeVirtualAddress(0)
                , symbolOffset(0)
                , sourceLineNumber(0)
                , symbolResult(E_NOT_SET)
                , sourceResult(E_NOT_SET)
            {}

            // The module handle also happens to be numerically identical to the base address of the module
            HMODULE  moduleHandle;

            // The relative virual address is the offset of the symbol from the module base address
            uint64_t relativeVirtualAddress;

            // symbol name information
            std::wstring symbolName;
            uint64_t symbolOffset;

            // source file information
            std::wstring sourceFileName;
            uint64_t sourceLineNumber;

            // error information from the symbol proxy
            HRESULT symbolResult;
            HRESULT sourceResult;
        };

        // Number of frames in the backtrace
        size_t size() const
        {
            return m_frames.size();
        }

        // Access to frames by index
        const frame_data &operator [] (unsigned idx) const
        {
            if (idx < m_frames.size())
            {
                return m_frames[idx];
            }
            throw std::out_of_range("Index is out of range");
        }

        // Capture a backtrace from the current thread
        // Result:
        //  Returns the number of frames captured
        size_t CaptureCurrentThread();

        // Capture a backtrace from the thread specified by the provided handle
        // Parameter:
        //  whichThread -- specifies the thread for which to capture a backtrace
        // Result:
        //  Returns the number of frames captured
        size_t CaptureCrossThread(HANDLE whichThread);

        static const unsigned CAPTURE_FLAGS_NONE = 0;
        static const unsigned CAPTURE_FLAGS_SYMBOL_INFO = 0x1; // Use this to resolve symbol name and offset
        static const unsigned CAPTURE_FLAGS_SOURCE_INFO = 0x2; // Use this to resolve source filename and line number
        static const unsigned CAPTURE_FLAGS_DEFAULT = CAPTURE_FLAGS_SYMBOL_INFO | CAPTURE_FLAGS_SOURCE_INFO;

        // Resolves the symbol information for the backtrace
        // Parameter:
        //  captureFlags -- indicates which symbol information to resolve
        HRESULT Resolve(unsigned captureFlags = CAPTURE_FLAGS_DEFAULT);

    private:
        void Clear();
        bool IsCurrentThread(HANDLE whichThread) const;

        std::vector<frame_data> m_frames;

        HRESULT ResolveSymbolInfo(size_t numFrames, ULONG_PTR *addresses);
        HRESULT ResolveSourceInfo(size_t numFrames, ULONG_PTR *addresses);

        struct ResolutionContext
        {
            unsigned whichFrame;
            frame_data *frames;
        };
        static BOOL CALLBACK s_SymCallback(LPVOID context, ULONG_PTR symbolAddress, HRESULT symbolResult, PCWSTR symName, ULONG offset);
        static BOOL CALLBACK s_SrcCallback(LPVOID context, ULONG_PTR symbolAddress, HRESULT sourceResult, PCWSTR filepath, ULONG lineNumber);
    };
} // namespace CallStack
