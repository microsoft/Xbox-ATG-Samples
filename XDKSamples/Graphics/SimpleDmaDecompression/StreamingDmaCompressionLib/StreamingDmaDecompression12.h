//--------------------------------------------------------------------------------------
// StreamingDmaDecompression12.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "StreamingDmaDecompression.h"

namespace XboxDmaCompression 
{
	HRESULT InitStreamingDma12(ID3D12Device* pDevice, ID3D12CommandQueue* pCmdQueue, DmaKickoffBehavior behavior, ULONG64 threadAffinity = -1);
}
