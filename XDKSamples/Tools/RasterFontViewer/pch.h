//--------------------------------------------------------------------------------------
// pch.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#pragma warning(push)
#pragma warning(disable : 4005)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NODRAWTEXT
#define NOBITMAP
#define NOMCX
#define NOSERVICE
#define NOHELP
#pragma warning(pop)

#include <algorithm>
#include <stdio.h>
#include <tchar.h>
#include <exception>
#include <stdint.h>
#include <set>

#include <windows.h>

namespace DX
{
    // formatted exception helper class
    template<unsigned SIZE>
    class exception_fmt : public std::exception
    {
    public:
        exception_fmt(const char *msg, ...)
        {
            va_list args;
            va_start(args, msg);

            vsprintf_s(exnBuffer, msg, args);

            va_end(args);
        }

        virtual const char *what() const override
        {
            return exnBuffer;
        }

    private:
        static thread_local char exnBuffer[SIZE];
    };

    template<unsigned SIZE>
    thread_local char exception_fmt<SIZE>::exnBuffer[SIZE];

    // Helper class for Win32 errors
    class last_err_exception : public std::exception
    {
    public:
        last_err_exception(uint32_t err)
            : result(err) {}

        last_err_exception()
            : result(0)
        {
            result = GetLastError();
        }

        virtual const char *what() const override
        {
            static char s_str[64] = {};
            sprintf_s(s_str, "Failure with last error of %08X", result);
            return s_str;
        }

    private:
        uint32_t result;
    };

    // Convert the last error into an exception
    inline void ThrowLastError()
    {
        throw last_err_exception();
    }

    template<typename T>
    inline void ThrowLastErrWhenFalse(T result)
    {
        if (!result)
        {
            throw last_err_exception();
        }
    }

    // Helper class for COM exceptions
    class com_exception : public std::exception
    {
    public:
        com_exception(HRESULT hr) : result(hr) {}

        virtual const char* what() const override
        {
            static char s_str[64] = {};
            sprintf_s(s_str, "Failure with HRESULT of %08X", result);
            return s_str;
        }

    private:
        HRESULT result;
    };

    // Helper utility converts D3D API failures into exceptions.
    inline void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            throw com_exception(hr);
        }
    }
} // namespace DX



// TODO: reference additional headers your program requires here
