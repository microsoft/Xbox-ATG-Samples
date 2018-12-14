//--------------------------------------------------------------------------------------
// FBC_GPU.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "FBC_GPU.h"

#include "ReadData.h"

using Microsoft::WRL::ComPtr;

namespace
{
	inline bool ispow2(_In_ size_t x)
	{
		return ((x != 0) && !(x & (x - 1)));
	}

	const uint32_t COMPRESS_TWO_MIPS_SIZE_THRESHOLD = 512;

	const uint32_t COMPRESS_ONE_MIP_THREADGROUP_WIDTH = 8;
	const uint32_t COMPRESS_TWO_MIPS_THREADGROUP_WIDTH = 16;

	enum GPURootParameterIndex
	{
		GPU_ConstantBuffer,
		GPU_TextureSRV,
		GPU_TextureUAV_0,
		GPU_TextureUAV_1,
		GPU_TextureUAV_2,
		GPU_TextureUAV_3,
		GPU_TextureUAV_4,
		GPU_Count
	};

    // Constant buffer for block compression shaders
    __declspec(align(16)) struct ConstantBufferBC
    {
        float g_oneOverTextureWidth;
        uint32_t pad[3];
    };

    static_assert((sizeof(ConstantBufferBC) % 16) == 0, "CB size not padded correctly");

    const uint64_t c_XMemAllocAttributes = MAKE_XALLOC_ATTRIBUTES(
        eXALLOCAllocatorId_MiddlewareReservedMin,
        0,
        XALLOC_MEMTYPE_GRAPHICS_WRITECOMBINE,
        XALLOC_PAGESIZE_64KB,
        XALLOC_ALIGNMENT_64K);
}

CompressorGPU::CompressorGPU() :
    m_intermediateUavsCPU{},
    m_intermediateUavsGPU{}
{
}

