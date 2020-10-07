//--------------------------------------------------------------------------------------
// FastBlockCompress.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "FastBlockCompress.h"

#include "ATGColors.h"
#include "ControllerFont.h"
#include "ReadData.h"

extern void ExitSample() noexcept;

using namespace DirectX;

using Microsoft::WRL::ComPtr;

namespace
{
    const uint32_t MAX_TEXTURE_WIDTH = 2048; // 2048x2048

    const uint32_t RMS_THREADGROUP_WIDTH = 64;

	enum RMSRootParameterIndex
	{
		RMS_ConstantBuffer,
		RMS_TextureSRV,
		RMS_TextureSRV_2,
		RMS_ReduceBufferA,
		RMS_ReduceBufferB,
		RMS_Count
	};

    __declspec(align(16)) struct ConstantBufferRMS
    {
        uint32_t textureWidth;
        uint32_t mipLevel;
        uint32_t reconstructZA;
        uint32_t reconstructZB;
    };

    static_assert((sizeof(ConstantBufferRMS) % 16) == 0, "CB size not padded correctly");

    struct ConstantBufferQuad
    {
        float oneOverZoom;
        float offsetX;
        float offsetY;
        float textureWidth;
        float textureHeight;
        uint32_t mipLevel;
        uint32_t highlightBlocks;
        uint32_t colorDiffs;
        uint32_t alphaDiffs;
        uint32_t reconstructZ;
        uint32_t pad[2];
    };

    static_assert((sizeof(ConstantBufferQuad) % 16) == 0, "CB size not padded correctly");

    const float c_zoomSpeed = 0.5f;
    const float c_panSpeed = 0.5f;

    inline XMFLOAT2 RMSComputeResult(ID3D12Resource* buffer, float width)
    {
        // Fetches RMS value from read-back buffer
        void* pMappedMemory = nullptr;
        D3D12_RANGE readRange = { 0, sizeof(XMFLOAT2) };
        DX::ThrowIfFailed(buffer->Map(0, &readRange, &pMappedMemory));

        auto res = reinterpret_cast<const XMFLOAT2*>(pMappedMemory);

        // Computes RMS
        XMFLOAT2 sqError;
        sqError.x = (1.0f / 3.0f) * sqrtf(res->x / (width * width));    // RGB
        sqError.y = sqrtf(res->y / (width * width));                    // Alpha

        D3D12_RANGE writeRange = { 0, 0 };
        buffer->Unmap(0, &writeRange);

        return sqError;
    }
}

Sample::Sample() noexcept(false) :
    m_frame(0),
    m_RMSError{},
    m_rmsFenceValue(0),
    m_rmsPending(false),
    m_fbcTextureMem(nullptr),
    m_fullscreen(false),
    m_toggleOriginal(false),
    m_highlightBlocks(false),
    m_colorDiffs(false),
    m_alphaDiffs(false),
    m_currentImage(0),
    m_currentMethod(0),
    m_mipLevel(0),
    m_zoom(1.f),
    m_offsetX(0),
    m_offsetY(0)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(IUnknown* window)
{
    m_gamePad = std::make_unique<GamePad>();

    m_deviceResources->SetWindow(window);

    m_deviceResources->CreateDeviceResources();  
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();
}

#pragma region Frame Update
// Executes basic render loop.
void Sample::Tick()
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Frame %llu", m_frame);

    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();

    PIXEndEvent();
    m_frame++;
}

