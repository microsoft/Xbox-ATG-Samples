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

#include <wrl.h>

#if defined(_XBOX_ONE) && defined(_TITLE)

#include <d3d12_x.h>
#include <d3dx12_x.h>

#else

#include <d3d12.h>
#include "d3dx12.h"
#include <dxgi1_4.h>

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

#endif

#include <DirectXMath.h>
#include <DirectXColors.h>


#include <algorithm>
#include <exception>
#include <memory>
#include <stdexcept>

#include <stdio.h>
#include <pix.h>

#include <ATGColors.h>
#include <CommonStates.h>
#include <DescriptorHeap.h>
#include <DirectXHelpers.h>
#include <Effects.h>
#include <GamePad.h>
#include <Keyboard.h>
#include <Model.h>
#include <Mouse.h>
#include <OrbitCamera.h>
#include <PrimitiveBatch.h>
#include <ResourceUploadBatch.h>
#include <SimpleMath.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <VertexTypes.h>
#include <WICTextureLoader.h>
#include <DDSTextureLoader.h>

namespace DX
{
    // Helper class for COM exceptions
    class com_exception : public std::exception
    {
    public:
        com_exception(HRESULT hr) : result(hr) {}

        virtual const char* what() const override
        {
            static char s_str[64] = { 0 };
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