_Use_decl_annotations_
void CompressorGPU::Initialize(
    ID3D12Device* device,
    D3D12_CPU_DESCRIPTOR_HANDLE intermediateUavsCPU[D3D12_REQ_MIP_LEVELS],
    D3D12_GPU_DESCRIPTOR_HANDLE intermediateUavsGPU[D3D12_REQ_MIP_LEVELS],
    D3D12_CPU_DESCRIPTOR_HANDLE bcTextureSrvsCPU[D3D12_REQ_MIP_LEVELS])
{
    auto blob = DX::ReadData(L"BC1Compress.cso");

    // Xbox One best practice is to use HLSL-based root signatures to support shader precompilation.

    DX::ThrowIfFailed(
        device->CreateRootSignature(0, blob.data(), blob.size(),
            IID_GRAPHICS_PPV_ARGS(m_rootSig.ReleaseAndGetAddressOf())));

    m_rootSig->SetName(L"BCCompress RS");

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_rootSig.Get();

    // BC1
    {
        psoDesc.CS.pShaderBytecode = blob.data();
        psoDesc.CS.BytecodeLength = blob.size();

        DX::ThrowIfFailed(
            device->CreateComputePipelineState(&psoDesc, IID_GRAPHICS_PPV_ARGS(m_bc1Compress.ReleaseAndGetAddressOf())));

        m_bc1Compress->SetName(L"BC1 Compress PSO");

        blob = DX::ReadData(L"BC1Compress2Mips.cso");

        psoDesc.CS.pShaderBytecode = blob.data();
        psoDesc.CS.BytecodeLength = blob.size();

        DX::ThrowIfFailed(
            device->CreateComputePipelineState(&psoDesc, IID_GRAPHICS_PPV_ARGS(m_bc1CompressTwoMips.ReleaseAndGetAddressOf())));

        m_bc1CompressTwoMips->SetName(L"BC1 Compress 2Mips PSO");

        blob = DX::ReadData(L"BC1CompressTailMips.cso");

        psoDesc.CS.pShaderBytecode = blob.data();
        psoDesc.CS.BytecodeLength = blob.size();

        DX::ThrowIfFailed(
            device->CreateComputePipelineState(&psoDesc, IID_GRAPHICS_PPV_ARGS(m_bc1CompressTailMips.ReleaseAndGetAddressOf())));

        m_bc1CompressTailMips->SetName(L"BC1 Compress Tails PSO");
    }

    // BC3
    {
        blob = DX::ReadData(L"BC3Compress.cso");

        psoDesc.CS.pShaderBytecode = blob.data();
        psoDesc.CS.BytecodeLength = blob.size();

        DX::ThrowIfFailed(
            device->CreateComputePipelineState(&psoDesc, IID_GRAPHICS_PPV_ARGS(m_bc3Compress.ReleaseAndGetAddressOf())));

        m_bc3Compress->SetName(L"BC3 Compress PSO");

        blob = DX::ReadData(L"BC3Compress2Mips.cso");

        psoDesc.CS.pShaderBytecode = blob.data();
        psoDesc.CS.BytecodeLength = blob.size();

        DX::ThrowIfFailed(
            device->CreateComputePipelineState(&psoDesc, IID_GRAPHICS_PPV_ARGS(m_bc3CompressTwoMips.ReleaseAndGetAddressOf())));

        m_bc3CompressTwoMips->SetName(L"BC3 Compress 2Mips PSO");

        blob = DX::ReadData(L"BC3CompressTailMips.cso");

        psoDesc.CS.pShaderBytecode = blob.data();
        psoDesc.CS.BytecodeLength = blob.size();

        DX::ThrowIfFailed(
            device->CreateComputePipelineState(&psoDesc, IID_GRAPHICS_PPV_ARGS(m_bc3CompressTailMips.ReleaseAndGetAddressOf())));

        m_bc3CompressTailMips->SetName(L"BC3 Compress Tails PSO");
    }

    // BC5
    {
        blob = DX::ReadData(L"BC5Compress.cso");

        psoDesc.CS.pShaderBytecode = blob.data();
        psoDesc.CS.BytecodeLength = blob.size();

        DX::ThrowIfFailed(
            device->CreateComputePipelineState(&psoDesc, IID_GRAPHICS_PPV_ARGS(m_bc5Compress.ReleaseAndGetAddressOf())));

        m_bc5Compress->SetName(L"BC5 Compress PSO");

        blob = DX::ReadData(L"BC5Compress2Mips.cso");

        psoDesc.CS.pShaderBytecode = blob.data();
        psoDesc.CS.BytecodeLength = blob.size();

        DX::ThrowIfFailed(
            device->CreateComputePipelineState(&psoDesc, IID_GRAPHICS_PPV_ARGS(m_bc5CompressTwoMips.ReleaseAndGetAddressOf())));

        m_bc5CompressTwoMips->SetName(L"BC5 Compress 2Mips PSO");

        blob = DX::ReadData(L"BC5CompressTailMips.cso");

        psoDesc.CS.pShaderBytecode = blob.data();
        psoDesc.CS.BytecodeLength = blob.size();

        DX::ThrowIfFailed(
            device->CreateComputePipelineState(&psoDesc, IID_GRAPHICS_PPV_ARGS(m_bc5CompressTailMips.ReleaseAndGetAddressOf())));

        m_bc5CompressTailMips->SetName(L"BC5 Compress Tails PSO");
    }

    m_device = device;

    memcpy_s(&m_intermediateUavsCPU, sizeof(m_intermediateUavsCPU), intermediateUavsCPU, sizeof(D3D12_CPU_DESCRIPTOR_HANDLE) * D3D12_REQ_MIP_LEVELS);
    memcpy_s(&m_intermediateUavsGPU, sizeof(m_intermediateUavsGPU), intermediateUavsGPU, sizeof(D3D12_GPU_DESCRIPTOR_HANDLE) * D3D12_REQ_MIP_LEVELS);

    memcpy_s(&m_bcTextureSrvsCPU, sizeof(m_bcTextureSrvsCPU), bcTextureSrvsCPU, sizeof(D3D12_CPU_DESCRIPTOR_HANDLE) * D3D12_REQ_MIP_LEVELS);
}