// Updates the world.
void Sample::Update(DX::StepTimer const& timer)
{
	PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

	float elapsedTime = float(timer.GetElapsedSeconds());

	auto pad = m_gamePad->GetState(0);
	if (pad.IsConnected())
	{
		m_gamePadButtons.Update(pad);

		if (pad.IsViewPressed())
		{
			ExitSample();
		}

		using ButtonState = GamePad::ButtonStateTracker::ButtonState;

		if (m_gamePadButtons.x == ButtonState::PRESSED)
		{
			m_fullscreen = !m_fullscreen;
			m_toggleOriginal = false;
		}

		if (m_gamePadButtons.a == ButtonState::PRESSED)
		{
			m_highlightBlocks = !m_highlightBlocks;
		}

		if (m_gamePadButtons.y == ButtonState::PRESSED)
		{
			if (m_colorDiffs)
			{
				m_colorDiffs = false;
				m_alphaDiffs = true;
			}
			else if (m_alphaDiffs)
			{
				m_alphaDiffs = false;
			}
			else
			{
				m_colorDiffs = true;
			}
		}

		if (m_gamePadButtons.rightShoulder == ButtonState::PRESSED)
		{
			m_currentImage = (m_currentImage + 1) % m_images.size();
			m_currentMethod = 0;
			m_toggleOriginal = false;
		}
		else if (m_gamePadButtons.leftShoulder == ButtonState::PRESSED)
		{
			m_currentImage = (m_currentImage + m_images.size() - 1) % m_images.size();
			m_currentMethod = 0;
			m_toggleOriginal = false;
		}

		if (m_gamePadButtons.dpadRight == ButtonState::PRESSED)
		{
			if (m_fullscreen)
			{
				if (m_toggleOriginal)
				{
					m_toggleOriginal = false;
				}
				else if (m_currentMethod + 1 == MAX_METHOD)
				{
					m_toggleOriginal = true;
					m_currentMethod = 0;
				}
				else
				{
					m_currentMethod = m_currentMethod + 1;
				}
			}
			else
			{
				m_currentMethod = (m_currentMethod + 1) % MAX_METHOD;
			}
		}
		else if (m_gamePadButtons.dpadLeft == ButtonState::PRESSED)
		{
			if (m_fullscreen)
			{
				if (m_toggleOriginal)
				{
					m_toggleOriginal = false;
				}
				else if (m_currentMethod == 0)
				{
					m_toggleOriginal = true;
					m_currentMethod = MAX_METHOD - 1;
				}
				else
				{
					m_currentMethod = m_currentMethod - 1;
				}
			}
			else
			{
				m_currentMethod = (m_currentMethod + MAX_METHOD - 1) % MAX_METHOD;
			}
		}

		if (m_gamePadButtons.dpadUp == ButtonState::PRESSED)
		{
			auto img = m_images[m_currentImage].get();
			m_mipLevel = std::min(m_mipLevel + 1, static_cast<int>(img->m_srv.size()) - 1);
		}
		else if (m_gamePadButtons.dpadDown == ButtonState::PRESSED)
		{
			m_mipLevel = std::max(m_mipLevel - 1, 0);
		}

		// Update camera
		{
			// Change zoom
			m_zoom *= (1.f + elapsedTime * c_zoomSpeed * pad.triggers.right);
			m_zoom *= (1.f - elapsedTime * c_zoomSpeed * pad.triggers.left);
			m_zoom = std::max(m_zoom, 1.f);

			float oneOverZoom = 1.0f / m_zoom;

			// Change offset
			m_offsetX += elapsedTime * c_panSpeed * oneOverZoom * pad.thumbSticks.rightX;
			m_offsetX = std::max(m_offsetX, 0.f);
			m_offsetX = std::min(m_offsetX, 1.f - oneOverZoom);
			m_offsetY -= elapsedTime * c_panSpeed * oneOverZoom * pad.thumbSticks.rightY;
			m_offsetY = std::max(m_offsetY, 0.f);
			m_offsetY = std::min(m_offsetY, 1.f - oneOverZoom);
		}

		// Reset camera
		if (pad.IsRightStickPressed())
		{
			m_zoom = 1.f;
			m_offsetX = 0;
			m_offsetY = 0;
		}
	}
	else
	{
		m_gamePadButtons.Reset();
	}

	{
		// Make sure we're not trying to display a mip that is unavailable in the source image
		auto img = m_images[m_currentImage].get();
		m_mipLevel = std::min(m_mipLevel, static_cast<int>(img->m_srv.size()) - 1);
	}

	if (m_fbcTextureMem)
	{
		m_deviceResources->WaitForGpu();

		m_fbcTexture.Reset();

		CompressorGPU::FreeMemory(m_fbcTextureMem);
		m_fbcTextureMem = nullptr;
	}

	switch (m_currentMethod)
	{
	case RTC_GPU:
	{
		PIXBeginEvent(m_computeCommandList.Get(), PIX_COLOR(0, 255, 0), "GPU Compress");
		m_gpuTimer.BeginFrame(m_computeCommandList.Get());

		ID3D12DescriptorHeap* pHeaps[] = { m_resourceDescriptors->Heap() };
		m_computeCommandList->SetDescriptorHeaps(_countof(pHeaps), pHeaps);

		auto img = m_images[m_currentImage].get();

		D3D12_GPU_DESCRIPTOR_HANDLE sourceSRV[D3D12_REQ_MIP_LEVELS];
		for (size_t j = 0; j < img->m_desc.MipLevels && j < D3D12_REQ_MIP_LEVELS; ++j)
		{
			sourceSRV[j] = m_resourceDescriptors->GetGpuHandle(img->m_srv[j]);
		}

		// First compress the top mip alone (to calculate the compression time)
		ComPtr<ID3D12Resource> resource1Mip;
		ComPtr<ID3D12Resource> intermediateUAV1Mip[3];
		void* textureMem1Mip = nullptr;

		DX::ThrowIfFailed(
			m_compressorGPU.Prepare(
				static_cast<uint32_t>(img->m_desc.Width), img->m_compressedFormat,
				1, resource1Mip.GetAddressOf(), &textureMem1Mip,
				intermediateUAV1Mip[0].GetAddressOf(), intermediateUAV1Mip[1].GetAddressOf(), intermediateUAV1Mip[2].GetAddressOf(),
				false)
		);

		m_gpuTimer.Start(m_computeCommandList.Get());

		DX::ThrowIfFailed(
			m_compressorGPU.Compress(
				m_computeCommandList.Get(), static_cast<uint32_t>(img->m_desc.Width), img->m_compressedFormat,
				1, sourceSRV, false)
		);

		m_gpuTimer.Stop(m_computeCommandList.Get());

        m_computeCommandList->FlushPipelineX(D3D12XBOX_FLUSH_BOP_CS_PARTIAL, D3D12_GPU_VIRTUAL_ADDRESS_NULL, D3D12XBOX_FLUSH_RANGE_ALL);

		// Then compress all of the mips
		ComPtr<ID3D12Resource> intermediateUAV[3];

		DX::ThrowIfFailed(
			m_compressorGPU.Prepare(
				static_cast<uint32_t>(img->m_desc.Width), img->m_compressedFormat,
				img->m_desc.MipLevels, m_fbcTexture.GetAddressOf(), &m_fbcTextureMem,
				intermediateUAV[0].GetAddressOf(), intermediateUAV[1].GetAddressOf(), intermediateUAV[2].GetAddressOf(),
				true)
		);

		m_gpuTimer.Start(m_computeCommandList.Get(), 1);

		DX::ThrowIfFailed(
			m_compressorGPU.Compress(
				m_computeCommandList.Get(), static_cast<uint32_t>(img->m_desc.Width), img->m_compressedFormat,
				img->m_desc.MipLevels, sourceSRV, true)
		);

		m_gpuTimer.Stop(m_computeCommandList.Get(), 1);

		PIXEndEvent(m_computeCommandList.Get());

		// close and execute the command list
		m_gpuTimer.EndFrame(m_computeCommandList.Get());

		m_computeCommandList->Close();
		ID3D12CommandList *tempList = m_computeCommandList.Get();
		m_computeCommandQueue->ExecuteCommandLists(1, &tempList);

		const uint64_t fence = m_computeFenceValue++;
		m_computeCommandQueue->Signal(m_computeFence.Get(), fence);
		if (m_computeFence->GetCompletedValue() < fence) // block until async compute has completed using a fence
		{
			m_computeFence->SetEventOnCompletion(fence, m_computeFenceEvent.Get());
			WaitForSingleObject(m_computeFenceEvent.Get(), INFINITE);
		}

		resource1Mip.Reset();
		CompressorGPU::FreeMemory(textureMem1Mip);

		m_computeAllocator->Reset();
		m_computeCommandList->Reset(m_computeAllocator.Get(), nullptr);
	}
	break;

	case RTC_CPU:
	{
		auto img = m_images[m_currentImage].get();

		// First compress the top mip alone (to calculate the compression time)
		std::unique_ptr<uint8_t, aligned_deleter> result1mip;
		std::vector<D3D12_SUBRESOURCE_DATA> subresources1mip;

		DX::ThrowIfFailed(
			m_compressorCPU.Prepare(
				static_cast<uint32_t>(img->m_desc.Width), img->m_compressedFormat,
				1, result1mip, subresources1mip)
		);

		m_cpuTimer.Start(0);

		DX::ThrowIfFailed(
			m_compressorCPU.Compress(
				static_cast<uint32_t>(img->m_desc.Width), 1, img->m_rgbaSubresources.data(), img->m_compressedFormat,
				subresources1mip.data())
		);

		m_cpuTimer.Stop(0);

		// Then compress all of the mips
		std::unique_ptr<uint8_t, aligned_deleter> resultAll;
		std::vector<D3D12_SUBRESOURCE_DATA> subresourcesAll;

		DX::ThrowIfFailed(
			m_compressorCPU.Prepare(
				static_cast<uint32_t>(img->m_desc.Width), img->m_compressedFormat,
				img->m_desc.MipLevels, resultAll, subresourcesAll)
		);

		m_cpuTimer.Start(1);

		DX::ThrowIfFailed(
			m_compressorCPU.Compress(
				static_cast<uint32_t>(img->m_desc.Width), img->m_desc.MipLevels, img->m_rgbaSubresources.data(), img->m_compressedFormat,
				subresourcesAll.data())
		);

		m_cpuTimer.Stop(1);

		// Upload result to GPU for viewing
		auto device = m_deviceResources->GetD3DDevice();

		auto desc = CD3DX12_RESOURCE_DESC::Tex2D(img->m_compressedFormat, img->m_desc.Width, img->m_desc.Height, 1, img->m_desc.MipLevels);

		CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

		DX::ThrowIfFailed(
			device->CreateCommittedResource(
				&defaultHeapProperties,
				D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_GRAPHICS_PPV_ARGS(m_fbcTexture.ReleaseAndGetAddressOf()))
		);

		ResourceUploadBatch upload(device);
		upload.Begin();

		upload.Upload(
			m_fbcTexture.Get(),
			0,
			subresourcesAll.data(),
			static_cast<UINT>(subresourcesAll.size()));

		upload.Transition(
			m_fbcTexture.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		auto finish = upload.End(m_deviceResources->GetCommandQueue());
		finish.wait();

		for (uint16_t i = 0; i < img->m_desc.MipLevels; ++i)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = img->m_compressedFormat;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture1D.MostDetailedMip = i;
			srvDesc.Texture2D.MipLevels = img->m_desc.MipLevels - i;

			device->CreateShaderResourceView(m_fbcTexture.Get(), &srvDesc, m_resourceDescriptors->GetCpuHandle(CPU_MipBase + i));
		}
	}
	break;
	}

	m_cpuTimer.Update();
    PIXEndEvent();
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Sample::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    // Prepare the command list to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    auto size = m_deviceResources->GetOutputSize();

    auto safe = SimpleMath::Viewport::ComputeTitleSafeArea(size.right, size.bottom);

    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    // Set the descriptor heaps
    ID3D12DescriptorHeap* descriptorHeaps[] =
    {
        m_resourceDescriptors->Heap()
    };
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    // Draw images
    auto img = m_images[m_currentImage].get();

    size_t indexOriginalBase = img->m_srv[0];
    size_t indexOriginal = img->m_srv[m_mipLevel];

    size_t indexCompressBase = 0;
    size_t indexCompress = 0;

    const wchar_t* bcLabel = nullptr;
    switch (img->m_compressedFormat)
    {
    case DXGI_FORMAT_BC1_UNORM: bcLabel = L"BC1"; break;
    case DXGI_FORMAT_BC3_UNORM: bcLabel = L"BC3"; break;
    case DXGI_FORMAT_BC5_UNORM: bcLabel = L"BC5"; break;
    }

    wchar_t label[64] = {};
    switch (m_currentMethod)
    {
    case RTC_GPU:
		indexCompressBase = m_srvFBC[0];
		indexCompress = m_srvFBC[m_mipLevel];
		swprintf_s(label, L"GPU (%ls)", bcLabel);
		break;

	case RTC_CPU:
		indexCompressBase = CPU_MipBase;
		indexCompress = CPU_MipBase + m_mipLevel;
		swprintf_s(label, L"CPU (%ls)", bcLabel);
		break;

    case OFFLINE:
        indexCompressBase = img->m_srvBC[0];
        indexCompress = img->m_srvBC[m_mipLevel];
        swprintf_s(label, L"Offline (%ls)", bcLabel);
        break;

    case OFFLINE_BC7:
        indexCompressBase = img->m_srvBC7[0];
        indexCompress = img->m_srvBC7[m_mipLevel];
        swprintf_s(label, L"Offline (BC7)");
        break;
    }

    // Compute RMS
    bool rmsSubmitted = false;
    if (!m_fullscreen || !m_toggleOriginal)
    {
        rmsSubmitted = RMSError(commandList, indexOriginalBase, indexCompressBase, m_mipLevel, uint32_t(img->m_desc.Width), false, (img->m_compressedFormat == DXGI_FORMAT_BC5_UNORM));
    }

    // View images
    ConstantBufferQuad cameraSettings = {};
    cameraSettings.oneOverZoom = 1.f / m_zoom;
    cameraSettings.offsetX = m_offsetX;
    cameraSettings.offsetY = m_offsetY;
    cameraSettings.textureWidth = cameraSettings.textureHeight = (static_cast<float>(img->m_desc.Width) / std::powf(2.f, static_cast<float>(m_mipLevel)));
    cameraSettings.mipLevel = m_mipLevel;
    cameraSettings.highlightBlocks = m_highlightBlocks;
    cameraSettings.reconstructZ = (img->m_compressedFormat == DXGI_FORMAT_BC5_UNORM) != 0;

    auto cbQuadOrig = m_graphicsMemory->AllocateConstant(cameraSettings);

    cameraSettings.colorDiffs = m_colorDiffs;
    cameraSettings.alphaDiffs = m_alphaDiffs;
    auto cbQuad = m_graphicsMemory->AllocateConstant(cameraSettings);

    if (m_fullscreen)
    {
        D3D12_VIEWPORT viewPort;
        viewPort.TopLeftX = 448;
        viewPort.TopLeftY = 28;
        viewPort.Width = 1024;
        viewPort.Height = 1024;
        viewPort.MinDepth = 0;
        viewPort.MaxDepth = 1;
        commandList->RSSetViewports(1, &viewPort);

        RECT rct = { long(viewPort.TopLeftX), long(viewPort.TopLeftY), long(viewPort.TopLeftX + viewPort.Width), long(viewPort.TopLeftY + viewPort.Height) };
        commandList->RSSetScissorRects(1, &rct);

        if (m_toggleOriginal)
        {
            m_fullScreenQuad->Draw(commandList, m_quadPSO.Get(), m_resourceDescriptors->GetGpuHandle(indexOriginal), cbQuadOrig.GpuAddress());
        }
        else
        {
            m_fullScreenQuad->Draw(commandList, m_quadPSO.Get(), m_resourceDescriptors->GetGpuHandle(indexCompress), m_resourceDescriptors->GetGpuHandle(indexOriginal), cbQuad.GpuAddress());
        }
    }
    else
    {
        // Draw the original image on the left
        D3D12_VIEWPORT viewPort;
        viewPort.TopLeftX = 96;
        viewPort.TopLeftY = 216;
        viewPort.Width = 810;
        viewPort.Height = 810;
        viewPort.MinDepth = 0;
        viewPort.MaxDepth = 1;
        commandList->RSSetViewports(1, &viewPort);

        RECT rct = { long(viewPort.TopLeftX), long(viewPort.TopLeftY), long(viewPort.TopLeftX + viewPort.Width), long(viewPort.TopLeftY + viewPort.Height) };
        commandList->RSSetScissorRects(1, &rct);

        m_fullScreenQuad->Draw(commandList, m_quadPSO.Get(), m_resourceDescriptors->GetGpuHandle(indexOriginal), cbQuadOrig.GpuAddress());

        // Draw the compressed image on the righ
        viewPort.TopLeftX = 1014;
        commandList->RSSetViewports(1, &viewPort);

        rct = { long(viewPort.TopLeftX), long(viewPort.TopLeftY), long(viewPort.TopLeftX + viewPort.Width), long(viewPort.TopLeftY + viewPort.Height) };
        commandList->RSSetScissorRects(1, &rct);

        m_fullScreenQuad->Draw(commandList, m_quadPSO.Get(), m_resourceDescriptors->GetGpuHandle(indexCompress), m_resourceDescriptors->GetGpuHandle(indexOriginal), cbQuad.GpuAddress());
    }

    // Draw UI
    {
        auto vp = m_deviceResources->GetScreenViewport();
        commandList->RSSetViewports(1, &vp);

        auto rct = m_deviceResources->GetScissorRect();
        commandList->RSSetScissorRects(1, &rct);
    }

    XMFLOAT2 pos(96.f, float(safe.top));

    float ysize = m_font->GetLineSpacing();

    m_batch->Begin(commandList);

    wchar_t buff[128] = {};
    swprintf_s(buff, L"[A] Highlight blocks %ls", m_highlightBlocks ? L"On" : L"Off");
    DX::DrawControllerString(m_batch.get(), m_font.get(), m_colorCtrlFont.get(), buff, pos, ATG::Colors::LightGrey);
    pos.y += ysize;

    swprintf_s(buff, L"[X] %ls", m_fullscreen ? L"Side-by-side" : L"Fullscreen");
    DX::DrawControllerString(m_batch.get(), m_font.get(), m_colorCtrlFont.get(), buff, pos, ATG::Colors::LightGrey);
    pos.y += ysize;

    if (m_colorDiffs)
    {
        swprintf_s(buff, L"[Y] Diffs: Color (10x scale)");
    }
    else if (m_alphaDiffs)
    {
        swprintf_s(buff, L"[Y] Diffs: Alpha (10x scale)");
    }
    else
    {
        swprintf_s(buff, L"[Y] Diffs: None");
    }

    DX::DrawControllerString(m_batch.get(), m_font.get(), m_colorCtrlFont.get(), buff, pos, ATG::Colors::LightGrey);
    pos.y += ysize;

    pos.x = 1490.f;
    pos.y = float(safe.top);
    swprintf_s(buff, L"[DPad] Up/Down Mip Level %u", m_mipLevel);
    DX::DrawControllerString(m_batch.get(), m_font.get(), m_colorCtrlFont.get(), buff, pos, ATG::Colors::LightGrey);
    pos.y += ysize;

    swprintf_s(buff, L"Texture dimensions: %llu x %u", img->m_desc.Width, img->m_desc.Height);
    m_font->DrawString(m_batch.get(), buff, pos, ATG::Colors::LightGrey);
    pos.y += ysize;

    if (!m_fullscreen || !m_toggleOriginal)
    {
        swprintf_s(buff, L"RGB RMS Error (Mip %u): %f", m_mipLevel, m_RMSError.x);
        m_font->DrawString(m_batch.get(), buff, pos, ATG::Colors::LightGrey);
        pos.y += ysize;

        swprintf_s(buff, L"Alpha RMS Error (Mip %u): %f", m_mipLevel, m_RMSError.y);
        m_font->DrawString(m_batch.get(), buff, pos, ATG::Colors::LightGrey);
        pos.y += ysize;

		switch (m_currentMethod)
		{
		case RTC_GPU:
			swprintf_s(buff, L"Time (Top) %1.3f ms; (All) %1.3f ms", m_gpuTimer.GetElapsedMS(0), m_gpuTimer.GetElapsedMS(1));
			m_font->DrawString(m_batch.get(), buff, pos, ATG::Colors::LightGrey);
			pos.y += ysize;
			break;

		case RTC_CPU:
#ifndef _DEBUG
			swprintf_s(buff, L"Time (Top) %1.3f ms; (All) %1.3f ms", m_cpuTimer.GetElapsedMS(0), m_cpuTimer.GetElapsedMS(1));
			m_font->DrawString(m_batch.get(), buff, pos, ATG::Colors::LightGrey);
			pos.y += ysize;
#endif
			break;
		}
    }

    if (m_fullscreen)
    {
        m_font->DrawString(m_batch.get(), m_toggleOriginal ? L"Original Image" : label, XMFLOAT2(448.f, 28.f - ysize), ATG::Colors::Green);
    }
   else
    {
       m_font->DrawString(m_batch.get(), L"Original Image", XMFLOAT2(96.f, 216.f - ysize), ATG::Colors::Green);

       m_font->DrawString(m_batch.get(), label, XMFLOAT2(1014.f, 216.f - ysize), ATG::Colors::Green);
   }
 
    DX::DrawControllerString(m_batch.get(), m_font.get(), m_ctrlFont.get(),
        L"[View] Exit   [LB] / [RB] Image select ",
        XMFLOAT2(float(safe.left), float(safe.bottom)), ATG::Colors::LightGrey);

    m_batch->End();

    PIXEndEvent(commandList);

    // Show the new frame.
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();

    auto queue = m_deviceResources->GetCommandQueue();

    if (rmsSubmitted)
    {
        const uint64_t fence = m_rmsFenceValue++;
        queue->Signal(m_rmsFence.Get(), fence);
        if (m_rmsFence->GetCompletedValue() < fence)
        {
            m_rmsPending = true;
            m_rmsFence->SetEventOnCompletion(fence, m_rmsFenceEvent.Get());
        }
        else
        {
            // Results ready immediately
            m_RMSError = RMSComputeResult(m_rmsResult.Get(), float(m_rmsWidth));
            m_rmsCB.Reset();
        }
    }

    m_graphicsMemory->Commit(queue);

    PIXEndEvent();
}

