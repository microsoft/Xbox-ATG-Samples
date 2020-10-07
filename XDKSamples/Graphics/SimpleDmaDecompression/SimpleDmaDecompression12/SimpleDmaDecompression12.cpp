//--------------------------------------------------------------------------------------
// SimpleDmaDecompression12.h
//
// Demonstrates how to use the hardware compression module to which implements DEFLATE
// www.rfc-base.org/rfc-1951.html
//
// The runtime operation demonstrates the hardware capabilities, along with zlib and zopfli options for comparison.
// Additionally, the load time code demonstrates how to use the DMA hardware to improve I/O throughput using the
// StreamingDmaDecompression helpers and on the offline DmaCompressionTool.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimpleDmaDecompression12.h"

#include "ATGColors.h"

extern void ExitSample() noexcept;

using namespace DirectX;

using Microsoft::WRL::ComPtr;
using namespace XboxDmaCompression;

#define RETURN_IF_FAILED( exp ) { HRESULT _hr_ = (exp); if( FAILED( _hr_ ) ) return _hr_; }

const WCHAR* CompressionOptionNames[]{
	L"Hardware",
	L"Software-Zlib",
	L"Software-Zopfli",
};
// zlib expects us to create these variables, even though we don't actually use them
extern "C" int maxDist = 0;
extern "C" int maxMatch = 0;

Sample::Sample() :
    m_frame(0)
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

	m_decompressCompleted = FALSE;
	m_exitRequested = FALSE;
	m_compressionOption = DmaHardware;
	m_useHardwareDecompression = TRUE;
	m_compressTimeMS = 0;
	m_decompressTimeMS = 0;


	// This section demonstrates the usage of the streaming dma decompression helpers, add a variety of content that you would
	// otherwise be loading, along with DmaCompressionTool equivelent, and see how it impacts your load times.
	// For a test battery of 500MB of BC7 encoded textures, we've seen 25-40% improvement in I/O throughput, and reduction in size.
	LARGE_INTEGER start, end, freq;
	QueryPerformanceFrequency(&freq);
	wchar_t message[256];

	wchar_t* compressedFiles[] = {
		L"Media\\Textures\\*.DDS.dcmp",
	};

	wchar_t* uncompressedFiles[] = {
		L"Media\\Textures\\*.DDS",
	};

	QueryPerformanceCounter(&start);
	LoadFiles(compressedFiles, _countof(compressedFiles), true);
	QueryPerformanceCounter(&end);

	m_CompressedFileLoadTimeMS = (FLOAT)((end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart);
	swprintf_s(message, _countof(message), L"Compressed File Time: %fms\n", m_CompressedFileLoadTimeMS);
	OutputDebugString(message);

	QueryPerformanceCounter(&start);
	LoadFiles(uncompressedFiles, _countof(uncompressedFiles), false);
	QueryPerformanceCounter(&end);

	m_UncompressedFileLoadTimeMS = (FLOAT)((end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart);
	swprintf_s(message, _countof(message), L"Uncompressed File Time: %fms\n", m_UncompressedFileLoadTimeMS);
	OutputDebugString(message);

	m_currentFile = 0;

	// Allocate the first working buffer for compression\decompression
	BYTE* pCompressedDataBuffer = (BYTE*)VirtualAlloc(NULL, FRAGMENT_SIZE, MEM_LARGE_PAGES | MEM_GRAPHICS | MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE | PAGE_GPU_COHERENT);
	if (!pCompressedDataBuffer)
	{
		OutputDebugString(L"Failed to allocate memory for m_pCompressedDataBuffer");
	}
	else
	{
		m_compressedDataFragmentBuffers.push_back(pCompressedDataBuffer);
		m_compressedDataFragmentOriginalSizes.push_back(0);
	}
	m_fragmentCount = 0;

	//
	// Create a background thread to perform compression and decompression. This is needed because
	// when using the software decoder, compression/decompression takes an extremely long time, and
	// we don't want to block the main thread
	//
	InitializeCriticalSection(&m_CS);
	m_hThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)Sample::CompressionThreadFunc, this, 0, nullptr);
	if (m_hThread == nullptr)
	{
		OutputDebugString(L"Failed to create compression thread");
	}

	m_currentRenderFence = 0;
	m_currentCompressionFence = 0;

}

