//--------------------------------------------------------------------------------------
// OSHelpers.h
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------
#pragma once

// It's required to include the proper OS header based on target platform before including this header

// Provides wrappers around various Windows objects to make them act as RAII
namespace DX
{
    struct handle_closer { void operator()(HANDLE h) { if (h) CloseHandle(h); } };

    typedef std::unique_ptr<void, handle_closer> ScopedHandle;

    inline HANDLE safe_handle(HANDLE h) { return (h == INVALID_HANDLE_VALUE) ? nullptr : h; }
}
