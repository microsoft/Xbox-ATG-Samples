//--------------------------------------------------------------------------------------
// SimpleDmaDecompression11.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimpleDmaDecompression11.h"


extern void ExitSample() noexcept;

using namespace DirectX;

using Microsoft::WRL::ComPtr;
using namespace XboxDmaCompression;


const wchar_t* CompressionOptionNames[]{
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
	m_UncompressedFileLoadTimeMS = 0;
	m_CompressedFileLoadTimeMS = 0;

	// Allocate a buffer for DMA error codes
	m_pErrorCodeBuffer = (uint32_t*)VirtualAlloc(nullptr, 64 * 1024, MEM_LARGE_PAGES | MEM_GRAPHICS | MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE | PAGE_GPU_COHERENT);
	if (!m_pErrorCodeBuffer)
	{
		OutputDebugString(L"Failed to allocate memory for m_pErrorCodeBuffer");
	}


	// This section demonstrates the usage of the streaming dma decompression helpers, add a variety of content that you would
	// otherwise be loading, along with DmaCompressionTool equivelent, and see how it impacts your load times.
	// For a test battery of 500MB of BC7 encoded textures, we've seen 25-40% improvement in I/O throughput, and reduction in size.
	LARGE_INTEGER start, end, freq;
	QueryPerformanceFrequency(&freq);
	wchar_t message[256];

	wchar_t* compressedFiles[] = {
		L"Media\\Textures\\*.dds.dcmp",
	};

	wchar_t* uncompressedFiles[] = {
		L"Media\\Textures\\*.dds",
	};

	QueryPerformanceCounter(&start);
	LoadFiles(compressedFiles, _countof(compressedFiles), true);
	QueryPerformanceCounter(&end);

	m_CompressedFileLoadTimeMS = (float)((end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart);
	swprintf_s(message, _countof(message), L"Compressed File Time: %fms\n", m_CompressedFileLoadTimeMS);
	OutputDebugString(message);

	QueryPerformanceCounter(&start);
	LoadFiles(uncompressedFiles, _countof(uncompressedFiles), false);
	QueryPerformanceCounter(&end);

	m_UncompressedFileLoadTimeMS = (float)((end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart);
	swprintf_s(message, _countof(message), L"Uncompressed File Time: %fms\n", m_UncompressedFileLoadTimeMS);
	OutputDebugString(message);

	m_currentFile = 0;

	uint8_t* pCompressedDataBuffer = (uint8_t*)VirtualAlloc(NULL, FRAGMENT_SIZE, MEM_LARGE_PAGES | MEM_GRAPHICS | MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE | PAGE_GPU_COHERENT);
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
	//  when using the software decoder, compression/decompression takes an extremely long time, and
	//  we don't want to block the main thread
	//
	InitializeCriticalSection(&m_CS);
	m_hThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)Sample::CompressionThreadFunc, this, 0, nullptr);
	if (m_hThread == nullptr)
	{
		OutputDebugString(L"Failed to create compression thread");
	}

}