// Load a DDS file, used to demonstrate compression / decompression.
//  In a real title you'd want to tile and compress your textures offline, then load and decompress the tiled data, 
//  then create a placement texture on top of it -- see the TextureStreaming sample
//
void Sample::LoadFiles(wchar_t* filePaths[], int cPaths, bool compressed)
{
	size_t firstNewFile = m_files.size();
	auto pDevice = m_deviceResources->GetD3DDevice();
	const D3D12_HEAP_PROPERTIES defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	for (int p = 0; p < cPaths; p++)
	{
		WIN32_FIND_DATA findData = { 0 };
		HANDLE search = FindFirstFile(filePaths[p], &findData);
		while (search != INVALID_HANDLE_VALUE)
		{
			wchar_t fullpath[MAX_PATH] = { 0 };
			wchar_t* lastSeparator = filePaths[p];
			wchar_t* nextSeparator; ;
			while ((nextSeparator = wcschr(lastSeparator + 1, L'\\')) != nullptr)
			{
				lastSeparator = nextSeparator;
			}
			wcsncat_s(fullpath, _countof(fullpath), filePaths[p], lastSeparator + 1 - filePaths[p]);
			wcscat_s(fullpath, _countof(fullpath), findData.cFileName);

			FileInfo* newFile = new FileInfo();
			if (newFile == nullptr)
			{
				OutputDebugString(L"Failed to load input file, out of memory");
			}

			// Open the input file
			newFile->Name = _wcsdup(fullpath);
			newFile->Handle = CreateFileW(fullpath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, NULL);

			if (newFile->Handle == INVALID_HANDLE_VALUE)
			{
				OutputDebugString(L"Failed to load input file");
			}
			// Get the file size and allocate space to load the file
			LARGE_INTEGER large;
			GetFileSizeEx(newFile->Handle, &large);
			newFile->OriginalDataSize = large.LowPart; // assume files are < 4GB
			newFile->Overlapped.hEvent = CreateEvent(nullptr, true, false, nullptr);

			if (compressed)
			{
				// This is just showing the custom allocator overload, of ReadFileCompressed.  We could instead create a placed resource after loading
				// when we know how many bytes a compressed stream expands to.
				struct AllocatorParam {
					ID3D12Device* Device;
					ID3D12Resource** Resource;
				};
				static AllocatorCallback bufferResourceAllocator = [](UINT byteCount, void* param)
				{
					AllocatorParam* externalParam = reinterpret_cast<AllocatorParam*>(param);
					const D3D12_HEAP_PROPERTIES defaultHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
					const D3D12_RESOURCE_DESC descBuffer = CD3DX12_RESOURCE_DESC::Buffer(byteCount);
					void* returnValue = nullptr;
					if (SUCCEEDED(externalParam->Device->CreateCommittedResource(
						&defaultHeapProps,
						D3D12_HEAP_FLAG_NONE,
						&descBuffer,
						D3D12_RESOURCE_STATE_COMMON,
						nullptr,
						IID_GRAPHICS_PPV_ARGS(externalParam->Resource))))
					{

						returnValue = (void*)(*externalParam->Resource)->GetGPUVirtualAddress();
					}
					else
					{
						SetLastError(ERROR_OUTOFMEMORY);
					}
					delete externalParam;
					return returnValue;
				};

				AllocatorParam* originalBufferAllocParam = new AllocatorParam({ pDevice, newFile->OriginalDataBuffer.ReleaseAndGetAddressOf() });
				ReadFileCompressed(newFile->Handle, nullptr, newFile->OriginalDataSize, &(newFile->Overlapped), bufferResourceAllocator, originalBufferAllocParam, nullptr);
			}
			else
			{
				// Graphics accessible memory must be allocated in 64KB chunks
				newFile->OriginalDataBufferSize = (newFile->OriginalDataSize + DMA_MEMORY_ALLOCATION_SIZE - 1) & ~(DMA_MEMORY_ALLOCATION_SIZE - 1);

				const D3D12_RESOURCE_DESC descOriginalBuffer = CD3DX12_RESOURCE_DESC::Buffer(newFile->OriginalDataBufferSize);
				DX::ThrowIfFailed(pDevice->CreateCommittedResource(
					&defaultHeapProperties,
					D3D12_HEAP_FLAG_NONE,
					&descOriginalBuffer,
					D3D12_RESOURCE_STATE_COMMON,
					nullptr,
					IID_GRAPHICS_PPV_ARGS(newFile->OriginalDataBuffer.ReleaseAndGetAddressOf())));
				DX::ThrowIfFailed(pDevice->CreateCommittedResource(
					&defaultHeapProperties,
					D3D12_HEAP_FLAG_NONE,
					&descOriginalBuffer,
					D3D12_RESOURCE_STATE_COPY_DEST,
					nullptr,
					IID_GRAPHICS_PPV_ARGS(newFile->DecompressedDataBuffer.ReleaseAndGetAddressOf())));

				// Clear the buffers to zero initially so we can compare them later
				ZeroMemory(reinterpret_cast<void*>(newFile->OriginalDataBuffer->GetGPUVirtualAddress()), newFile->OriginalDataBufferSize);
				ZeroMemory(reinterpret_cast<void*>(newFile->DecompressedDataBuffer->GetGPUVirtualAddress()), newFile->OriginalDataBufferSize);
				ReadFile(newFile->Handle, reinterpret_cast<BYTE*>(newFile->OriginalDataBuffer->GetGPUVirtualAddress()), (newFile->OriginalDataSize + 4095ull) & ~(4095ull), nullptr, &(newFile->Overlapped));
			}

			m_files.push_back(newFile);

			if (!FindNextFile(search, &findData))
			{
				CloseHandle(search);
				search = INVALID_HANDLE_VALUE;
			}
		}	// end while loop for single path search
	}	// end loop through paths

	ResourceUploadBatch resourceUpload(pDevice);
	resourceUpload.Begin();

	for (size_t i = firstNewFile; i < m_files.size(); i++)
	{
		DWORD bytesRead = 0;
		if (GetOverlappedResult(m_files[i]->Handle, &(m_files[i]->Overlapped), &bytesRead, true))
		{
			if (compressed)
			{
				/* Compressed I/O completion, where bytes read will be larger than original size specified */
				m_files[i]->OriginalDataSize = bytesRead;
				m_files[i]->OriginalDataBufferSize = (m_files[i]->OriginalDataSize + DMA_MEMORY_ALLOCATION_SIZE - 1) & ~(DMA_MEMORY_ALLOCATION_SIZE - 1); 

				const D3D12_RESOURCE_DESC descBuffer = CD3DX12_RESOURCE_DESC::Buffer(m_files[i]->OriginalDataBufferSize);
				DX::ThrowIfFailed(pDevice->CreateCommittedResource(
					&defaultHeapProperties,
					D3D12_HEAP_FLAG_NONE,
					&descBuffer,
					D3D12_RESOURCE_STATE_COPY_DEST,
					nullptr,
					IID_GRAPHICS_PPV_ARGS(m_files[i]->DecompressedDataBuffer.ReleaseAndGetAddressOf())));

				ZeroMemory(reinterpret_cast<void*>(m_files[i]->DecompressedDataBuffer->GetGPUVirtualAddress()), m_files[i]->OriginalDataBufferSize);
			}
			else {
				/* Regular, uncompressed read completion */
				if (bytesRead != m_files[i]->OriginalDataSize)
				{
					OutputDebugString(L"Error Loading File, unexpected number of bytes.");
				}
				else
				{
					/*  ...though as a result of unbuffered I/O, must clear the end of the buffer */
					UINT64 firstGarbageByte = m_files[i]->OriginalDataBuffer->GetGPUVirtualAddress() + m_files[i]->OriginalDataSize;
					UINT64 garbageBytes = ((((UINT64)firstGarbageByte) + 4095ull) & ~4095ull) - (UINT64)firstGarbageByte;
					ZeroMemory(reinterpret_cast<BYTE*>(firstGarbageByte), garbageBytes);
				}
			}
			resourceUpload.Transition(m_files[i]->OriginalDataBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
		}
		else
		{
			wchar_t message[256];
			swprintf_s(message, _countof(message), L"Error %d loading %s\n", GetLastError(), m_files[i]->Name);
			OutputDebugString(message);
		}
		CloseHandle(m_files[i]->Handle);
		m_files[i]->Handle = 0;
		CloseHandle(m_files[i]->Overlapped.hEvent);
		m_files[i]->Overlapped.hEvent = 0;
	}
	auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());
	uploadResourcesFinished.wait();
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
}