void CompressorGPU::ReleaseDevice()
{
    m_rootSig.Reset();

    m_bc1Compress.Reset();
    m_bc1CompressTwoMips.Reset();
    m_bc1CompressTailMips.Reset();

    m_bc3Compress.Reset();
    m_bc3CompressTwoMips.Reset();
    m_bc3CompressTailMips.Reset();

    m_bc5Compress.Reset();
    m_bc5CompressTwoMips.Reset();
    m_bc5CompressTailMips.Reset();

    m_device.Reset();

    memset(&m_intermediateUavsCPU, 0, sizeof(m_intermediateUavsCPU));
    memset(&m_intermediateUavsGPU, 0, sizeof(m_intermediateUavsGPU));

    memset(&m_bcTextureSrvsCPU, 0, sizeof(m_bcTextureSrvsCPU));
}

_Use_decl_annotations_
HRESULT CompressorGPU::Prepare(
	uint32_t texSize, DXGI_FORMAT bcFormat,
	uint32_t mipLevels,
	ID3D12Resource** bcTexture,
	void** bcTextureMem,
	ID3D12Resource** intermediateUAV,
	ID3D12Resource** p2x2intermediateUAV,
	ID3D12Resource** p1x1intermediateUAV,
	bool generateMips)
{
	if (texSize < 16 || !mipLevels || mipLevels > D3D12_REQ_MIP_LEVELS)
		return E_INVALIDARG;

	if (!bcTextureMem || !bcTexture)
		return E_INVALIDARG;

	*bcTextureMem = nullptr;
	*bcTexture = nullptr;

	DXGI_FORMAT intermediateFormat = DXGI_FORMAT_UNKNOWN;
	switch (bcFormat)
	{
	case DXGI_FORMAT_BC1_UNORM:
		intermediateFormat = DXGI_FORMAT_R32G32_UINT;
		break;

	case DXGI_FORMAT_BC3_UNORM:
		intermediateFormat = DXGI_FORMAT_R32G32B32A32_UINT;
		break;

	case DXGI_FORMAT_BC5_UNORM:
		intermediateFormat = DXGI_FORMAT_R32G32B32A32_UINT;
		break;

	default:
		return E_INVALIDARG;
	}

	// We only support square, power-of-two textures. This makes life easier because every mip level 
	// (above 2x2) of a power-of-two texture will be divisible by 4. It wouldn't be much additional 
	// work to support more general textures, but you would need to properly handle reading off the 
	// edge when a mip level isn't divisible by 4.
	if (!ispow2(texSize))
		return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

	uint32_t numMips = 0;
	uint32_t numIntermediateMips = 0;
	if (generateMips)
	{
		numMips = mipLevels;
		numIntermediateMips = numMips - 2;
	}
	else
	{
		numMips = numIntermediateMips = 1;
	}

	//
	// Xbox One can avoid the need to do CopyResource / CopySubresourceRegion to get the intermediate value into a BC texture
	// by alising the video memory directly.
	//

	{
		// Calculate the size and alignment required to create our compressed texture in memory
		XG_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = texSize;
		texDesc.Height = texSize;
		texDesc.ArraySize = 1;
		texDesc.MipLevels = numMips;
		texDesc.Format = static_cast<XG_FORMAT>(bcFormat);
		texDesc.BindFlags = XG_BIND_SHADER_RESOURCE;
		texDesc.SampleDesc.Count = 1;
		texDesc.TileMode = XGComputeOptimalTileMode(
			XG_RESOURCE_DIMENSION_TEXTURE2D,
			texDesc.Format, texDesc.Width, texDesc.Height,
			texDesc.ArraySize, texDesc.SampleDesc.Count, texDesc.BindFlags);

		ComPtr<XGTextureAddressComputer> computer;
		DX::ThrowIfFailed(XGCreateTexture2DComputer(&texDesc, computer.GetAddressOf()));

		XG_RESOURCE_LAYOUT layout;
		DX::ThrowIfFailed(computer->GetResourceLayout(&layout));

#ifdef _DEBUG
		// Verify that the layout for our intermediate texture is the same as the layout for our block compressed texture.
		{
			XG_TEXTURE2D_DESC intermediateTexDesc = {};
			intermediateTexDesc.Width = texSize / 4;
			intermediateTexDesc.Height = texSize / 4;
			intermediateTexDesc.ArraySize = 1;
			intermediateTexDesc.MipLevels = numIntermediateMips;
			intermediateTexDesc.Format = static_cast<XG_FORMAT>(intermediateFormat);
			intermediateTexDesc.BindFlags = XG_BIND_SHADER_RESOURCE;
			intermediateTexDesc.SampleDesc.Count = 1;
			intermediateTexDesc.TileMode = XGComputeOptimalTileMode(
				XG_RESOURCE_DIMENSION_TEXTURE2D,
				intermediateTexDesc.Format, intermediateTexDesc.Width, intermediateTexDesc.Height,
				intermediateTexDesc.ArraySize, intermediateTexDesc.SampleDesc.Count, intermediateTexDesc.BindFlags);

			assert(texDesc.TileMode == intermediateTexDesc.TileMode);

			XGTextureAddressComputer* pIntermediateComputer = nullptr;
			DX::ThrowIfFailed(XGCreateTexture2DComputer(&intermediateTexDesc, &pIntermediateComputer));

			XG_RESOURCE_LAYOUT intermediateLayout;
			DX::ThrowIfFailed(pIntermediateComputer->GetResourceLayout(&intermediateLayout));

			assert(intermediateTexDesc.TileMode == texDesc.TileMode);
			assert(layout.SizeBytes >= intermediateLayout.SizeBytes);
			assert(layout.BaseAlignmentBytes == intermediateLayout.BaseAlignmentBytes);
			for (uint32_t i = 0; i < numIntermediateMips; ++i)
			{
				assert(computer->GetMipLevelOffsetBytes(0, i) == pIntermediateComputer->GetMipLevelOffsetBytes(0, i));
			}
		}
#endif

		UINT64 alignmentBytes = std::max<UINT64>(4 * 1024, layout.BaseAlignmentBytes);
		UINT64 sizeBytes = DirectX::AlignUp(layout.SizeBytes, alignmentBytes);

		*bcTextureMem = XMemAlloc(sizeBytes, c_XMemAllocAttributes);
		if (!*bcTextureMem)
			throw std::exception("Out of video memory");

		// Create our block compressed texture
		auto desc = CD3DX12_RESOURCE_DESC::Tex2D(bcFormat, texSize, texSize, 1, static_cast<UINT16>(numMips));
		desc.Layout = static_cast<D3D12_TEXTURE_LAYOUT>(0x100 | texDesc.TileMode);

		DX::ThrowIfFailed(
			m_device->CreatePlacedResourceX(
				reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS>(*bcTextureMem),
				&desc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				nullptr,
				IID_GRAPHICS_PPV_ARGS(bcTexture))
		);

		// Create our intermediate texture
		desc.Width = texSize / 4;   // 4 pixels per block in each dimension
		desc.Height = texSize / 4;
		desc.MipLevels = static_cast<UINT16>(numIntermediateMips);
		desc.Format = intermediateFormat;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		DX::ThrowIfFailed(
			m_device->CreatePlacedResourceX(
				reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS>(*bcTextureMem),
				&desc,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				nullptr,
				IID_GRAPHICS_PPV_ARGS(intermediateUAV))
		);

		// Create a UAV for each mip level of our intermediate texture
		D3D12_UNORDERED_ACCESS_VIEW_DESC descUav = {};
		descUav.Format = intermediateFormat;
		descUav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		for (uint32_t i = 0; i < numIntermediateMips; ++i)
		{
			descUav.Texture2D.MipSlice = i;
			m_device->CreateUnorderedAccessView(*intermediateUAV, nullptr, &descUav, m_intermediateUavsCPU[i]);
		}

		// Create extra intermediate textures for the two lowest mips, which are not covered by the primary intermediate texture
		if (generateMips)
		{
			auto p2x2MipMem = reinterpret_cast<uint8_t*>(*bcTextureMem) + computer->GetMipLevelOffsetBytes(0, numMips - 2);
			desc.Width = 1;
			desc.Height = 1;
			desc.MipLevels = 1;

			// We use the LINEAR tile mode here due to its loose alignment restrictions; this allows us to alias the 2x2 and 1x1 intermediate textures
			// at exactly the right spot in memory for the appropriate mips. In terms of data layout, LINEAR and 2D_THIN should be identical for these
			// small mip levels.
			desc.Layout = static_cast<D3D12_TEXTURE_LAYOUT>(0x100 | XG_TILE_MODE_LINEAR);

			DX::ThrowIfFailed(
				m_device->CreatePlacedResourceX(
					reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS>(p2x2MipMem),
					&desc,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					nullptr,
					IID_GRAPHICS_PPV_ARGS(p2x2intermediateUAV))
			);

			auto p1x1MipMem = reinterpret_cast<uint8_t*>(*bcTextureMem) + computer->GetMipLevelOffsetBytes(0, numMips - 1);

			DX::ThrowIfFailed(
				m_device->CreatePlacedResourceX(
					reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS>(p1x1MipMem),
					&desc,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					nullptr,
					IID_GRAPHICS_PPV_ARGS(p1x1intermediateUAV))
			);

			// Create UAVs
			descUav.Texture2D.MipSlice = 0;
			m_device->CreateUnorderedAccessView(*p2x2intermediateUAV, nullptr, &descUav, m_intermediateUavsCPU[numIntermediateMips]);
			m_device->CreateUnorderedAccessView(*p1x1intermediateUAV, nullptr, &descUav, m_intermediateUavsCPU[numIntermediateMips + 1]);
		}
	}

	// Create our 'final' SRV (which isn't valid until all work completes)
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = bcFormat;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

	for (uint32_t i = 0; i < numMips; ++i)
	{
		srvDesc.Texture1D.MostDetailedMip = i;
		srvDesc.Texture2D.MipLevels = numMips - i;
		m_device->CreateShaderResourceView(*bcTexture, &srvDesc, m_bcTextureSrvsCPU[i]);
	}

	return S_OK;
}

