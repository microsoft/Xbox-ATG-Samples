//--------------------------------------------------------------------------------------
// FBC_GPU.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

class CompressorGPU
{
public:
    CompressorGPU();

    CompressorGPU(const CompressorGPU&) = delete;
    CompressorGPU& operator=(const CompressorGPU&) = delete;

    CompressorGPU(CompressorGPU&&) = default;
    CompressorGPU& operator=(CompressorGPU&&) = default;

    void Initialize(
        _In_ ID3D12Device* device,
        D3D12_CPU_DESCRIPTOR_HANDLE intermediateUavsCPU[D3D12_REQ_MIP_LEVELS],
        D3D12_GPU_DESCRIPTOR_HANDLE intermediateUavsGPU[D3D12_REQ_MIP_LEVELS],
        D3D12_CPU_DESCRIPTOR_HANDLE bcTextureSrvsCPU[D3D12_REQ_MIP_LEVELS]);
    void ReleaseDevice();

	HRESULT Prepare(
		uint32_t texSize, DXGI_FORMAT bcFormat,
		uint32_t mipLevels,
		_Outptr_ ID3D12Resource** bcTexture,
		_Outptr_ void** bcTextureMem,
		_Outptr_ ID3D12Resource** intermediateUAV,
		_Outptr_ ID3D12Resource** p2x2intermediateUAV,
		_Outptr_ ID3D12Resource** p1x1intermediateUAV,
		bool generateMips = false);

    HRESULT Compress(
        _In_ ID3D12GraphicsCommandList* commandList,
        uint32_t texSize, DXGI_FORMAT bcFormat,
        uint32_t mipLevels, const D3D12_GPU_DESCRIPTOR_HANDLE* inputTexture,
        bool generateMips = false);

    static void FreeMemory(_In_opt_ void *textureMem);

private:
    Microsoft::WRL::ComPtr<ID3D12Device>        m_device;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSig;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_bc1Compress;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_bc1CompressTwoMips;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_bc1CompressTailMips;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_bc3Compress;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_bc3CompressTwoMips;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_bc3CompressTailMips;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_bc5Compress;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_bc5CompressTwoMips;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_bc5CompressTailMips;

    D3D12_CPU_DESCRIPTOR_HANDLE m_intermediateUavsCPU[D3D12_REQ_MIP_LEVELS];
    D3D12_GPU_DESCRIPTOR_HANDLE m_intermediateUavsGPU[D3D12_REQ_MIP_LEVELS];
    D3D12_CPU_DESCRIPTOR_HANDLE m_bcTextureSrvsCPU[D3D12_REQ_MIP_LEVELS];
};