// Updates the world.
void Sample::Update(DX::StepTimer const& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");
	UNREFERENCED_PARAMETER(timer);

    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);

        if (pad.IsViewPressed())
        {
            ExitSample();
        }

		EnterCriticalSection(&m_CS);
		if (m_gamePadButtons.a == GamePad::ButtonStateTracker::ButtonState::PRESSED)
		{
			m_compressionOption = (CompressionOption)((m_compressionOption + 1) % CompressionOptionCount);
			m_compressTimeMS = 0;
			m_decompressCompleted = FALSE;
		}

		if (m_gamePadButtons.b == GamePad::ButtonStateTracker::ButtonState::PRESSED)
		{
			m_useHardwareDecompression = !m_useHardwareDecompression;
			m_decompressTimeMS = 0;
			m_decompressCompleted = FALSE;
		}

		if (m_gamePadButtons.dpadUp == GamePad::ButtonStateTracker::ButtonState::PRESSED)
		{
			if (m_currentFile == 0)
			{
				m_currentFile = (UINT)m_files.size() - 1;
			}
			else
			{
				m_currentFile--;
			}
			m_compressTimeMS = 0;
			m_decompressCompleted = FALSE;
		}

		if (m_gamePadButtons.dpadDown == GamePad::ButtonStateTracker::ButtonState::PRESSED)
		{
			m_currentFile = (m_currentFile + 1) % m_files.size();
			m_compressTimeMS = 0;
			m_decompressCompleted = FALSE;
		}
		LeaveCriticalSection(&m_CS);
    }
    else
    {
        m_gamePadButtons.Reset();
    }

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

    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

	EnterCriticalSection(&m_CS);
	{
		if (m_spTexture.Get())
		{
			m_currentRenderFence = ++m_currentFenceValue;

			D3D12_VIEWPORT vp;
			vp.TopLeftX = 616;
			vp.TopLeftY = 90;
			vp.MinDepth = 0;
			vp.MaxDepth = 1;
			vp.Width = 686;
			vp.Height = 900;

			RECT rect;
			rect.top = 100;
			rect.bottom = 800;
			rect.left = 260;
			rect.right = 500;

			// Draw the texture
			m_spriteBatch->Begin(commandList);
			m_spriteBatch->SetViewport(vp);

			auto size = GetTextureSize(m_spTexture.Get());
			m_spriteBatch->Draw(m_resourceDescriptorHeap->GetGpuHandle(ResourceDescriptors::Image), size,  rect);

			m_spriteBatch->End();
			
			m_decompressCompleted = TRUE;
		}


		// Display the UI
		m_spriteBatch->SetViewport(m_deviceResources->GetScreenViewport());
		m_spriteBatch->Begin(commandList);

		FLOAT x = 96.0f;
		FLOAT y = 40.0f;
		FLOAT yInc = 25.0f;

		DirectX::SimpleMath::Vector2 xy(x, y);
		m_fontOverlay->DrawString(m_spriteBatch.get(), L"SimpleDMADecompress12", xy);

		xy.y += 2 * yInc;
		wchar_t textLine[256];
		swprintf_s(textLine, _countof(textLine), L"File load time with async ReadFile(): %.2fms", m_UncompressedFileLoadTimeMS);
		m_fontOverlay->DrawString(m_spriteBatch.get(), textLine, xy);

		xy.y += yInc;
		swprintf_s(textLine, _countof(textLine), L"File load time with async ReadFileCompressed(): %.2fms", m_CompressedFileLoadTimeMS);
		DX::DrawControllerString(m_spriteBatch.get(), m_fontOverlay.get(), m_fontController.get(), textLine, xy);

		xy.y += 2 * yInc;
		swprintf_s(textLine, _countof(textLine), L"[DPad]File: %s", m_files[m_currentFile]->Name);
		DX::DrawControllerString(m_spriteBatch.get(), m_fontOverlay.get(), m_fontController.get(), textLine, xy);

		xy.y += 2 * yInc;
		if (m_compressTimeMS == 0)
		{
			swprintf_s(textLine, _countof(textLine), L"[A] Compression: %s -- IN PROGRESS %s", CompressionOptionNames[m_compressionOption], (m_compressionOption == SoftwareZopfli ? L"...can be slow for large files" : L""));
		}
		else
		{
			swprintf_s(textLine, _countof(textLine), L"[A] Compression: %s -- %.2f ms  (%d bytes)", CompressionOptionNames[m_compressionOption], m_compressTimeMS, m_compressedSize);
		}
		DX::DrawControllerString(m_spriteBatch.get(), m_fontOverlay.get(), m_fontController.get(), textLine, xy);

		xy.y += yInc;
		if (m_decompressTimeMS == 0)
		{
			swprintf_s(textLine, _countof(textLine), L"[B] Decompression: %s -- IN PROGRESS", m_useHardwareDecompression ? L"Hardware" : L"Software-Zlib");
		}
		else
		{
			swprintf_s(textLine, _countof(textLine), L"[B] Decompression: %s -- %.2f ms  (%d bytes)", m_useHardwareDecompression ? L"Hardware" : L"Software-Zlib", m_decompressTimeMS, m_files[m_currentFile]->OriginalDataSize);
		}
		DX::DrawControllerString(m_spriteBatch.get(), m_fontOverlay.get(), m_fontController.get(), textLine, xy);

		m_spriteBatch->End();
	
	}
	LeaveCriticalSection(&m_CS);

    PIXEndEvent(commandList);

    // Show the new frame.
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
	m_graphicsMemory->Commit(m_deviceResources->GetCommandQueue());

	DX::ThrowIfFailed(m_deviceResources->GetCommandQueue()->Signal(m_spCurrentFence.Get(), m_currentFenceValue));
	DX::ThrowIfFailed(m_spCurrentFence->SetEventOnCompletion(m_currentFenceValue, m_hFrameEvent));
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

