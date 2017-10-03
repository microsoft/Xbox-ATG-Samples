//--------------------------------------------------------------------------------------
// CallStack.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "pch.h"
#include "CallStack.h"
#include <mutex>
#include <SymbolResolve.h>
#include <OSLockable.h>
#include <ThreadHelpers.h>

using namespace CallStack;

size_t CallStack::CaptureBackTraceFromCurrentThread(void** addresses, HMODULE* modules, size_t framesToCapture)
{
    DWORD dwStackHash = 0;
    static const uint32_t FRAMES_TO_SKIP = 1;
    size_t numCapturedFrames = RtlCaptureStackBackTrace(FRAMES_TO_SKIP, static_cast<DWORD>(framesToCapture), addresses, &dwStackHash);

    for (uint32_t i = 0; i < numCapturedFrames; ++i)
    {
        uint64_t baseAddress = 0;
        RUNTIME_FUNCTION *runtimeFunction = ::RtlLookupFunctionEntry(reinterpret_cast<uint64_t>(addresses[i]), &baseAddress, nullptr);

        if (runtimeFunction == nullptr)
        {
            // If we don't have a RUNTIME_FUNCTION, then we've encountered
            // a leaf function.  Adjust the stack data appropriately.
            if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCWSTR>(addresses[i]), &modules[i]))
            {
                modules[i] = 0;
            }
        }
        else
        {
            modules[i] = reinterpret_cast<HMODULE>(baseAddress);
        }
    }

    return numCapturedFrames;
}

size_t CallStack::CaptureBackTraceFromContext(CONTEXT *contextRecord, void** addresses, HMODULE* modules, size_t framesToCapture)
{
    if (contextRecord == nullptr)
    {
        return 0;
    }

    CONTEXT ctx = *contextRecord;

    uint64_t baseAddress = 0;

    KNONVOLATILE_CONTEXT_POINTERS NvContext = {};
    void* handlerData = nullptr;
    uint64_t establisherFrame = 0;

    size_t numCapturedFrames = 0;
    do
    {
        RUNTIME_FUNCTION *runtimeFunction = RtlLookupFunctionEntry(ctx.Rip, &baseAddress, nullptr);

        modules[numCapturedFrames] = (HMODULE)baseAddress;
        addresses[numCapturedFrames] = reinterpret_cast<void*>(ctx.Rip);
        ++numCapturedFrames;

        if (runtimeFunction)
        {
            // unwind based on current context
            RtlVirtualUnwind(
                UNW_FLAG_NHANDLER,
                baseAddress,
                ctx.Rip,
                runtimeFunction,
                &ctx,
                &handlerData,
                &establisherFrame,
                &NvContext);
        }
        else
        {
            // If we don't have a RUNTIME_FUNCTION, then we've encountered
            // a leaf function.  Adjust the stack appropriately.
            ctx.Rip = *reinterpret_cast<uint64_t*>(ctx.Rsp);
            ctx.Rsp += 8;
        }
    } while (numCapturedFrames < framesToCapture && ctx.Rip != 0);

    return numCapturedFrames;
}

size_t CallStack::GetFrameCountFromContext(CONTEXT *contextRecord)
{
    if (contextRecord == nullptr)
    {
        return 0;
    }

    CONTEXT ctx = *contextRecord;

    uint64_t baseAddress = 0;

    KNONVOLATILE_CONTEXT_POINTERS NvContext = {};
    void* handlerData = nullptr;
    uint64_t establisherFrame = 0;

    size_t numCapturedFrames = 0;
    do
    {
        RUNTIME_FUNCTION *runtimeFunction = RtlLookupFunctionEntry(ctx.Rip, &baseAddress, nullptr);

        ++numCapturedFrames;

        if (runtimeFunction)
        {
            // unwind based on current context
            RtlVirtualUnwind(
                UNW_FLAG_NHANDLER,
                baseAddress,
                ctx.Rip,
                runtimeFunction,
                &ctx,
                &handlerData,
                &establisherFrame,
                &NvContext);
        }
        else
        {
            // If we don't have a RUNTIME_FUNCTION, then we've encountered
            // a leaf function.  Adjust the stack appropriately.
            ctx.Rip = *reinterpret_cast<uint64_t*>(ctx.Rsp);
            ctx.Rsp += 8;
        }
    } while (ctx.Rip != 0);

    return numCapturedFrames;
}

namespace
{
    static const size_t MAX_FRAMES_TO_CAPTURE = 64;
    void* g_addresses[MAX_FRAMES_TO_CAPTURE];
    HMODULE g_modules[MAX_FRAMES_TO_CAPTURE];
    ATG::CriticalSectionLockable g_frameDataMutex;
} // anonymous namespace

void BackTrace::Clear()
{
    m_frames.clear();
    memset(g_addresses, 0, sizeof(g_addresses));
    memset(g_modules, 0, sizeof(g_modules));
}

size_t BackTrace::CaptureCurrentThread()
{
    std::lock_guard<ATG::CriticalSectionLockable> scopedLock(g_frameDataMutex);
    Clear();
    size_t numCapturedFrames = CaptureBackTraceFromCurrentThread(g_addresses, g_modules, MAX_FRAMES_TO_CAPTURE);
    m_frames.resize(numCapturedFrames);
    for (size_t i = 0; i < numCapturedFrames; ++i)
    {
        frame_data &fd = m_frames[i];
        fd.moduleHandle = g_modules[i];
        fd.relativeVirtualAddress = reinterpret_cast<uint64_t>(g_addresses[i]) - reinterpret_cast<uint64_t>(g_modules[i]);
    }
    return numCapturedFrames;
}