// Helper method to clear the back buffers.
void Sample::Clear()
{
    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.
    auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
    auto dsvDescriptor = m_deviceResources->GetDepthStencilView();

    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
    commandList->ClearRenderTargetView(rtvDescriptor, ATG::Colors::Background, 0, nullptr);
    commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Set the viewport and scissor rect.
    auto viewport = m_deviceResources->GetScreenViewport();
    auto scissorRect = m_deviceResources->GetScissorRect();
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);

    PIXEndEvent(commandList);
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Sample::OnSuspending()
{
    auto queue = m_deviceResources->GetCommandQueue();
    queue->SuspendX(0);
}

void Sample::OnResuming()
{
    auto queue = m_deviceResources->GetCommandQueue();
    queue->ResumeX();
    m_timer.ResetElapsedTime();
    m_gamePadButtons.Reset();
	m_cpuTimer.Reset();
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Sample::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device);

    RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());
    rtState;

    m_resourceDescriptors = std::make_unique<DescriptorPile>(device,
        256,
        Descriptors::Count);

    // create compute fence and event
    m_computeFenceEvent.Attach(CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS));
    if (!m_computeFenceEvent.IsValid())
    {
        throw std::exception("CreateEvent");
    }

    DX::ThrowIfFailed(
        device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_GRAPHICS_PPV_ARGS(m_computeFence.ReleaseAndGetAddressOf())));
    m_computeFence->SetName(L"Compute");
    m_computeFenceValue = 1;

    // Image rendering
    m_fullScreenQuad = std::make_unique<DX::FullScreenQuad>();
    m_fullScreenQuad->Initialize(device);

    {
        auto pixelShaderBlob = DX::ReadData(L"QuadWithCamera.cso");
        auto vertexShaderBlob = DX::ReadData(L"FullScreenQuadVS.cso");

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = m_fullScreenQuad->GetRootSignature();
        psoDesc.VS = { vertexShaderBlob.data(), vertexShaderBlob.size() };
        psoDesc.PS = { pixelShaderBlob.data(), pixelShaderBlob.size() };
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.DSVFormat = m_deviceResources->GetDepthBufferFormat();
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = m_deviceResources->GetBackBufferFormat();
        psoDesc.SampleDesc.Count = 1;

        DX::ThrowIfFailed(
            device->CreateGraphicsPipelineState(&psoDesc,
                IID_GRAPHICS_PPV_ARGS(m_quadPSO.ReleaseAndGetAddressOf())));
    }

    // RMS computation
    {
        auto blob = DX::ReadData(L"RMSError.cso");

        // Xbox One best practice is to use HLSL-based root signatures to support shader precompilation.

        DX::ThrowIfFailed(
            device->CreateRootSignature(0, blob.data(), blob.size(),
                IID_GRAPHICS_PPV_ARGS(m_rmsRootSig.ReleaseAndGetAddressOf())));

        m_rmsRootSig->SetName(L"RMS RS");

        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = m_rmsRootSig.Get();

        psoDesc.CS.pShaderBytecode = blob.data();
        psoDesc.CS.BytecodeLength = blob.size();

        DX::ThrowIfFailed(
            device->CreateComputePipelineState(&psoDesc, IID_GRAPHICS_PPV_ARGS(m_rmsErrorPSO.ReleaseAndGetAddressOf())));

        m_rmsErrorPSO->SetName(L"RMS Error PSO");

        blob = DX::ReadData(L"RMSReduce.cso");
        psoDesc.CS.pShaderBytecode = blob.data();
        psoDesc.CS.BytecodeLength = blob.size();

        DX::ThrowIfFailed(
            device->CreateComputePipelineState(&psoDesc, IID_GRAPHICS_PPV_ARGS(m_rmsReducePSO.ReleaseAndGetAddressOf())));

        m_rmsReducePSO->SetName(L"RMS Reduce PSO");
    }

    m_rmsFenceEvent.Attach(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
    if (!m_rmsFenceEvent.IsValid())
    {
        throw std::exception("CreateEvent");
    }

    DX::ThrowIfFailed(
        device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_GRAPHICS_PPV_ARGS(m_rmsFence.ReleaseAndGetAddressOf())));
    m_rmsFence->SetName(L"RSM Fence");
    m_rmsFenceValue = 1;

    {
        UINT64 numElements = (MAX_TEXTURE_WIDTH * MAX_TEXTURE_WIDTH) / 4;
        auto rmsDesc = CD3DX12_RESOURCE_DESC::Buffer(numElements * sizeof(XMFLOAT2), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

        DX::ThrowIfFailed(
            device->CreateCommittedResource(
                &defaultHeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &rmsDesc,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                nullptr,
                IID_GRAPHICS_PPV_ARGS(m_rmsReduceBufferA.ReleaseAndGetAddressOf())));

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.NumElements = static_cast<UINT>(numElements);
        uavDesc.Buffer.StructureByteStride = sizeof(XMFLOAT2);

        device->CreateUnorderedAccessView(m_rmsReduceBufferA.Get(), nullptr, &uavDesc, m_resourceDescriptors->GetCpuHandle(RMS_Reduce_A_UAV));

        numElements = std::max<UINT64>(numElements / 4, 1);
        rmsDesc.Width = numElements * sizeof(XMFLOAT2);
        uavDesc.Buffer.NumElements = static_cast<UINT>(numElements);

        DX::ThrowIfFailed(
            device->CreateCommittedResource(
                &defaultHeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &rmsDesc,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                nullptr,
                IID_GRAPHICS_PPV_ARGS(m_rmsReduceBufferB.ReleaseAndGetAddressOf())));

        device->CreateUnorderedAccessView(m_rmsReduceBufferB.Get(), nullptr, &uavDesc, m_resourceDescriptors->GetCpuHandle(RMS_Reduce_B_UAV));
    }

    {
        auto readBackDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(XMFLOAT2));

        CD3DX12_HEAP_PROPERTIES readBackHeapProperties(D3D12_HEAP_TYPE_READBACK);

        DX::ThrowIfFailed(
            device->CreateCommittedResource(
                &readBackHeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &readBackDesc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_GRAPHICS_PPV_ARGS(m_rmsResult.ReleaseAndGetAddressOf())));
    }

    {
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> cpuHandlesUAV;
        std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> gpuHandlesUAV;
        cpuHandlesUAV.reserve(D3D12_REQ_MIP_LEVELS);
        gpuHandlesUAV.reserve(D3D12_REQ_MIP_LEVELS);

        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> cpuHandlesSRV;
        cpuHandlesSRV.reserve(D3D12_REQ_MIP_LEVELS);

		m_srvFBC.clear();
        for (uint32_t j = 0; j < D3D12_REQ_MIP_LEVELS; ++j)
        {
            size_t idx = m_resourceDescriptors->Allocate();
            cpuHandlesUAV.emplace_back(m_resourceDescriptors->GetCpuHandle(idx));
            gpuHandlesUAV.emplace_back(m_resourceDescriptors->GetGpuHandle(idx));

            idx = m_resourceDescriptors->Allocate();
            cpuHandlesSRV.emplace_back(m_resourceDescriptors->GetCpuHandle(idx));
			m_srvFBC.push_back(idx);
        }

        assert(cpuHandlesUAV.size() == D3D12_REQ_MIP_LEVELS);
        assert(gpuHandlesUAV.size() == D3D12_REQ_MIP_LEVELS);

        assert(cpuHandlesSRV.size() == D3D12_REQ_MIP_LEVELS);

		m_compressorGPU.Initialize(device, cpuHandlesUAV.data(), gpuHandlesUAV.data(), cpuHandlesSRV.data());
    }

    // Upload resources
    ResourceUploadBatch upload(device);
    upload.Begin();

    {
        SpriteBatchPipelineStateDescription pd(
            rtState,
            &CommonStates::AlphaBlend);
        m_batch = std::make_unique<SpriteBatch>(device, upload, pd);
    }

    m_font = std::make_unique<SpriteFont>(device, upload,
        L"SegoeUI_18.spritefont",
        m_resourceDescriptors->GetCpuHandle(Descriptors::Font),
        m_resourceDescriptors->GetGpuHandle(Descriptors::Font));

    m_ctrlFont = std::make_unique<SpriteFont>(device, upload,
        L"XboxOneControllerLegendSmall.spritefont",
        m_resourceDescriptors->GetCpuHandle(Descriptors::CtrlFont),
        m_resourceDescriptors->GetGpuHandle(Descriptors::CtrlFont));

    m_colorCtrlFont = std::make_unique<SpriteFont>(device, upload,
        L"XboxOneControllerSmall.spritefont",
        m_resourceDescriptors->GetCpuHandle(Descriptors::ColorCtrlFont),
        m_resourceDescriptors->GetGpuHandle(Descriptors::ColorCtrlFont));

    // opaque_1024.dds: 1024x1024 RGB: photo of recycle bin and book
    {
        auto image = std::make_unique<Image>(device, upload, m_resourceDescriptors.get(), L"Assets\\opaque_1024.dds", DXGI_FORMAT_BC1_UNORM);
        m_images.emplace_back(std::move(image));
    }

    // opaque_512.dds: 512x512 RGB: photo of Microsoft building interior
    {
        auto image = std::make_unique<Image>(device, upload, m_resourceDescriptors.get(), L"Assets\\opaque_512.dds", DXGI_FORMAT_BC1_UNORM);
        m_images.emplace_back(std::move(image));
    }

    // alpha.dds: 1024x1024 RGBA: photo of recycle bin and book with alpha
    {
        auto image = std::make_unique<Image>(device, upload, m_resourceDescriptors.get(), L"Assets\\alpha.dds", DXGI_FORMAT_BC3_UNORM);
        m_images.emplace_back(std::move(image));
    }

    // normal.dds: 1024x1024 RG: normal map texture
    {
        auto image = std::make_unique<Image>(device, upload, m_resourceDescriptors.get(), L"Assets\\normal.dds", DXGI_FORMAT_BC5_UNORM);
        m_images.emplace_back(std::move(image));
    }

    auto finish = upload.End(m_deviceResources->GetCommandQueue());

    // Create compute allocator, command queue and command list
    {
        D3D12_COMMAND_QUEUE_DESC descCommandQueue = { D3D12_COMMAND_LIST_TYPE_COMPUTE, 0, D3D12_COMMAND_QUEUE_FLAG_NONE };

        DX::ThrowIfFailed(
            device->CreateCommandQueue(&descCommandQueue, IID_GRAPHICS_PPV_ARGS(m_computeCommandQueue.ReleaseAndGetAddressOf())));

        DX::ThrowIfFailed(
            device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_GRAPHICS_PPV_ARGS(m_computeAllocator.ReleaseAndGetAddressOf())));

        DX::ThrowIfFailed(
            device->CreateCommandList(
                0,
                D3D12_COMMAND_LIST_TYPE_COMPUTE,
                m_computeAllocator.Get(),
                nullptr,
                IID_GRAPHICS_PPV_ARGS(m_computeCommandList.ReleaseAndGetAddressOf())));
    }

	m_gpuTimer.RestoreDevice(device, m_computeCommandQueue.Get());

	finish.wait();
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    m_batch->SetViewport(m_deviceResources->GetScreenViewport());
}
#pragma endregion