HRESULT Sample::CreateCommandAndSyncObjects(
	_In_ ID3D12Device* const pDevice,
	_COM_Outptr_ ID3D12CommandAllocator** ppCmdAllocator,
	_COM_Outptr_ ID3D12XboxDmaCommandList** ppCmdList,
	_COM_Outptr_ ID3D12Fence** ppFence,
	UINT64 InitialFenceValue,
	_Outptr_ HANDLE *phSyncEvent)
{
	assert(pDevice != nullptr);
	assert(ppCmdAllocator != nullptr);
	assert(ppCmdList != nullptr);
	assert(ppFence != nullptr);
	assert(phSyncEvent != nullptr);

	*ppCmdAllocator = nullptr;
	*ppCmdList = nullptr;
	*ppFence = nullptr;
	*ppFence = nullptr;
	*phSyncEvent = nullptr;

	DX::ThrowIfFailed(pDevice->CreateCommandAllocator(D3D12XBOX_COMMAND_LIST_TYPE_DMA, IID_GRAPHICS_PPV_ARGS(ppCmdAllocator)));
	DX::ThrowIfFailed(pDevice->CreateCommandList(D3D12XBOX_NODE_MASK, D3D12XBOX_COMMAND_LIST_TYPE_DMA, *ppCmdAllocator, nullptr, IID_GRAPHICS_PPV_ARGS(ppCmdList)));
	DX::ThrowIfFailed(pDevice->CreateFence(InitialFenceValue, D3D12_FENCE_FLAG_NONE, IID_GRAPHICS_PPV_ARGS(ppFence)));
	*phSyncEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);

	return S_OK;
}

HRESULT Sample::WaitForFrame(
	_In_ ID3D12CommandQueue* const pCmdQueue,
	_In_ ID3D12Fence* const pFence,
	_Inout_ UINT64* pFenceValue,
	HANDLE hSyncEvent,
	_In_opt_ ID3D12CommandAllocator* const pCmdAllocator,
	_In_opt_ ID3D12XboxDmaCommandList* const pCmdList,
	_In_opt_ bool resetCommandListAndAllocator
)
{
	assert(pCmdQueue != nullptr);
	assert(pFence != nullptr);
	assert(pFenceValue != nullptr);
	assert(hSyncEvent != nullptr);

	// Increment the fence value and signal it
	RETURN_IF_FAILED(pCmdQueue->Signal(pFence, ++(*pFenceValue)));

	// Wait until the fence value is passed
	const UINT64 completedFence = pFence->GetCompletedValue();
	if (completedFence < *pFenceValue)
	{
		RETURN_IF_FAILED(pFence->SetEventOnCompletion(*pFenceValue, hSyncEvent));
		WaitForSingleObject(hSyncEvent, INFINITE);
	}

	// Optionally reset the command list and allocator
	if (resetCommandListAndAllocator)
	{
		assert(pCmdAllocator != nullptr);
		assert(pCmdList != nullptr);

		RETURN_IF_FAILED(pCmdAllocator->Reset());
		RETURN_IF_FAILED(pCmdList->Reset(pCmdAllocator, nullptr));
	}

	return S_OK;

}

#pragma region Message Handlers
// Message handlers
void Sample::OnSuspending()
{
    auto queue = m_deviceResources->GetCommandQueue();
    queue->SuspendX(0);

	InterlockedExchange(&m_exitRequested, TRUE);
	WaitForSingleObject(m_hThread, 1000);
	CloseHandle(m_hThread);

	// Actually for shutdown, but for Sample simplicity, not implementing resume
	CloseHandle(m_hFrameEvent);
	CloseHandle(m_hEvent2);
	CloseHandle(m_hEvent3);

	DeleteCriticalSection(&m_CS);

	for (unsigned int i = 0; i < m_files.size(); i++)
	{
		m_files[i]->OriginalDataBuffer.ReleaseAndGetAddressOf();
		m_files[i]->DecompressedDataBuffer.ReleaseAndGetAddressOf();
	}
	for (int i = 0; i < m_compressedDataFragmentBuffers.size(); i++)
	{
		VirtualFree(m_compressedDataFragmentBuffers[i], 0, MEM_RELEASE);
	}
}

void Sample::OnResuming()
{
    auto queue = m_deviceResources->GetCommandQueue();
    queue->ResumeX();
    m_timer.ResetElapsedTime();
    m_gamePadButtons.Reset();
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

	// Create descriptor heap for resources
	m_resourceDescriptorHeap = std::make_unique<DescriptorHeap>(device, static_cast<UINT>(ResourceDescriptors::Count));

	// initialize sync fence
	m_currentFenceValue = 0;
	DX::ThrowIfFailed(device->CreateFence(m_currentFenceValue++, D3D12_FENCE_FLAG_NONE, IID_GRAPHICS_PPV_ARGS(m_spCurrentFence.GetAddressOf())));
	m_hFrameEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);

	// Create DMA contexts, giving each one a 4KB ring buffer.
	D3D12XBOX_COMMAND_QUEUE_DESC descDmaQueue = {};
	descDmaQueue.Type = D3D12XBOX_COMMAND_LIST_TYPE_DMA;
	descDmaQueue.EngineOrPipeIndex = 2;
	DX::ThrowIfFailed(device->CreateCommandQueueX(&descDmaQueue, IID_GRAPHICS_PPV_ARGS(m_spCommandQueueDma2.ReleaseAndGetAddressOf())));
	m_fenceValue2 = 0;
	
	DX::ThrowIfFailed(CreateCommandAndSyncObjects(device, m_spCommandAllocatorDma2.ReleaseAndGetAddressOf(),
		m_spCommandListDma2.ReleaseAndGetAddressOf(), m_spFence2.ReleaseAndGetAddressOf(), m_fenceValue2, &m_hEvent2));

	descDmaQueue.EngineOrPipeIndex = 3;
	DX::ThrowIfFailed(device->CreateCommandQueueX(&descDmaQueue, IID_GRAPHICS_PPV_ARGS(m_spCommandQueueDma3.ReleaseAndGetAddressOf())));
	m_fenceValue3 = 0;
	DX::ThrowIfFailed(CreateCommandAndSyncObjects(device, m_spCommandAllocatorDma3.ReleaseAndGetAddressOf(),
		m_spCommandListDma3.ReleaseAndGetAddressOf(), m_spFence3.ReleaseAndGetAddressOf(), m_fenceValue3, &m_hEvent3));

	// Allocate a buffer for DMA error codes
	const D3D12_HEAP_PROPERTIES defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	const D3D12_RESOURCE_DESC descErrorCodeBuffer = CD3DX12_RESOURCE_DESC::Buffer(64 * 1024);
	DX::ThrowIfFailed(device->CreateCommittedResource(
		&defaultHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&descErrorCodeBuffer,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_GRAPHICS_PPV_ARGS(m_spErrorCodeBuffer.ReleaseAndGetAddressOf())));

	{
		ResourceUploadBatch resourceUpload(device);
		resourceUpload.Begin();

		SpriteBatchPipelineStateDescription pd(rtState, &CommonStates::AlphaBlend);
		m_spriteBatch = std::make_unique<SpriteBatch>(device, resourceUpload, pd);

		auto cpuDescHandleText = m_resourceDescriptorHeap->GetCpuHandle(static_cast<int>(ResourceDescriptors::TextFont));
		auto gpuDescHandleText = m_resourceDescriptorHeap->GetGpuHandle(static_cast<int>(ResourceDescriptors::TextFont));
		m_fontOverlay = std::make_unique<SpriteFont>(device, resourceUpload, L"SegoeUI_18.spritefont", cpuDescHandleText, gpuDescHandleText);

		auto cpuDescHandleController = m_resourceDescriptorHeap->GetCpuHandle(static_cast<int>(ResourceDescriptors::ControllerFont));
		auto gpuDescHandleController = m_resourceDescriptorHeap->GetGpuHandle(static_cast<int>(ResourceDescriptors::ControllerFont));
		m_fontController = std::make_unique<SpriteFont>(device, resourceUpload, L"XboxOneControllerSmall.spritefont", cpuDescHandleController, gpuDescHandleController);

		auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());
		uploadResourcesFinished.wait();     // Wait for resources to upload
	}
	
	InitStreamingDma12(device, m_spCommandQueueDma2.Get(), DmaKickoffBehavior::Immediate);
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
}
#pragma endregion