void Sample::LoadFiles(wchar_t* filePaths[], int cPaths, bool compressed)
{
	uint32_t firstNewFile = (uint32_t)m_files.size();

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
			ZeroMemory(newFile, sizeof(FileInfo));

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
				// For compressed files, we can't do the other allocations till later when the real file size is known.
				ReadFileCompressed(newFile->Handle, reinterpret_cast<PVOID*>(&newFile->OriginalDataBuffer), newFile->OriginalDataSize, &(newFile->Overlapped), nullptr, nullptr, nullptr);
			}
			else
			{
				// Graphics accessible memory must be allocated in 64KB chunks
				newFile->OriginalDataBufferSize = (newFile->OriginalDataSize + DMA_MEMORY_ALLOCATION_SIZE - 1) & ~(DMA_MEMORY_ALLOCATION_SIZE - 1);
				newFile->OriginalDataBuffer = (uint8_t*)VirtualAlloc(nullptr, newFile->OriginalDataBufferSize, MEM_LARGE_PAGES | MEM_GRAPHICS | MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE | PAGE_GPU_COHERENT);
				newFile->DecompressedDataBuffer = (uint8_t*)VirtualAlloc(nullptr, newFile->OriginalDataBufferSize, MEM_LARGE_PAGES | MEM_GRAPHICS | MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE | PAGE_GPU_COHERENT);
				if (!newFile->OriginalDataBuffer || !newFile->DecompressedDataBuffer)
				{
					OutputDebugString(L"Failed to allocate memory for OriginalDataBuffer or DecompressedDataBuffer");
				}

				// Clear the buffers to zero initially so we can compare them later
				ZeroMemory(newFile->OriginalDataBuffer, newFile->OriginalDataBufferSize);
				ZeroMemory(newFile->DecompressedDataBuffer, newFile->OriginalDataBufferSize);
				// for unbuffered read, must be on 4kb boundary, 
				ReadFile(newFile->Handle, newFile->OriginalDataBuffer, (newFile->OriginalDataSize + 4095ull) & ~(4095ull), nullptr, &(newFile->Overlapped));
			}

			m_files.push_back(newFile);

			if (!FindNextFile(search, &findData))
			{
				CloseHandle(search);
				search = INVALID_HANDLE_VALUE;
			}
		}	// end while loop for single path search
	}	// end loop through paths

	for (int i = firstNewFile; i < m_files.size(); i++)
	{
		DWORD bytesRead = 0;
		if (GetOverlappedResult(m_files[i]->Handle, &(m_files[i]->Overlapped), &bytesRead, true))
		{
			if (compressed)
			{
				/* Compressed I/O completion, where bytes read will be larger than original size specified */
				m_files[i]->OriginalDataSize = bytesRead;
				m_files[i]->OriginalDataBufferSize = (m_files[i]->OriginalDataSize + DMA_MEMORY_ALLOCATION_SIZE - 1) & ~(DMA_MEMORY_ALLOCATION_SIZE - 1);
				m_files[i]->DecompressedDataBuffer = (uint8_t*)VirtualAlloc(nullptr, m_files[i]->OriginalDataBufferSize, MEM_LARGE_PAGES | MEM_GRAPHICS | MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE | PAGE_GPU_COHERENT);
				ZeroMemory(m_files[i]->DecompressedDataBuffer, m_files[i]->OriginalDataBufferSize);
			}
			else
			{
				/* Regular, uncompressed read completion */
				if (bytesRead != m_files[i]->OriginalDataSize)
				{
					OutputDebugString(L"Error Loading File, unexpected number of bytes.");
				}
				else
				{
					/*  ...though as a result of unbuffered I/O, must clear the end of the buffer */
					uint8_t* firstGarbageByte = m_files[i]->OriginalDataBuffer + m_files[i]->OriginalDataSize;
					uint64_t garbageBytes = ((((uint64_t)firstGarbageByte) + 4095ull) & ~4095ull) - (uint64_t)firstGarbageByte;
					ZeroMemory(firstGarbageByte, garbageBytes);
				}
			}
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
}





#pragma region Frame Update
// Executes basic render loop.
void Sample::Tick()
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Frame %I64u", m_frame);

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
    elapsedTime;

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
				m_currentFile = (uint32_t)m_files.size() - 1;
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

    // Prepare the render target to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Render");

	EnterCriticalSection(&m_CS);
	{
		if (m_spTextureSRV.Get())
		{
			D3D11_VIEWPORT vp;
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
			m_spriteBatch->Begin();
			m_spriteBatch->SetViewport(vp); 
			
			m_spriteBatch->Draw(m_spTextureSRV.Get(), rect);

			m_spriteBatch->End();

			m_decompressCompleted = TRUE;
		}

		// Display the UI
		m_spriteBatch->Begin();

		auto viewport = m_deviceResources->GetScreenViewport();
		m_spriteBatch->SetViewport(viewport);

		float x = 96.0f;
		float y = 40.0f;
		float yInc = 25.0f;
			
		DirectX::SimpleMath::Vector2 xy(x, y);
		m_fontOverlay->DrawString(m_spriteBatch.get(), L"SimpleDMADecompress11", xy);

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

    PIXEndEvent(context);

    // Show the new frame.
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    m_graphicsMemory->Commit();
    PIXEndEvent(context);
}

// Helper method to clear the back buffers.
void Sample::Clear()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.
    auto renderTarget = m_deviceResources->GetRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    context->ClearRenderTargetView(renderTarget, ATG::Colors::Background);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

    // Set the viewport.
    auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    PIXEndEvent(context);
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Sample::OnSuspending()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    context->Suspend(0);

	InterlockedExchange(&m_exitRequested, TRUE);
	WaitForSingleObject(m_hThread, 1000);
	CloseHandle(m_hThread);

	// Actually for shutdown, but for Sample simplicity, not implementing resume
	DeleteCriticalSection(&m_CS);

	VirtualFree(m_pErrorCodeBuffer, 0, MEM_RELEASE);
	for (unsigned int i = 0; i < m_files.size(); i++)
	{
		VirtualFree(m_files[i]->OriginalDataBuffer, 0, MEM_RELEASE);
		VirtualFree(m_files[i]->DecompressedDataBuffer, 0, MEM_RELEASE);
	}
	for (int i = 0; i < m_compressedDataFragmentBuffers.size(); i++)
	{
		VirtualFree(m_compressedDataFragmentBuffers[i], 0, MEM_RELEASE);
	}
}