bool Sample::RMSError(ID3D12GraphicsCommandList* commandList,
    size_t originalTexture, size_t compressedTexture,
    uint32_t mipLevel, uint32_t width,
    bool reconstructZSource, bool reconstructZCompressed)
{
    if (m_rmsPending)
    {
        // RMS error computation in-flight; just check for result this frame.
        if (WaitForSingleObject(m_rmsFenceEvent.Get(), 0) == WAIT_OBJECT_0)
        {
            m_RMSError = RMSComputeResult(m_rmsResult.Get(), float(m_rmsWidth));
            m_rmsCB.Reset();
            m_rmsPending = false;
        }
        return false;
    }

    // We pass in the width of the full texture, but we need the width of the current mip level
    width >>= mipLevel;
    m_rmsWidth = width;

    ConstantBufferRMS cbs = { width, mipLevel, reconstructZSource ? 1u : 0u, reconstructZCompressed ? 1u : 0u };
    m_rmsCB = m_graphicsMemory->AllocateConstant(cbs);

    D3D12_GPU_DESCRIPTOR_HANDLE reduceA = m_resourceDescriptors->GetGpuHandle(RMS_Reduce_A_UAV);
    D3D12_GPU_DESCRIPTOR_HANDLE reduceB = m_resourceDescriptors->GetGpuHandle(RMS_Reduce_B_UAV);

    commandList->SetComputeRootSignature(m_rmsRootSig.Get());
    commandList->SetComputeRootConstantBufferView(RMS_ConstantBuffer, m_rmsCB.GpuAddress());
    commandList->SetComputeRootDescriptorTable(RMS_TextureSRV, m_resourceDescriptors->GetGpuHandle(originalTexture));
    commandList->SetComputeRootDescriptorTable(RMS_TextureSRV_2, m_resourceDescriptors->GetGpuHandle(compressedTexture));
    commandList->SetComputeRootDescriptorTable(RMS_ReduceBufferA, reduceA);
    commandList->SetComputeRootDescriptorTable(RMS_ReduceBufferB, reduceB);
    commandList->SetPipelineState(m_rmsErrorPSO.Get());

    UINT threadGroupCountX = std::max<UINT>(1, width / 2 / RMS_THREADGROUP_WIDTH);
    commandList->Dispatch(threadGroupCountX, width /* We only support square textures */, 1);

    // Reduce
    commandList->SetPipelineState(m_rmsReducePSO.Get());

    UINT numReduceBufferElements = std::max<UINT>(2, width * width / 4);
    while (numReduceBufferElements > 1)
    {
        D3D12_RESOURCE_BARRIER csbarrier = {};
        csbarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        csbarrier.UAV.pResource = (reduceA.ptr == m_resourceDescriptors->GetGpuHandle(RMS_Reduce_A_UAV).ptr) ? m_rmsReduceBufferA.Get() : m_rmsReduceBufferB.Get();
        commandList->ResourceBarrier(1, &csbarrier);

        commandList->SetComputeRootDescriptorTable(RMS_ReduceBufferA, reduceA);
        commandList->SetComputeRootDescriptorTable(RMS_ReduceBufferB, reduceB);

        threadGroupCountX = std::max<UINT>(1, numReduceBufferElements / 2 / RMS_THREADGROUP_WIDTH);
        commandList->Dispatch(threadGroupCountX, 1, 1);

        numReduceBufferElements = std::max<UINT>(1, numReduceBufferElements / 4);
        std::swap(reduceA, reduceB);
    }

    // Readback
    ID3D12Resource* currentReduce = (reduceA.ptr == m_resourceDescriptors->GetGpuHandle(RMS_Reduce_A_UAV).ptr) ? m_rmsReduceBufferA.Get() : m_rmsReduceBufferB.Get();

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = currentReduce;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

    commandList->ResourceBarrier(1, &barrier);

    commandList->CopyBufferRegion(m_rmsResult.Get(), 0, currentReduce, 0, sizeof(XMFLOAT2));

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    commandList->ResourceBarrier(1, &barrier);

    return true;
}