#pragma region Compression Helpers
//--------------------------------------------------------------------------------------
// Name: CompressWithDMA()
// Desc: Compress a memory buffer using a DMA operation
//--------------------------------------------------------------------------------------
//void Sample::CompressWithDMA(D3D12_GPU_VIRTUAL_ADDRESS vaDest, D3D12_GPU_VIRTUAL_ADDRESS vaSrc, UINT srcSize)
void Sample::CompressWithDMA(std::vector<BYTE*> &destFragments, std::vector<UINT> &originalSizes, _Out_ UINT* pFragmentCount, _In_ BYTE* pSrc, UINT srcSize)
{
	UINT* vaErrorCodeBuffer = reinterpret_cast<UINT*>(m_spErrorCodeBuffer->GetGPUVirtualAddress());

	// The hardware encoder also has the limitation of working on a 4MB input buffer...
	// ...and needs to produce resultant blocks that are <4MB to be compatible to round trip through the decoder, so artificially making
	// the fragment size smaller, just in case a file is encountered that bloats durring compression
	DWORD maxCompressionBufferSize = (MAX_COMPRESSED_BUFFER_SIZE * 3) / 4;

	UINT requiredFragments = ((srcSize - 1) / maxCompressionBufferSize) + 1;
	for (size_t i = destFragments.size(); i < requiredFragments; i++)
	{
		BYTE* pCompressedDataBuffer = (BYTE*)VirtualAlloc(NULL, FRAGMENT_SIZE, MEM_LARGE_PAGES | MEM_GRAPHICS | MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE | PAGE_GPU_COHERENT);
		if (!pCompressedDataBuffer)
		{
			OutputDebugString(L"Failed to allocate memory for m_pCompressedDataBuffer");
		}
		else
		{
			destFragments.push_back(pCompressedDataBuffer);
			originalSizes.push_back(0);
		}
	}

	UINT bytesProcessed = 0;
	int block = 0;
	while (bytesProcessed < srcSize)
	{
		UINT bytesInThisBlock = std::min<UINT>(maxCompressionBufferSize, srcSize - bytesProcessed);

		DX::ThrowIfFailed(m_spCommandListDma3->LZCompressMemoryX(
			reinterpret_cast<UINT64>(destFragments[block]),
			reinterpret_cast<UINT64>(pSrc + bytesProcessed),
			bytesInThisBlock));

		m_spCommandListDma3->CopyLastErrorCodeToMemoryX(
			reinterpret_cast<UINT64>(&(vaErrorCodeBuffer[block])));

		originalSizes[block] = bytesInThisBlock;
		bytesProcessed += bytesInThisBlock;
		block++;
	}

	DX::ThrowIfFailed(m_spCommandListDma3->Close());
	m_spCommandQueueDma3->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList*const*>(m_spCommandListDma3.GetAddressOf()));

	// Insert a fence and wait for the operation to finish
	DX::ThrowIfFailed(WaitForFrame(m_spCommandQueueDma3.Get(), m_spFence3.Get(), &m_fenceValue3, m_hEvent3, m_spCommandAllocatorDma3.Get(), m_spCommandListDma3.Get()));

	for (unsigned int i = 0; i < requiredFragments; i++)
	{
		// Check the error code
		if (vaErrorCodeBuffer[i] != 0)
		{
			OutputDebugString(L"DMA compress operation failed");
			break;
		}
	}
	*pFragmentCount = block;
}


//--------------------------------------------------------------------------------------
// Name: DecompressWithDMA()
// Desc: Decompress a memory buffer using a DMA operation
//--------------------------------------------------------------------------------------
void Sample::DecompressWithDMA(_Out_ BYTE* pDest, std::vector<BYTE*> &srcFragments, std::vector<UINT> &originalSizes, UINT fragmentCount)
{
	UINT* vaErrorCodeBuffer = reinterpret_cast<UINT*>(m_spErrorCodeBuffer->GetGPUVirtualAddress());

	UINT destBytesProcessed = 0;
	for (unsigned int i = 0; i < fragmentCount; i++)
	{
		int uncompressedBytesInFragement = originalSizes[i];

		// Advance four bytes past the beginning of the buffer to match the hardware compressor, which prepends a UINT with the stream size
		DX::ThrowIfFailed(m_spCommandListDma2->LZDecompressMemoryX(
			reinterpret_cast<UINT64>(pDest) + destBytesProcessed,
			reinterpret_cast<UINT64>(srcFragments[i]) + sizeof(UINT),
			*(UINT*)(srcFragments[i])));
		m_spCommandListDma2->CopyLastErrorCodeToMemoryX(
			reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS>(&(vaErrorCodeBuffer[i])));

		destBytesProcessed += uncompressedBytesInFragement;
	}

	DX::ThrowIfFailed(m_spCommandListDma2->Close());
	m_spCommandQueueDma2->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList*const*>(m_spCommandListDma2.GetAddressOf()));

	// Insert a fence and wait for the operation to finish
	DX::ThrowIfFailed(WaitForFrame(m_spCommandQueueDma2.Get(), m_spFence2.Get(), &m_fenceValue2, m_hEvent2, m_spCommandAllocatorDma2.Get(), m_spCommandListDma2.Get()));

	for (unsigned int i = 0; i < fragmentCount; i++)
	{
		// Check the error code
		if (vaErrorCodeBuffer[i] != 0)
		{
			OutputDebugString(L"DMA decompress operation failed");
			break;
		}
	}
}


