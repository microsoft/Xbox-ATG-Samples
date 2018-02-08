//--------------------------------------------------------------------------------------
// pch.h
//
// Header for standard system include files.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

// Use the C++ standard templated min/max
#define NOMINMAX

#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <stdint.h>
#include <conio.h>
#include <assert.h>
#include <memory>

namespace DX
{
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
}