Image::Image(ID3D12Device* device, ResourceUploadBatch& batch, DescriptorPile* resourceDescriptors, const wchar_t* filename, DXGI_FORMAT format) :
	m_compressedFormat(format)
{
	// Load raw data for both CPU and GPU compression
	std::unique_ptr<uint8_t[]> ddsSource;
	std::vector<D3D12_SUBRESOURCE_DATA> sourceData;
	DX::ThrowIfFailed(
		LoadDDSTextureFromFile(device, filename, m_sourceTexture.GetAddressOf(), ddsSource, sourceData)
	);

	// Upload for use by GPU compressor
	batch.Upload(
		m_sourceTexture.Get(),
		0,
		sourceData.data(),
		static_cast<UINT>(sourceData.size()));

	batch.Transition(
		m_sourceTexture.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	m_desc = m_sourceTexture->GetDesc();

	// The sample only supports square, power-of-two textures larger than 16x16
	assert(m_desc.Width == m_desc.Height);
	assert((m_desc.Width != 0) && !(m_desc.Width & (m_desc.Width - 1)));
	assert(m_desc.Width > 16);

	// Need RGBA memory copy for CPU compressor
	switch (m_desc.Format)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
		break;

	default:
		throw std::exception("Unsupported source format");
	}

	if (m_desc.MipLevels != sourceData.size())
	{
		throw std::exception("Not enough source data");
	}
	
	MakeRGBATexture(static_cast<uint32_t>(m_desc.Width), sourceData.size(), sourceData.data(),
		m_desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM, m_desc.Format == DXGI_FORMAT_B8G8R8X8_UNORM);

	ddsSource.reset();
	sourceData.clear();

    for (uint16_t i = 0; i < m_desc.MipLevels; ++i)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = m_desc.Format;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture1D.MostDetailedMip = i;
        srvDesc.Texture2D.MipLevels = m_desc.MipLevels - i;

        size_t index = resourceDescriptors->Allocate();

        device->CreateShaderResourceView(m_sourceTexture.Get(), &srvDesc, resourceDescriptors->GetCpuHandle(index));

        m_srv.push_back(index);
    }

    // Offline Compressed
    {
        wchar_t drive[_MAX_DRIVE] = {};
        wchar_t dir[_MAX_DIR] = {};
        wchar_t ext[_MAX_EXT] = {};
        wchar_t fname[_MAX_FNAME] = {};
        _wsplitpath_s(filename, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, ext, _MAX_EXT);

        wcscat_s(fname, L"_offline");

        switch (format)
        {
        case DXGI_FORMAT_BC1_UNORM: wcscat_s(fname, L"_bc1"); break;
        case DXGI_FORMAT_BC3_UNORM: wcscat_s(fname, L"_bc3"); break;
        case DXGI_FORMAT_BC5_UNORM: wcscat_s(fname, L"_bc5"); break;
        }

        wchar_t newFilename[_MAX_PATH] = {};
        _wmakepath_s(newFilename, drive, dir, fname, ext);

        DX::ThrowIfFailed(
            CreateDDSTextureFromFile(device, batch, newFilename, m_bcTexture.GetAddressOf())
        );

        auto desc = m_bcTexture->GetDesc();

        assert(desc.Format == format);
        assert(desc.Width == m_desc.Width);
        assert(desc.Height == m_desc.Height);
        assert(desc.MipLevels == m_desc.MipLevels);

        for (uint16_t i = 0; i < m_desc.MipLevels; ++i)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = desc.Format;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture1D.MostDetailedMip = i;
            srvDesc.Texture2D.MipLevels = desc.MipLevels - i;

            size_t index = resourceDescriptors->Allocate();

            device->CreateShaderResourceView(m_bcTexture.Get(), &srvDesc, resourceDescriptors->GetCpuHandle(index));

            m_srvBC.push_back(index);
        }
    }

    // Offline BC7 Compressed
    {
        wchar_t drive[_MAX_DRIVE] = {};
        wchar_t dir[_MAX_DIR] = {};
        wchar_t ext[_MAX_EXT] = {};
        wchar_t fname[_MAX_FNAME] = {};
        _wsplitpath_s(filename, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, ext, _MAX_EXT);

        wcscat_s(fname, L"_offline_bc7");

        wchar_t newFilename[_MAX_PATH] = {};
        _wmakepath_s(newFilename, drive, dir, fname, ext);

        DX::ThrowIfFailed(
            CreateDDSTextureFromFile(device, batch, newFilename, m_bc7Texture.GetAddressOf())
        );

        auto desc = m_bc7Texture->GetDesc();

        assert(desc.Format == DXGI_FORMAT_BC7_UNORM);
        assert(desc.Width == m_desc.Width);
        assert(desc.Height == m_desc.Height);
        assert(desc.MipLevels == m_desc.MipLevels);

        for (uint16_t i = 0; i < desc.MipLevels; ++i)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = DXGI_FORMAT_BC7_UNORM;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture1D.MostDetailedMip = i;
            srvDesc.Texture2D.MipLevels = desc.MipLevels - i;

            size_t index = resourceDescriptors->Allocate();

            device->CreateShaderResourceView(m_bc7Texture.Get(), &srvDesc, resourceDescriptors->GetCpuHandle(index));

            m_srvBC7.push_back(index);
        }
    }
}