//--------------------------------------------------------------------------------------
// Name: CompressWithZlib()
// Desc: Compress a memory buffer using the software zlib library
//          Based on: http://zlib.net/zlib_how.html
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void Sample::CompressWithZlib(std::vector<BYTE*> &destFragments, std::vector<UINT> &originalSizes, _Out_ UINT* fragmentCount, _In_ BYTE* pSrc, UINT srcSize)
{
	int ret;
	z_stream strm;
	UINT const CHUNK = 128 * 1024;
	unsigned char out[CHUNK];

	// allocate deflate state
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_out = CHUNK;
	strm.next_out = out;

	// These settings match the maximum settings decompressible by the hardware decoder.  The hardware encoder in
	// instead limited to a 10 bit window, but since decompression the primary scenario, using best settings for it.
	ret = deflateInit2(&strm,           // pointer to zlib structure
		Z_BEST_COMPRESSION,              // compressionLevel = highest
		Z_DEFLATED,						 // method = use DEFLATE algorithm
		12,                              // windowBits = 4KB (largest supported by decompression hardware)
		MAX_MEM_LEVEL,					 // memLevel = default                           
		Z_DEFAULT_STRATEGY);	         // strategy 

	UINT workingBufferSize = deflateBound(&strm, srcSize) + sizeof(UINT);   // Add sizeof(UINT) because the hardware compressor prepends the size of the stream
	BYTE* workingBuffer = (BYTE*)VirtualAlloc(NULL, workingBufferSize, MEM_LARGE_PAGES | MEM_GRAPHICS | MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE | PAGE_GPU_COHERENT);
	if (!workingBuffer)
	{
		OutputDebugString(L"Failed to allocate memory for workingBuffer");
	}


	UINT bytesProcessed = 0;
	int fragmentIndex = 0;
	UINT prospectiveCompressionBytes = srcSize;
	while (bytesProcessed < srcSize)
	{
		prospectiveCompressionBytes = std::min<UINT>(prospectiveCompressionBytes, srcSize - bytesProcessed);
		CompressWithZlib_Fragment(workingBuffer, pSrc + bytesProcessed, prospectiveCompressionBytes);
		UINT requiredFragmentSpace = *(UINT*)workingBuffer + sizeof(UINT);

		while (requiredFragmentSpace > FRAGMENT_SIZE)
		{
			float currentRatio = (float)requiredFragmentSpace / MAX_COMPRESSED_BUFFER_SIZE;
			prospectiveCompressionBytes = (UINT)((prospectiveCompressionBytes / currentRatio) * 0.9f);
			prospectiveCompressionBytes &= (~3);	// ensure we're always tackling chunks that are 4 byte aligned.
			CompressWithZlib_Fragment(workingBuffer, pSrc + bytesProcessed, prospectiveCompressionBytes);
			requiredFragmentSpace = *(UINT*)workingBuffer + sizeof(UINT);
		}

		if (fragmentIndex >= destFragments.size())
		{
			BYTE* pCompressedDataBuffer = (BYTE*)VirtualAlloc(NULL, FRAGMENT_SIZE, MEM_LARGE_PAGES | MEM_GRAPHICS | MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE | PAGE_GPU_COHERENT);
			if (!pCompressedDataBuffer)
			{
				OutputDebugString(L"Failed to allocate memory for m_pCompressedDataBuffer");
			}
			else
			{
				destFragments.push_back(pCompressedDataBuffer);
				originalSizes.push_back(0);
			}
		}
		memcpy_s(destFragments[fragmentIndex], FRAGMENT_SIZE, workingBuffer, requiredFragmentSpace);
		originalSizes[fragmentIndex] = prospectiveCompressionBytes;

		bytesProcessed += prospectiveCompressionBytes;
		fragmentIndex++;
	}
	// clean up 
	(void)deflateEnd(&strm);
	VirtualFree(workingBuffer, 0, MEM_RELEASE);
	*fragmentCount = fragmentIndex;
}

void Sample::CompressWithZlib_Fragment(BYTE* pDest, _In_ BYTE* pSrc, UINT srcSize)
{
	// We prepend the size of the compressed data to match the behavior of the hardware encoder
	void* pCompressedSize = pDest;
	pDest = (BYTE*)pDest + sizeof(UINT);

	int ret, flush;
	unsigned have;
	z_stream strm;

	// allocate deflate state
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_out = MAX_COMPRESSED_BUFFER_SIZE;
	strm.next_out = pDest;

	// These settings match the maximum settings decompressible by the hardware decoder.  The hardware encoder in
	// instead limited to a 10 bit window, but since decompression the primary scenario, using best settings for it.
	ret = deflateInit2(&strm,           // pointer to zlib structure
		Z_BEST_COMPRESSION,              // compressionLevel = highest
		Z_DEFLATED,						 // method = use DEFLATE algorithm
		12,                              // windowBits = 4KB (largest supported by decompression hardware)
		MAX_MEM_LEVEL,					 // memLevel = default                           
		Z_DEFAULT_STRATEGY);             // strategy 

	if (ret != Z_OK)
	{
		OutputDebugString(L"zlib compression failed");
	}

	/* compress until end of file */
	strm.avail_in = srcSize;
	flush = Z_FINISH;
	strm.next_in = (Bytef*)pSrc;

	//   run deflate() on input until output buffer not full, finish
	//   compression if all of source has been read in
	do {
		strm.avail_out = MAX_COMPRESSED_BUFFER_SIZE;
		strm.next_out = pDest; //out;

		ret = deflate(&strm, flush);    // no bad return value
		assert(ret != Z_STREAM_ERROR);  // state not clobbered

		have = MAX_COMPRESSED_BUFFER_SIZE - strm.avail_out;
		//memcpy( pDest, out, have );
		pDest = (BYTE*)pDest + have;

	} while (strm.avail_out == 0);
	assert(strm.avail_in == 0);     // all input will be used

									// done when last data in file processed
	assert(ret == Z_STREAM_END);        // stream will be complete 

										// clean up 
	(void)deflateEnd(&strm);

	// Write out the size of the compressed stream
	UINT compressedSize = (UINT)((BYTE*)pDest - (BYTE*)pCompressedSize - sizeof(UINT));
	*(UINT*)pCompressedSize = compressedSize;
}


