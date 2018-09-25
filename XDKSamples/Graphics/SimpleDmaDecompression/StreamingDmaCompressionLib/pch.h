//--------------------------------------------------------------------------------------
// pch.h
//
// Header for standard system include files.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#if defined(_XBOX_ONE) && defined(_TITLE)
#define _XDK_NO_XBLD_INFO
#include <xdk.h>

#if defined(D3D11X)
#include <d3d11_x.h>
#elif defined (D3D12X)
#include <d3d12_x.h>
#endif

#endif // _XBOX_ONE && _TITLE

#include <Windows.h>
#include <stdint.h>
#include <intrin.h>
#include <wrl.h>
#include <assert.h>

#include "..\zlib\zlib-1.2.8\zlib.h"
#include "..\zopfli\src\zopfli\zlib_container.h"

