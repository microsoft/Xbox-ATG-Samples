//--------------------------------------------------------------------------------------
// StreamingDmaDecompression11.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "StreamingDmaDecompression.h"

namespace XboxDmaCompression 
{
	HRESULT InitStreamingDma11(ID3D11DeviceX* pDevice, ID3D11DmaEngineContextX* pDma, DmaKickoffBehavior behavior, ULONG64 threadAffinity = -1);
}
