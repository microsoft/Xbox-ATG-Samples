//--------------------------------------------------------------------------------------
// XboxDmaCompression.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "pch.h"

#include <vector>

namespace XboxDmaCompression
{
	//
	//	In this sample, both these compression options are limited to 4KB windows to maintain compatibility with the hardware engine
	//  but the internal details of each algorithm can be adjusted per the instructions that come with each third party library
	//
	//	Generally speaking, zlib is much faster at compression time, and Zopfli has a slightly higher compression ratio while maintaining
	//  compatibility with RCF 1951 (DEFLATE) https://www.ietf.org/rfc/rfc1951.txt
	//

	void ChunkedCompressWithZopfli(std::vector<uint8_t*> &destFragments, std::vector<uint32_t> &compressedSizes, std::vector<uint32_t> &originalSizes, _Out_ uint32_t* fragmentCount, _In_ uint8_t* pSrc, uint32_t srcSize);
	void ChunkedCompressWithZlib(std::vector<uint8_t*> &destFragments, std::vector<uint32_t> &compressedSizes, std::vector<uint32_t> &originalSizes, _Out_ uint32_t* fragmentCount, _In_ uint8_t* pSrc, uint32_t srcSize);
}