void Sample::OnResuming()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    context->Resume();
    m_timer.ResetElapsedTime();
    m_gamePadButtons.Reset();
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Sample::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device, m_deviceResources->GetBackBufferCount());

	m_fontOverlay = std::make_unique<SpriteFont>(device, L"SegoeUI_18.spritefont");
	m_fontController = std::make_unique<SpriteFont>(device, L"XboxOneControllerSmall.spritefont");
	auto context = m_deviceResources->GetD3DDeviceContext();
	m_spriteBatch = std::make_unique<SpriteBatch>(context);

	// Create DMA contexts...  
	D3D11_DMA_ENGINE_CONTEXT_DESC dmaDesc;
	ZeroMemory(&dmaDesc, sizeof(dmaDesc));
	dmaDesc.CreateFlags = D3D11_DMA_ENGINE_CONTEXT_CREATE_SDMA_2;
	DX::ThrowIfFailed(device->CreateDmaEngineContext(&dmaDesc, m_spDma2.GetAddressOf()));
	dmaDesc.CreateFlags = D3D11_DMA_ENGINE_CONTEXT_CREATE_SDMA_3;
	DX::ThrowIfFailed(device->CreateDmaEngineContext(&dmaDesc, m_spDma3.GetAddressOf()));

	InitStreamingDma11(device, m_spDma2.Get(), DmaKickoffBehavior::Immediate);
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
_Use_decl_annotations_
void Sample::CompressWithDMA(std::vector<uint8_t*> &destFragments, std::vector<uint32_t> &originalSizes, _Out_ uint32_t* pFragmentCount, _In_ uint8_t* pSrc, uint32_t srcSize)
{
	// The hardware encoder also has the limitation of working on a 4MB input buffer...
	// ...and needs to produce resultant blocks that are <4MB to be compatible to round trip through the decoder, so artificially making
	// the fragment size smaller, just in case a file is encountered that bloats durring compression
	uint32_t maxCompressionBufferSize = (MAX_COMPRESSED_BUFFER_SIZE * 3) / 4;
	int requiredFragments = ((srcSize - 1) / maxCompressionBufferSize) + 1;
	for (size_t i = destFragments.size(); i < requiredFragments; i++)
	{
		uint8_t* pCompressedDataBuffer = (uint8_t*)VirtualAlloc(NULL, FRAGMENT_SIZE, MEM_LARGE_PAGES | MEM_GRAPHICS | MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE | PAGE_GPU_COHERENT);
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

	uint32_t bytesProcessed = 0;
	int block = 0;
	while (bytesProcessed < srcSize)
	{
		uint32_t bytesInThisBlock = std::min<uint32_t>(maxCompressionBufferSize, srcSize - bytesProcessed);
		DX::ThrowIfFailed(m_spDma3->LZCompressMemory(destFragments[block], pSrc + bytesProcessed, bytesInThisBlock, 0));
		m_spDma3->CopyLastErrorCodeToMemory(&m_pErrorCodeBuffer[block]);
		originalSizes[block] = bytesInThisBlock;
		bytesProcessed += bytesInThisBlock;
		block++;
	}
	uint64_t fence = m_spDma3->InsertFence(0);   // Insert a fence and kickoff

											   // Wait for the operation to complete
	//XSF::D3DDevice* const 
	auto pDev = m_deviceResources->GetD3DDevice();
	while (pDev->IsFencePending(fence))
	{
		SwitchToThread();
	}

	// Check the error code
	for (int i = 0; i < block; i++)
	{
		if (m_pErrorCodeBuffer[i] != 0)
		{
			OutputDebugString(L"DMA compress operation failed");
		}
	}
	*pFragmentCount = block;
}


//--------------------------------------------------------------------------------------
// Name: DecompressWithDMA()
// Desc: Decompress a memory buffer using a DMA operation
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void Sample::DecompressWithDMA(_Out_ uint8_t* pDest, std::vector<uint8_t*> &srcFragments, std::vector<uint32_t> &originalSizes, uint32_t fragmentCount)
{
	uint32_t destBytesProcessed = 0;
	for (unsigned int i = 0; i < fragmentCount; i++)
	{
		int uncompressedBytesInFragement = originalSizes[i];

		DX::ThrowIfFailed(m_spDma2->LZDecompressMemory(pDest + destBytesProcessed, srcFragments[i] + sizeof(uint32_t), *(uint32_t*)(srcFragments[i]), 0));
		m_spDma2->CopyLastErrorCodeToMemory(&m_pErrorCodeBuffer[i]);

		destBytesProcessed += uncompressedBytesInFragement;
	}
	uint64_t fence = m_spDma2->InsertFence(0);   // Insert a fence and kickoff

											   // Wait for the operation to complete
	//XSF::D3DDevice* const 
	auto pDev = m_deviceResources->GetD3DDevice();
	while (pDev->IsFencePending(fence))
	{
		SwitchToThread();
	}

	for (unsigned int i = 0; i < fragmentCount; i++)
	{
		// Check the error code
		if (m_pErrorCodeBuffer[i] != 0)
		{
			OutputDebugString(L"DMA decompress operation failed");
		}
	}
}


//--------------------------------------------------------------------------------------
// Name: CompressWithZlib()
// Desc: Compress a memory buffer using the software zlib library
//          Based on: http://zlib.net/zlib_how.html
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void Sample::CompressWithZlib(std::vector<uint8_t*> &destFragments, std::vector<uint32_t> &originalSizes, _Out_ uint32_t* fragmentCount, _In_ uint8_t* pSrc, uint32_t srcSize)
{
	int ret;
	z_stream strm;
	uint32_t const CHUNK = 128 * 1024;
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

	uint32_t workingBufferSize = deflateBound(&strm, srcSize) + sizeof(uint32_t);   // Add sizeof(uint32_t) because the hardware compressor prepends the size of the stream
	uint8_t* workingBuffer = (uint8_t*)VirtualAlloc(NULL, workingBufferSize, MEM_LARGE_PAGES | MEM_GRAPHICS | MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE | PAGE_GPU_COHERENT);
	if (!workingBuffer)
	{
		OutputDebugString(L"Failed to allocate memory for workingBuffer");
	}


	uint32_t bytesProcessed = 0;
	int fragmentIndex = 0;
	uint32_t prospectiveCompressionBytes = srcSize;
	while (bytesProcessed < srcSize)
	{
		prospectiveCompressionBytes = std::min<uint32_t>(prospectiveCompressionBytes, srcSize - bytesProcessed);
		CompressWithZlib_Fragment(workingBuffer, pSrc + bytesProcessed, prospectiveCompressionBytes);
		uint32_t requiredFragmentSpace = *(uint32_t*)workingBuffer + sizeof(uint32_t);

		while (requiredFragmentSpace > FRAGMENT_SIZE)
		{
			float currentRatio = (float)requiredFragmentSpace / MAX_COMPRESSED_BUFFER_SIZE;
			prospectiveCompressionBytes = (uint32_t)((prospectiveCompressionBytes / currentRatio) * 0.9f);
			prospectiveCompressionBytes &= (~3);	// ensure we're always tackling chunks that are 4 byte aligned.
			CompressWithZlib_Fragment(workingBuffer, pSrc + bytesProcessed, prospectiveCompressionBytes);
			requiredFragmentSpace = *(uint32_t*)workingBuffer + sizeof(uint32_t);
		}

		if (fragmentIndex >= destFragments.size())
		{
			uint8_t* pCompressedDataBuffer = (uint8_t*)VirtualAlloc(NULL, FRAGMENT_SIZE, MEM_LARGE_PAGES | MEM_GRAPHICS | MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE | PAGE_GPU_COHERENT);
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

void Sample::CompressWithZlib_Fragment(uint8_t* pDest, _In_ uint8_t* pSrc, uint32_t srcSize)
{
	// We prepend the size of the compressed data to match the behavior of the hardware encoder
	void* pCompressedSize = pDest;
	pDest = (uint8_t*)pDest + sizeof(uint32_t);

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
		pDest = (uint8_t*)pDest + have;

	} while (strm.avail_out == 0);
	assert(strm.avail_in == 0);     // all input will be used

									// done when last data in file processed
	assert(ret == Z_STREAM_END);        // stream will be complete 

										// clean up 
	(void)deflateEnd(&strm);

	// Write out the size of the compressed stream
	uint32_t compressedSize = (uint32_t)((uint8_t*)pDest - (uint8_t*)pCompressedSize - sizeof(uint32_t));
	*(uint32_t*)pCompressedSize = compressedSize;
}


//--------------------------------------------------------------------------------------
// Name: DecompressWithZlib()
// Desc: Decompress a memory buffer using the software zlib library
//          Based on: http://zlib.net/zlib_how.html
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void Sample::DecompressWithZlib(_Out_ uint8_t* pDest, std::vector<uint8_t*> &srcFragments, std::vector<uint32_t> &originalSizes, uint32_t fragmentCount)
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
void Sample::CompressWithZopfli(std::vector<uint8_t*> &destFragments, std::vector<uint32_t> &originalSizes, _Out_ uint32_t* fragmentCount, _In_ uint8_t* pSrc, uint32_t srcSize)
{
	uint32_t bytesProcessed = 0;
	int fragmentIndex = 0;
	uint32_t prospectiveCompressionBytes = srcSize;
	while (bytesProcessed < srcSize)
	{
		prospectiveCompressionBytes = std::min<uint32_t>(prospectiveCompressionBytes, srcSize - bytesProcessed);

		// Allocate compressed fragment space if needed...
		if (fragmentIndex >= destFragments.size())
		{
			uint8_t* pCompressedDataBuffer = (uint8_t*)VirtualAlloc(NULL, FRAGMENT_SIZE, MEM_LARGE_PAGES | MEM_GRAPHICS | MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE | PAGE_GPU_COHERENT);
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
		uint32_t requiredFragmentSpace = *(uint32_t*)(destFragments[fragmentIndex]) + sizeof(uint32_t);

		while (requiredFragmentSpace > FRAGMENT_SIZE)
		{
			float currentRatio = (float)requiredFragmentSpace / MAX_COMPRESSED_BUFFER_SIZE;
			prospectiveCompressionBytes = (uint32_t)((prospectiveCompressionBytes / currentRatio) * 0.9f);
			prospectiveCompressionBytes &= (~3);	// ensure we're always tackling chunks that are 4 byte aligned.
			CompressWithZopfli_Fragment(destFragments[fragmentIndex], pSrc + bytesProcessed, prospectiveCompressionBytes);
			requiredFragmentSpace = *(uint32_t*)(destFragments[fragmentIndex]) + sizeof(uint32_t);
		}

		originalSizes[fragmentIndex] = prospectiveCompressionBytes;
		bytesProcessed += prospectiveCompressionBytes;
		fragmentIndex++;
	}
	// clean up 
	*fragmentCount = fragmentIndex;
}


_Use_decl_annotations_
void Sample::CompressWithZopfli_Fragment(uint8_t* pDest, _In_ uint8_t* pSrc, uint32_t srcSize)
{
	// We prepend the size of the compressed data to match the behavior of the hardware encoder
	void* pCompressedSize = pDest;
	pDest = (uint8_t*)pDest + sizeof(uint32_t);

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
		*(uint32_t*)pCompressedSize = (uint32_t)outputBytes;
	}
	else
	{
		*(uint32_t*)pCompressedSize = (uint32_t)outputBytes;
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
	auto pDev = m_deviceResources->GetD3DDevice();

	LARGE_INTEGER start, end, freq;
	QueryPerformanceFrequency(&freq);

	while (FALSE == InterlockedCompareExchange(&m_exitRequested, FALSE, FALSE))
	{
		float compressTimeMS = 0;
		float decompressTimeMS = 0;

		// Read the compression settings (volatile), which can be modified on the main thread
		CompressionOption compressor = m_compressionOption;
		bool useHardwareDecompression = m_useHardwareDecompression;
		uint32_t currentFile = m_currentFile;

		//
		// Compress our data
		//
		{
			QueryPerformanceCounter(&start);

			switch (compressor)
			{
			case DmaHardware:
				CompressWithDMA(m_compressedDataFragmentBuffers, m_compressedDataFragmentOriginalSizes, &m_fragmentCount, m_files[currentFile]->OriginalDataBuffer, m_files[m_currentFile]->OriginalDataSize);
				break;
			case SoftwareZlib:
				CompressWithZlib(m_compressedDataFragmentBuffers, m_compressedDataFragmentOriginalSizes, &m_fragmentCount, m_files[currentFile]->OriginalDataBuffer, m_files[m_currentFile]->OriginalDataSize);
				break;
			case SoftwareZopfli:
				CompressWithZopfli(m_compressedDataFragmentBuffers, m_compressedDataFragmentOriginalSizes, &m_fragmentCount, m_files[currentFile]->OriginalDataBuffer, m_files[m_currentFile]->OriginalDataSize);
				break;
			}
			QueryPerformanceCounter(&end);
			compressTimeMS = (float)((end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart);
		}

		uint32_t compressedSize = 0;
		for (uint32_t i = 0; i < m_fragmentCount; i++)
		{
			compressedSize += *(uint32_t*)(m_compressedDataFragmentBuffers[i]);
		}
		m_compressedSize = compressedSize;

		//
		// Decompress the data we just compressed
		//
		{
			QueryPerformanceCounter(&start);

			if (m_useHardwareDecompression)
			{
				DecompressWithDMA(m_files[currentFile]->DecompressedDataBuffer, m_compressedDataFragmentBuffers, m_compressedDataFragmentOriginalSizes, m_fragmentCount);
			}
			else
			{
				DecompressWithZlib(m_files[currentFile]->DecompressedDataBuffer, m_compressedDataFragmentBuffers, m_compressedDataFragmentOriginalSizes, m_fragmentCount);
			}

			QueryPerformanceCounter(&end);
			decompressTimeMS = (float)((end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart);
		}


		//
		// Check that the decompressed data is the same as the original data
		//
		if (memcmp(m_files[currentFile]->OriginalDataBuffer, m_files[currentFile]->DecompressedDataBuffer, m_files[currentFile]->OriginalDataBufferSize) != 0)
		{
			OutputDebugString(L"Original and decompressed buffers are not equal");
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
			DX::ThrowIfFailed(DirectX::CreateDDSTextureFromMemory(pDev, m_files[currentFile]->DecompressedDataBuffer, m_files[currentFile]->OriginalDataSize, nullptr, m_spTextureSRV.ReleaseAndGetAddressOf()));
			D3D11_SHADER_RESOURCE_VIEW_DESC desc;
			m_spTextureSRV->GetDesc(&desc);

			m_compressTimeMS = compressTimeMS;
			m_decompressTimeMS = decompressTimeMS;
		}
		LeaveCriticalSection(&m_CS);
	}
}
#pragma endregion