//--------------------------------------------------------------------------------------
// Name: DecompressWithZlib()
// Desc: Decompress a memory buffer using the software zlib library
//          Based on: http://zlib.net/zlib_how.html
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void Sample::DecompressWithZlib(_Out_ BYTE* pDest, std::vector<BYTE*> &srcFragments, std::vector<UINT> &originalSizes, UINT fragmentCount)
{
	unsigned int bytesProcessed = 0;

	for (unsigned int frag = 0; frag < fragmentCount; frag++)
	{
		int ret;
		unsigned have;
		z_stream strm;

		// allocate inflate state
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		strm.avail_in = 0;
		strm.next_in = Z_NULL;
		ret = inflateInit(&strm);
		if (ret != Z_OK)
		{
			OutputDebugString(L"zlib decompression failed");
		}

		strm.avail_in = *(unsigned int *)srcFragments[frag];
		strm.next_in = ((Bytef*)srcFragments[frag]) + sizeof(int);

		strm.next_out = pDest + bytesProcessed;
		strm.avail_out = originalSizes[frag];

		ret = inflate(&strm, Z_NO_FLUSH);
		assert(ret != Z_STREAM_ERROR);  // state not clobbered
		switch (ret) {
		case Z_NEED_DICT:
			ret = Z_DATA_ERROR;     // and fall through
		case Z_DATA_ERROR:
		case Z_MEM_ERROR:
			(void)inflateEnd(&strm);
			OutputDebugString(L"zlib decompression failed");
		}

		have = originalSizes[frag] - strm.avail_out;

		if (have != originalSizes[frag])
		{
			OutputDebugString(L"zlib data corrupt");
		}
		bytesProcessed += have;

		// clean up and return
		(void)inflateEnd(&strm);
		if (ret != Z_STREAM_END)
		{
			OutputDebugString(L"zlib decompression failed");
		}
	}
}



//--------------------------------------------------------------------------------------
// Name: CompressWithZopfli()
// Desc: Compress a memory buffer using the software zopfli library
//--------------------------------------------------------------------------------------
void Sample::CompressWithZopfli(std::vector<BYTE*> &destFragments, std::vector<UINT> &originalSizes, _Out_ UINT* fragmentCount, _In_ BYTE* pSrc, UINT srcSize)
{
	UINT bytesProcessed = 0;
	int fragmentIndex = 0;
	UINT prospectiveCompressionBytes = srcSize;
	while (bytesProcessed < srcSize)
	{
		prospectiveCompressionBytes = std::min<UINT>(prospectiveCompressionBytes, srcSize - bytesProcessed);

		// Allocate compressed fragment space if needed...
		if (fragmentIndex >= destFragments.size())
		{
			BYTE* pCompressedDataBuffer = (BYTE*)VirtualAlloc(NULL, FRAGMENT_SIZE, MEM_LARGE_PAGES | MEM_GRAPHICS | MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE | PAGE_GPU_COHERENT);
			if (!pCompressedDataBuffer)
			{
				OutputDebugString(L"Failed to allocate memory for m_pCompressedDataBuffer");
			}
			else
			{
				destFragments.push_back(pCompressedDataBuffer);
				originalSizes.push_back(0);
			}
		}

		CompressWithZopfli_Fragment(destFragments[fragmentIndex], pSrc + bytesProcessed, prospectiveCompressionBytes);
		UINT requiredFragmentSpace = *(UINT*)(destFragments[fragmentIndex]) + sizeof(UINT);

		while (requiredFragmentSpace > FRAGMENT_SIZE)
		{
			float currentRatio = (float)requiredFragmentSpace / MAX_COMPRESSED_BUFFER_SIZE;
			prospectiveCompressionBytes = (UINT)((prospectiveCompressionBytes / currentRatio) * 0.9f);
			prospectiveCompressionBytes &= (~3);	// ensure we're always tackling chunks that are 4 byte aligned.
			CompressWithZopfli_Fragment(destFragments[fragmentIndex], pSrc + bytesProcessed, prospectiveCompressionBytes);
			requiredFragmentSpace = *(UINT*)(destFragments[fragmentIndex]) + sizeof(UINT);
		}

		originalSizes[fragmentIndex] = prospectiveCompressionBytes;
		bytesProcessed += prospectiveCompressionBytes;
		fragmentIndex++;
	}
	// clean up 
	*fragmentCount = fragmentIndex;
}


_Use_decl_annotations_
void Sample::CompressWithZopfli_Fragment(BYTE* pDest, _In_ BYTE* pSrc, UINT srcSize)
{
	// We prepend the size of the compressed data to match the behavior of the hardware encoder
	void* pCompressedSize = pDest;
	pDest = (BYTE*)pDest + sizeof(UINT);

	// More extreme options (bsm=90 & i=10) have been observed to rarely produce streams the hardware can't handle.
	// The options below have never been observed producing problematic compressed streams.
	ZopfliOptions options = { 0 };
	options.blocksplitting = true;
	options.blocksplittinglast = false;
	options.blocksplittingmax = 15;
	options.numiterations = 5;

	size_t outputBytes = 0;
	unsigned char * tempOutput;

	ZopfliZlibCompress(&options,
		(const unsigned char*)pSrc,
		srcSize,
		&tempOutput,
		&outputBytes);

	if (pCompressedSize && outputBytes <= MAX_COMPRESSED_BUFFER_SIZE)
	{
		memcpy(pDest, tempOutput, outputBytes);
		*(UINT*)pCompressedSize = (UINT)outputBytes;
	}
	else
	{
		*(UINT*)pCompressedSize = (UINT)outputBytes;
	}
	free(tempOutput);
}
#pragma endregion

