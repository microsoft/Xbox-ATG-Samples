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

#include <xdk.h>

#if _XDK_VER < 0x38390403 /* XDK Edition 160800 */
#error This sample requires the August 2016 XDK or later
#endif

#include <wrl.h>

#include <d3d11_x.h>
#include <DirectXMath.h>
#include <DirectXColors.h>

#include <algorithm>
#include <array>
#include <exception>
#include <memory>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include <stdio.h>
#include <stdlib.h>
#include <pix.h>
#include <xgmemory.h>

#include "ATGColors.h"
#include "CommonStates.h"
#include "ControllerFont.h"
#include "DDSTextureLoader.h"
#include "DirectXHelpers.h"
#include "Effects.h"
#include "GamePad.h"
#include "GeometricPrimitive.h"
#include "GraphicsMemory.h"
#include "Model.h"
#include "PerformanceTimersXbox.h"
#include "PIXHelpers.h"
#include "PostProcess.h"
#include "ReadData.h"
#include "SimpleMath.h"
#include "SpriteBatch.h"
#include "SpriteFont.h"
#include "VertexTypes.h"

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
            sprintf_s(s_str, "Failure with HRESULT of %08X", static_cast<unsigned int>(result));
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

namespace ATG
{
    void CreateColorTextureAndViews(
        ID3D11Device* pDev,
        UINT width,
        UINT height,
        DXGI_FORMAT fmt,
        ID3D11Texture2D** ppTexture,
        ID3D11RenderTargetView** ppRTV,
        ID3D11ShaderResourceView** ppSRV,
        D3D11_USAGE usage = D3D11_USAGE_DEFAULT);

    void CreateDepthTextureAndViews(
        ID3D11Device* pDev,
        UINT width,
        UINT height,
        DXGI_FORMAT fmt,
        ID3D11Texture2D** ppTexture,
        ID3D11DepthStencilView** ppDSV,
        ID3D11ShaderResourceView** ppSRV,
        D3D11_USAGE usage = D3D11_USAGE_DEFAULT);
}
