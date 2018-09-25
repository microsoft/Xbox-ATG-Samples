//--------------------------------------------------------------------------------------
// SimpleDmaDecompression11.h
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

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include <vector>

enum CompressionOption {
	DmaHardware = 0,
	SoftwareZlib,
	SoftwareZopfli,
	CompressionOptionCount,
};

struct FileInfo {
	wchar_t*      Name;
	uint8_t*         OriginalDataBuffer;
	uint8_t*         DecompressedDataBuffer;
	uint32_t          OriginalDataSize;         // Actual size of the uncompressed file data
	uint32_t          OriginalDataBufferSize;   // Buffer size is rounded to the next page boundary

	HANDLE        Handle;
	OVERLAPPED    Overlapped;
};

#define FRAGMENT_SIZE (4 * 1024 * 1024)

// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
class Sample
{
public:

    Sample();

    // Initialization and management
    void Initialize(IUnknown* window);

    // Basic game loop
    void Tick();

    // Messages
    void OnSuspending();
    void OnResuming();

    // Properties
    bool RequestHDRMode() const { return m_deviceResources ? (m_deviceResources->GetDeviceOptions() & DX::DeviceResources::c_EnableHDR) != 0 : false; }

private:
	// Compression / Decompression functions 
	void CompressWithDMA(std::vector<uint8_t*> &destFragments, std::vector<uint32_t> &originalSizes, _Out_ uint32_t* fragmentCount, _In_ uint8_t* pSrc, uint32_t srcSize);
	void CompressWithZlib(std::vector<uint8_t*> &destFragments, std::vector<uint32_t> &originalSizes, _Out_ uint32_t* fragmentCount, _In_ uint8_t* pSrc, uint32_t srcSize);
	void CompressWithZlib_Fragment(uint8_t *pFragment, _In_ uint8_t* pSrc, uint32_t srcSize);
	void CompressWithZopfli(std::vector<uint8_t*> &destFragments, std::vector<uint32_t> &originalSizes, _Out_ uint32_t* fragmentCount, _In_ uint8_t* pSrc, uint32_t srcSize);
	void CompressWithZopfli_Fragment(uint8_t* pFragment, _In_ uint8_t* pSrc, uint32_t srcSize);

	void DecompressWithDMA(_Out_ uint8_t* pDest, std::vector<uint8_t*> &srcFragments, std::vector<uint32_t> &originalSizes, uint32_t fragmentCount);
	void DecompressWithZlib(_Out_ uint8_t* pDest, std::vector<uint8_t*> &srcFragments, std::vector<uint32_t> &originalSizes, uint32_t fragmentCount);

	// Compression / Decompression thread
	static void CompressionThreadFunc(_In_ void* pParam);
	void CompressionThread();

	// Demonstration of streaming dma decompression
	void Sample::LoadFiles(wchar_t* filePaths[], int cPaths, bool compressed);


	// Settings -- written on the main thread and read by the compression/decompression thread
	volatile CompressionOption m_compressionOption;
	volatile bool              m_useHardwareDecompression;
	volatile uint32_t              m_currentFile;

	// Timings -- written on the compression/decompression thread and read by the main thread
	volatile float            m_compressTimeMS;
	volatile float            m_decompressTimeMS;
	volatile uint32_t             m_compressedSize;


	// Timing -- Streaming Dma Decompression, only used on main thread
	float                     m_UncompressedFileLoadTimeMS;
	float                     m_CompressedFileLoadTimeMS;

	// Critical section to synchronize main thread and compression/decompression thread
	bool                      m_decompressCompleted;
	CRITICAL_SECTION          m_CS;
	HANDLE                    m_hThread;
	volatile LONG             m_exitRequested;

	// File Info 
	std::vector<FileInfo*>    m_files;

	// Compressed data buffers, only used on compression thread
	std::vector<uint8_t*>        m_compressedDataFragmentBuffers;
	std::vector<uint32_t>         m_compressedDataFragmentOriginalSizes;
	uint32_t                      m_fragmentCount;

	// DMA
	Microsoft::WRL::ComPtr<ID3D11DmaEngineContextX>  m_spDma2;
	Microsoft::WRL::ComPtr<ID3D11DmaEngineContextX>  m_spDma3;
	uint32_t*                                            m_pErrorCodeBuffer;

	// D3D
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_spTextureSRV;
	Microsoft::WRL::ComPtr<ID3D11SamplerState>       m_spSamplerState;
	std::unique_ptr<DirectX::SpriteFont>             m_fontOverlay;
	std::unique_ptr<DirectX::SpriteFont>             m_fontController;
	std::unique_ptr<DirectX::SpriteBatch>            m_spriteBatch;

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>        m_deviceResources;

    // Rendering loop timer.
    uint64_t                                    m_frame;
    DX::StepTimer                               m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>           m_gamePad;

    DirectX::GamePad::ButtonStateTracker        m_gamePadButtons;

    // DirectXTK objects.
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;
};