size_t BackTrace::CaptureCrossThread(HANDLE whichThread)
{
    if (IsCurrentThread(whichThread))
    {
        return CaptureCurrentThread();
    }

    {
        std::lock_guard<ATG::CriticalSectionLockable> scopedLock(g_frameDataMutex);
        size_t numCapturedFrames = 0;

        CONTEXT ctx = {};
        ctx.ContextFlags = CONTEXT_FULL;
        {
            DX::ThreadSuspender susp(whichThread);
            if (GetThreadContext(whichThread, &ctx))
            {
                // Only want to suspend the thread for the duration of the capture
                numCapturedFrames = CaptureBackTraceFromContext(&ctx, g_addresses, g_modules, MAX_FRAMES_TO_CAPTURE);
            }
        }

        m_frames.resize(numCapturedFrames);
        for (size_t i = 0; i < numCapturedFrames; ++i)
        {
            frame_data &fd = m_frames[i];
            fd.moduleHandle = g_modules[i];
            fd.relativeVirtualAddress = reinterpret_cast<uint64_t>(g_addresses[i]) - reinterpret_cast<uint64_t>(g_modules[i]);
        }
        return numCapturedFrames;
    }
}

HRESULT BackTrace::Resolve(unsigned flags)
{
    std::vector<uint64_t> rawFrames;
    size_t numFrames = m_frames.size();

    if (numFrames == 0)
    {
        assert(numFrames > 0); // Probably forgot to capture frames (should at least have a frame for the capture method -- right?)
        return E_NOT_VALID_STATE;
    }

    rawFrames.resize(numFrames);
    for (size_t i = 0; i < numFrames; ++i)
    {
        rawFrames[i] = m_frames[i].relativeVirtualAddress + (uint64_t)m_frames[i].moduleHandle;
    }

    HRESULT hr = S_OK;
    if (SUCCEEDED(hr) && (flags & CAPTURE_FLAGS_SYMBOL_INFO) == CAPTURE_FLAGS_SYMBOL_INFO)
    {
        hr = ResolveSymbolInfo(rawFrames.size(), &rawFrames[0]);
    }

    if (SUCCEEDED(hr) && (flags & CAPTURE_FLAGS_SOURCE_INFO) == CAPTURE_FLAGS_SOURCE_INFO)
    {
        hr = ResolveSourceInfo(rawFrames.size(), &rawFrames[0]);
    }
    return hr;
}

HRESULT BackTrace::ResolveSymbolInfo(size_t numFrames, ULONG_PTR *addresses)
{
    ResolutionContext ctx;
    ctx.whichFrame = 0;
    ctx.frames = &m_frames[0];

    return ::GetSymbolFromAddress(ResolveDisposition::DefaultPriority, static_cast<DWORD>(numFrames), addresses, s_SymCallback, &ctx);
}

HRESULT BackTrace::ResolveSourceInfo(size_t numFrames, ULONG_PTR *addresses)
{
    ResolutionContext ctx;
    ctx.whichFrame = 0;
    ctx.frames = &m_frames[0];

    return ::GetSourceLineFromAddress(ResolveDisposition::DefaultPriority, static_cast<DWORD>(numFrames), addresses, s_SrcCallback, &ctx);
}

BOOL BackTrace::s_SymCallback(LPVOID context, ULONG_PTR symbolAddress, HRESULT symbolResult, PCWSTR symName, ULONG offset)
{
    UNREFERENCED_PARAMETER(symbolAddress);

    if (context == nullptr)
    {
        assert(context != nullptr);
        // Yikes!! (this could only be a programming error)
        return FALSE;
    }

    ResolutionContext &ctx = *reinterpret_cast<ResolutionContext*>(context);
    unsigned i = ctx.whichFrame++;
    ctx.frames[i].symbolResult = symbolResult;
    if (SUCCEEDED(symbolResult))
    {
        ctx.frames[i].symbolName = symName;
        ctx.frames[i].symbolOffset = offset;
    }
    return TRUE;
}

BOOL BackTrace::s_SrcCallback(LPVOID context, ULONG_PTR symbolAddress, HRESULT sourceResult, PCWSTR filepath, ULONG lineNumber)
{
    UNREFERENCED_PARAMETER(symbolAddress);

    if (context == nullptr)
    {
        assert(context != nullptr);
        // Yikes!! (this could only be a programming error)
        return FALSE;
    }

    ResolutionContext &ctx = *reinterpret_cast<ResolutionContext*>(context);
    unsigned i = ctx.whichFrame++;
    ctx.frames[i].sourceResult = sourceResult;
    if (SUCCEEDED(sourceResult))
    {
        ctx.frames[i].sourceFileName = filepath;
        ctx.frames[i].sourceLineNumber = lineNumber;
    }
    return TRUE;
}

bool BackTrace::IsCurrentThread(HANDLE whichThread) const
{
    return GetThreadId(whichThread) == ::GetCurrentThreadId();
}