_Use_decl_annotations_
HRESULT CompressorGPU::Compress(
    ID3D12GraphicsCommandList* commandList,
    uint32_t texSize,
    DXGI_FORMAT bcFormat,
    uint32_t mipLevels, const D3D12_GPU_DESCRIPTOR_HANDLE* inputTexture,
    bool generateMips)
{
    if (texSize < 16 || !mipLevels || mipLevels > D3D12_REQ_MIP_LEVELS)
        return E_INVALIDARG;

    ID3D12PipelineState* psoCompress = nullptr;
    ID3D12PipelineState* psoCompressTwoMips = nullptr;
    ID3D12PipelineState* psoCompressTailsMips = nullptr;
    bool compress2mips = true;
    switch (bcFormat)
    {
    case DXGI_FORMAT_BC1_UNORM:
        psoCompress = m_bc1Compress.Get();
        psoCompressTwoMips = m_bc1CompressTwoMips.Get();
        psoCompressTailsMips = m_bc1CompressTailMips.Get();
        break;

    case DXGI_FORMAT_BC3_UNORM:
        psoCompress = m_bc3Compress.Get();
        psoCompressTwoMips = m_bc3CompressTwoMips.Get();
        psoCompressTailsMips = m_bc3CompressTailMips.Get();
        compress2mips = false; // For BC3, using the "compress two mips" shader seems to decrease performance
        break;

    case DXGI_FORMAT_BC5_UNORM:
        psoCompress = m_bc5Compress.Get();
        psoCompressTwoMips = m_bc5CompressTwoMips.Get();
        psoCompressTailsMips = m_bc5CompressTailMips.Get();
        break;

    default:
        return E_INVALIDARG;
    }

    // We only support square, power-of-two textures. This makes life easier because every mip level 
    // (above 2x2) of a power-of-two texture will be divisible by 4. It wouldn't be much additional 
    // work to support more general textures, but you would need to properly handle reading off the 
    // edge when a mip level isn't divisible by 4.
    if (!ispow2(texSize))
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

    uint32_t numMips = 0;
    uint32_t numIntermediateMips = 0;
    if (generateMips)
    {
        numMips = mipLevels;
        numIntermediateMips = numMips - 2;
    }
    else
    {
        numMips = numIntermediateMips = 1;
    }

    bool twoMips = generateMips && compress2mips;

    commandList->SetComputeRootSignature(m_rootSig.Get());

	// We run several compute shader passes to generate all of our mips
	for (uint32_t i = 0; i < numMips; i += (twoMips ? 2 : 1))
	{
		uint32_t mipWidth = std::max(texSize >> i, 1u);

		ConstantBufferBC cb;
		cb.g_oneOverTextureWidth = 1.0f / mipWidth;
		auto cbs = DirectX::GraphicsMemory::Get().AllocateConstant(cb);
		commandList->SetComputeRootConstantBufferView(GPU_ConstantBuffer, cbs.GpuAddress());

		commandList->SetComputeRootDescriptorTable(GPU_TextureSRV, inputTexture[i]);

		// If we've reached the 16x16 mip, use our "tail mips" shader that compresses the remaining
		//  mips (from 16x16 to 1x1) in one pass
		if (mipWidth == 16)
		{
			commandList->SetComputeRootDescriptorTable(GPU_TextureUAV_0, m_intermediateUavsGPU[i]);
			commandList->SetComputeRootDescriptorTable(GPU_TextureUAV_1, m_intermediateUavsGPU[i + 1]);
			commandList->SetComputeRootDescriptorTable(GPU_TextureUAV_2, m_intermediateUavsGPU[i + 2]);
			commandList->SetComputeRootDescriptorTable(GPU_TextureUAV_3, m_intermediateUavsGPU[i + 3]);
			commandList->SetComputeRootDescriptorTable(GPU_TextureUAV_4, m_intermediateUavsGPU[i + 4]);
			commandList->SetPipelineState(psoCompressTailsMips);
			commandList->Dispatch(1, 1, 1);
			break;
		}

		// If we've been using the "compress two mips" shader, determine whether we've reached
		//  the threshold where we want to switch to the "compress one mip" shader
		if (twoMips)
		{
			if (mipWidth < COMPRESS_TWO_MIPS_SIZE_THRESHOLD)
			{
				twoMips = false;
			}
		}

		commandList->SetComputeRootDescriptorTable(GPU_TextureUAV_0, m_intermediateUavsGPU[i]);
		commandList->SetComputeRootDescriptorTable(GPU_TextureUAV_1, m_intermediateUavsGPU[i + 1]);
		commandList->SetPipelineState(twoMips ? psoCompressTwoMips : psoCompress);

		// Fire off the shader
		UINT dispatchWidth = mipWidth / 4 / (twoMips ? COMPRESS_TWO_MIPS_THREADGROUP_WIDTH : COMPRESS_ONE_MIP_THREADGROUP_WIDTH);
		dispatchWidth = std::max<UINT>(1, dispatchWidth);
		commandList->Dispatch(dispatchWidth, dispatchWidth, 1);
	}

    return S_OK;
}

void CompressorGPU::FreeMemory(_In_opt_ void *textureMem)
{
    if (textureMem)
    {
        XMemFree(textureMem, c_XMemAllocAttributes);
    }
}