//--------------------------------------------------------------------------------------
// FBC_CPU.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

class CompressorCPU
{
public:
    CompressorCPU();

    CompressorCPU(const CompressorCPU&) = delete;
    CompressorCPU& operator=(const CompressorCPU&) = delete;

    CompressorCPU(CompressorCPU&&) = default;
    CompressorCPU& operator=(CompressorCPU&&) = default;

	HRESULT Prepare(uint32_t texSize, DXGI_FORMAT bcFormat, uint32_t mipLevels, std::unique_ptr<uint8_t, aligned_deleter>&result, std::vector<D3D12_SUBRESOURCE_DATA>& subresources);

	//
	// pixels here must be in DXGI_FORMAT_R8G8B8A8_UNORM format
	// each miplevel must start on a 16-byte aligned boundary
	// the 2x2 and 1x1 miplevels must be 4x4 in size created through replication of pixels
	//
    HRESULT Compress(uint32_t texSize, uint32_t mipLevels, _In_reads_(mipLevels) const D3D12_SUBRESOURCE_DATA* subresources, DXGI_FORMAT bcFormat, _In_reads_(mipLevels) const D3D12_SUBRESOURCE_DATA* bcSubresources);

private:
};