//--------------------------------------------------------------------------------------
// Name: CompressionThreadFunc
// Desc: Entry point for the compression / decompression thread
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
/*static*/ void Sample::CompressionThreadFunc(void* pParam)
{
	SetThreadAffinityMask(GetCurrentThread(), 1);

	Sample* pSample = (Sample*)pParam;
	pSample->CompressionThread();
}

#pragma region Compression Thread
//--------------------------------------------------------------------------------------
// Name: CompressionThread
// Desc: Background thread that performs compression and decompression
//--------------------------------------------------------------------------------------
void Sample::CompressionThread()
{
	auto pDevice = m_deviceResources->GetD3DDevice();

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> spCmdAllocator;
	DX::ThrowIfFailed(pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_GRAPHICS_PPV_ARGS(spCmdAllocator.GetAddressOf())));

	LARGE_INTEGER start, end, freq;
	QueryPerformanceFrequency(&freq);

	while (FALSE == InterlockedCompareExchange(&m_exitRequested, FALSE, FALSE))
	{
		FLOAT compressTimeMS = 0;
		FLOAT decompressTimeMS = 0;

		// Read the compression settings (volatile), which can be modified on the main thread
		CompressionOption compressor = m_compressionOption;
		BOOL useHardwareDecompression = m_useHardwareDecompression;
		UINT currentFile = m_currentFile;

		//
		// Compress our data
		//
		{
			QueryPerformanceCounter(&start);
			switch (compressor)
			{
			case DmaHardware:
				CompressWithDMA(m_compressedDataFragmentBuffers, m_compressedDataFragmentOriginalSizes, &m_fragmentCount,
					reinterpret_cast<BYTE*>(m_files[currentFile]->OriginalDataBuffer->GetGPUVirtualAddress()),
					m_files[currentFile]->OriginalDataSize);
				break;
			case SoftwareZlib:
				CompressWithZlib(m_compressedDataFragmentBuffers, m_compressedDataFragmentOriginalSizes, &m_fragmentCount,
					reinterpret_cast<BYTE*>(m_files[currentFile]->OriginalDataBuffer->GetGPUVirtualAddress()),
					m_files[currentFile]->OriginalDataSize);
				break;
			case SoftwareZopfli:
				CompressWithZopfli(m_compressedDataFragmentBuffers, m_compressedDataFragmentOriginalSizes, &m_fragmentCount,
					reinterpret_cast<BYTE*>(m_files[currentFile]->OriginalDataBuffer->GetGPUVirtualAddress()),
					m_files[currentFile]->OriginalDataSize);
				break;
			}

			QueryPerformanceCounter(&end);
			compressTimeMS = static_cast<FLOAT>((end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart);
		}

		//
		// Decompress the data we just compressed
		//
		{
			QueryPerformanceCounter(&start);

			if (useHardwareDecompression)
			{
				DecompressWithDMA(reinterpret_cast<BYTE*>(m_files[currentFile]->DecompressedDataBuffer->GetGPUVirtualAddress()),
					m_compressedDataFragmentBuffers, m_compressedDataFragmentOriginalSizes, m_fragmentCount);
			}
			else
			{
				DecompressWithZlib(reinterpret_cast<BYTE*>(m_files[currentFile]->DecompressedDataBuffer->GetGPUVirtualAddress()),
					m_compressedDataFragmentBuffers, m_compressedDataFragmentOriginalSizes, m_fragmentCount);
			}

			QueryPerformanceCounter(&end);
			decompressTimeMS = static_cast<FLOAT>((end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart);
		}

		UINT compressedSize = 0;
		for (UINT i = 0; i < m_fragmentCount; i++)
		{
			compressedSize += *(UINT*)(m_compressedDataFragmentBuffers[i]);
		}
		m_compressedSize = compressedSize;

		//
		// Check that the decompressed data is the same as the original data
		//
		if (memcmp(reinterpret_cast<const void*>(m_files[currentFile]->OriginalDataBuffer->GetGPUVirtualAddress()), reinterpret_cast<void*>(m_files[currentFile]->DecompressedDataBuffer->GetGPUVirtualAddress()), m_files[currentFile]->OriginalDataBufferSize) != 0)
		{
			OutputDebugString(L"Original and decompressed buffers are not equal");
		}

		// wait for the render thread to finish using the texture before releasing it
		UINT64 lastCompletedValue = 0;
		while (m_currentRenderFence > (lastCompletedValue = m_spCurrentFence->GetCompletedValue()) || m_currentRenderFence < m_currentCompressionFence)
		{
			SwitchToThread();

			if (TRUE == InterlockedCompareExchange(&m_exitRequested, FALSE, FALSE))
			{
				break;
			}
		}

		//
		// Update the texture and timings
		//
		EnterCriticalSection(&m_CS);
		{
			// If the user changed the settings, throw out the work we just did and start over
			if (m_compressionOption != compressor || m_useHardwareDecompression != useHardwareDecompression || m_currentFile != currentFile)
			{
				LeaveCriticalSection(&m_CS);
				continue;
			}

			// Recreate the texture with the newly decompressed data
			PIXBeginEvent(PIX_COLOR_DEFAULT, L"SimpleDMADecompress:CompressionThread DDS load");

			ResourceUploadBatch resourceUpload(pDevice);
			resourceUpload.Begin();

			resourceUpload.Transition(m_files[currentFile]->DecompressedDataBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);

	
			DX::ThrowIfFailed(CreateDDSTextureFromMemory(pDevice, resourceUpload,
				reinterpret_cast<const uint8_t*>(m_files[currentFile]->DecompressedDataBuffer->GetGPUVirtualAddress()),
				m_files[currentFile]->OriginalDataSize,
				m_spTexture.ReleaseAndGetAddressOf()));

			CreateShaderResourceView(pDevice, m_spTexture.Get(),
				m_resourceDescriptorHeap->GetCpuHandle(ResourceDescriptors::Image));

			resourceUpload.Transition(m_files[currentFile]->DecompressedDataBuffer.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);

			auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());
			uploadResourcesFinished.wait();

			PIXEndEvent();

			m_currentCompressionFence = m_currentFenceValue;

			// Update our timings
			m_compressTimeMS = compressTimeMS;
			m_decompressTimeMS = decompressTimeMS;
		}
		LeaveCriticalSection(&m_CS);
	}
}

#pragma endregion