namespace
{
	inline uint32_t Swizzle(uint32_t t, bool ignorealpha)
	{
		uint32_t t1 = (t & 0x00ff0000) >> 16;
		uint32_t t2 = (t & 0x000000ff) << 16;
		uint32_t t3 = (t & 0x0000ff00);
		uint32_t ta = ignorealpha ? 0xff000000 : (t & 0xFF000000);

		return (t1 | t2 | t3 | ta);
	}
}

void Image::MakeRGBATexture(uint32_t texSize, size_t levels, const D3D12_SUBRESOURCE_DATA* subresources, bool swizzle, bool ignorealpha)
{
	// CPU codec requires input data to be in 16-byte aligned memory with DXGI_FORMAT_R8G8B8A8_UNORM format with a 'natural' pitch
	//
	// Each miplevel must start on a 16-byte boundary
	//
	// 2x2 and 1x1 need to be 4x4 size with replication

	size_t totalSize = 0;

	for (uint32_t level = 0; level < levels; ++level)
	{
		uint32_t mipSize = std::max(texSize >> level, 4u);

		totalSize += AlignUp(mipSize * mipSize * 4, 16);
	}

	auto mem = reinterpret_cast<uint8_t*>(_aligned_malloc(totalSize, 16));
	if (!mem)
		throw std::bad_alloc();

	m_rgbaTexture.reset(mem);
	m_rgbaSubresources.clear();

	uint8_t* destPtr = mem;

	for (uint32_t level = 0; level < levels; ++level)
	{
		uint32_t mipSize = std::max(texSize >> level, 1u);
		uint32_t targetMipSize = std::max(texSize >> level, 4u);

		D3D12_SUBRESOURCE_DATA rgba;
		rgba.pData = destPtr;
		rgba.RowPitch = targetMipSize * 4;
		rgba.SlicePitch = targetMipSize * targetMipSize * 4;
		m_rgbaSubresources.emplace_back(rgba);

		auto rowPitch = static_cast<uint32_t>(subresources[level].RowPitch);

		auto *srcPtr = static_cast<const uint8_t*>(subresources[level].pData);

		if (mipSize >= 4)
		{
			for (uint32_t y = 0; y < mipSize; ++y)
			{
				if (swizzle)
				{
					// BGR <-> RGB
					auto sPtr = reinterpret_cast<const uint32_t*>(srcPtr);
					auto dPtr = reinterpret_cast<uint32_t*>(destPtr);
					for (uint32_t x = 0; x < mipSize; ++x)
					{
						*(dPtr++) = Swizzle(*(sPtr++), ignorealpha);
					}
				}
				else
				{
					memcpy_s(destPtr, mipSize * 4, srcPtr, rowPitch);
				}

				srcPtr += rowPitch;
				destPtr += mipSize * 4;
			}
		}
		else if (mipSize >= 2)
		{
			// 2x2 replicate pattern (originally used by D3DX)

			// 0 1 0 1
			// 2 3 2 3
			// 0 1 0 1
			// 2 3 2 3

			auto sPtr0 = reinterpret_cast<const uint32_t*>(srcPtr);
			auto sPtr1 = reinterpret_cast<const uint32_t*>(srcPtr + 4);
			auto sPtr2 = reinterpret_cast<const uint32_t*>(srcPtr + rowPitch);
			auto sPtr3 = reinterpret_cast<const uint32_t*>(srcPtr + rowPitch + 4);

			uint32_t p0, p1, p2, p3;
			if (swizzle)
			{
				p0 = Swizzle(*sPtr0, ignorealpha);
				p1 = Swizzle(*sPtr1, ignorealpha);
				p2 = Swizzle(*sPtr2, ignorealpha);
				p3 = Swizzle(*sPtr3, ignorealpha);
			}
			else
			{
				p0 = *sPtr0;
				p1 = *sPtr1;
				p2 = *sPtr2;
				p3 = *sPtr3;
			}

			memcpy(&destPtr[0], &p0, sizeof(uint32_t));
			memcpy(&destPtr[4], &p1, sizeof(uint32_t));
			memcpy(&destPtr[8], &p0, sizeof(uint32_t));
			memcpy(&destPtr[12], &p1, sizeof(uint32_t));
			destPtr += 4 * 4;

			memcpy(&destPtr[0], &p2, sizeof(uint32_t));
			memcpy(&destPtr[4], &p3, sizeof(uint32_t));
			memcpy(&destPtr[8], &p2, sizeof(uint32_t));
			memcpy(&destPtr[12], &p3, sizeof(uint32_t));
			destPtr += 4 * 4;

			memcpy(&destPtr[0], &p0, sizeof(uint32_t));
			memcpy(&destPtr[4], &p1, sizeof(uint32_t));
			memcpy(&destPtr[8], &p0, sizeof(uint32_t));
			memcpy(&destPtr[12], &p1, sizeof(uint32_t));
			destPtr += 4 * 4;

			memcpy(&destPtr[0], &p2, sizeof(uint32_t));
			memcpy(&destPtr[4], &p3, sizeof(uint32_t));
			memcpy(&destPtr[8], &p2, sizeof(uint32_t));
			memcpy(&destPtr[12], &p3, sizeof(uint32_t));
			destPtr += 4 * 4;

			srcPtr += rowPitch * 2;
		}
		else
		{
			// 1x1 replicate 
			auto sPtr0 = reinterpret_cast<const uint32_t*>(srcPtr);

			uint32_t p0 = (swizzle) ? Swizzle(*sPtr0, ignorealpha) : *sPtr0;

			memcpy(&destPtr[0], &p0, sizeof(uint32_t));
			memcpy(&destPtr[4], &p0, sizeof(uint32_t));
			memcpy(&destPtr[8], &p0, sizeof(uint32_t));
			memcpy(&destPtr[12], &p0, sizeof(uint32_t));
			destPtr += 4 * 4;

			memcpy(&destPtr[0], &p0, sizeof(uint32_t));
			memcpy(&destPtr[4], &p0, sizeof(uint32_t));
			memcpy(&destPtr[8], &p0, sizeof(uint32_t));
			memcpy(&destPtr[12], &p0, sizeof(uint32_t));
			destPtr += 4 * 4;

			memcpy(&destPtr[0], &p0, sizeof(uint32_t));
			memcpy(&destPtr[4], &p0, sizeof(uint32_t));
			memcpy(&destPtr[8], &p0, sizeof(uint32_t));
			memcpy(&destPtr[12], &p0, sizeof(uint32_t));
			destPtr += 4 * 4;

			memcpy(&destPtr[0], &p0, sizeof(uint32_t));
			memcpy(&destPtr[4], &p0, sizeof(uint32_t));
			memcpy(&destPtr[8], &p0, sizeof(uint32_t));
			memcpy(&destPtr[12], &p0, sizeof(uint32_t));
			destPtr += 4 * 4;

			srcPtr += rowPitch;
		}

		destPtr = reinterpret_cast<uint8_t*>(AlignUp(reinterpret_cast<uintptr_t>(destPtr), 16));
	}
}
