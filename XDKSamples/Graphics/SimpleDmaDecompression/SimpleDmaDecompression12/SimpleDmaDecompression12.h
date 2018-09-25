//--------------------------------------------------------------------------------------
// SimpleDmaDecompression12.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"

enum CompressionOption {
	DmaHardware = 0,
	SoftwareZlib,
	SoftwareZopfli,
	CompressionOptionCount,
};

#define FRAGMENT_SIZE (4 * 1024 * 1024)

struct FileInfo {
	wchar_t*                               Name;
	Microsoft::WRL::ComPtr<ID3D12Resource> OriginalDataBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> DecompressedDataBuffer;
	UINT                                   OriginalDataSize;         // Actual size of the uncompressed file data
	UINT                                   OriginalDataBufferSize;   // Buffer size is rounded to the next page boundary

	HANDLE                                 Handle;
	OVERLAPPED                             Overlapped;

	FileInfo() :
		Name(nullptr),
		OriginalDataBuffer(),
		DecompressedDataBuffer(),
		OriginalDataSize(0),
		OriginalDataBufferSize(0),
		Handle(0),
		Overlapped{ 0 }
	{}
};

// A basic sample implementation that creates a D3D12 device and
// provides a render loop.
class Sample
{
public:

    Sample();

    // Initialization and management
    void Initialize(IUnknown* window);

    // Basic Sample loop
    void Tick();

    // Messages
    void OnSuspending();
    void OnResuming();

    // Properties
    bool RequestHDRMode() const { return m_deviceResources ? (m_deviceResources->GetDeviceOptions() & DX::DeviceResources::c_EnableHDR) != 0 : false; }

private:
	HRESULT CreateCommandAndSyncObjects(
		_In_ ID3D12Device* const pDevice,
		_COM_Outptr_ ID3D12CommandAllocator** ppCmdAllocator,
		_COM_Outptr_ ID3D12XboxDmaCommandList** ppCmdList,
		_COM_Outptr_ ID3D12Fence** ppFence,
		UINT64 InitialFenceValue,
		_Outptr_ HANDLE *phSyncEvent);

	HRESULT WaitForFrame(
		_In_ ID3D12CommandQueue* const pCmdQueue,
		_In_ ID3D12Fence* const pFence,
		_Inout_ UINT64* pFenceValue,
		HANDLE hSyncEvent,
		_In_opt_ ID3D12CommandAllocator* const pCmdAllocator = nullptr,
		_In_opt_ ID3D12XboxDmaCommandList* const pCmdList = nullptr,
		_In_opt_ bool resetCommandListAndAllocator = true
	);

	// Compression / Decompression functions
	void CompressWithDMA(std::vector<BYTE*> &destFragments, std::vector<UINT> &originalSizes, _Out_ UINT* fragmentCount, _In_ BYTE* pSrc, UINT srcSize);
	void CompressWithZlib(std::vector<BYTE*> &destFragments, std::vector<UINT> &originalSizes, _Out_ UINT* fragmentCount, _In_ BYTE* pSrc, UINT srcSize);
	void CompressWithZlib_Fragment(BYTE *pFragment, _In_ BYTE* pSrc, UINT srcSize);
	void CompressWithZopfli(std::vector<BYTE*> &destFragments, std::vector<UINT> &originalSizes, _Out_ UINT* fragmentCount, _In_ BYTE* pSrc, UINT srcSize);
	void CompressWithZopfli_Fragment(BYTE* pFragment, _In_ BYTE* pSrc, UINT srcSize);

	void DecompressWithDMA(_Out_ BYTE* pDest, std::vector<BYTE*> &srcFragments, std::vector<UINT> &originalSizes, UINT fragmentCount);
	void DecompressWithZlib(_Out_ BYTE* pDest, std::vector<BYTE*> &srcFragments, std::vector<UINT> &originalSizes, UINT fragmentCount);

	// Compression / Decompression thread
	static void CompressionThreadFunc(_In_ void* pParam);
	void CompressionThread();

	// Demonstration of streaming dma decompression
	void Sample::LoadFiles(wchar_t* filePaths[], int cPaths, bool compressed);


	// Settings -- written on the main thread and read by the compression/decompression thread
	volatile CompressionOption m_compressionOption;
	volatile BOOL              m_useHardwareDecompression;
	volatile UINT              m_currentFile;

	// Timings -- written on the compression/decompression thread and read by the main thread
	volatile FLOAT            m_compressTimeMS;
	volatile FLOAT            m_decompressTimeMS;
	volatile UINT             m_compressedSize;


	// Timing -- Streaming Dma Decompression, only used on main thread
	FLOAT                     m_UncompressedFileLoadTimeMS;
	FLOAT                     m_CompressedFileLoadTimeMS;

	// Critical section to synchronize main thread and compression/decompression thread
	CRITICAL_SECTION          m_CS;
	BOOL                      m_decompressCompleted;
	HANDLE                    m_hThread;
	volatile LONG             m_exitRequested;

	// File Info 
	std::vector<FileInfo*>    m_files;

	// Compressed data buffers, only used on compression thread
	std::vector<BYTE*>        m_compressedDataFragmentBuffers;
	std::vector<UINT>         m_compressedDataFragmentOriginalSizes;
	UINT                      m_fragmentCount;

	// DMA
	Microsoft::WRL::ComPtr<ID3D12CommandQueue>       m_spCommandQueueDma2;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator>   m_spCommandAllocatorDma2;
	Microsoft::WRL::ComPtr<ID3D12XboxDmaCommandList> m_spCommandListDma2;
	Microsoft::WRL::ComPtr<ID3D12Fence>              m_spFence2;
	UINT64                                           m_fenceValue2;
	HANDLE                                           m_hEvent2;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue>       m_spCommandQueueDma3;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator>   m_spCommandAllocatorDma3;
	Microsoft::WRL::ComPtr<ID3D12XboxDmaCommandList> m_spCommandListDma3;
	Microsoft::WRL::ComPtr<ID3D12Fence>              m_spFence3;
	UINT64                                           m_fenceValue3;
	HANDLE                                           m_hEvent3;

	Microsoft::WRL::ComPtr<ID3D12Resource>           m_spErrorCodeBuffer;

	// D3D
	Microsoft::WRL::ComPtr<ID3D12Fence>              m_spCurrentFence;
	Microsoft::WRL::ComPtr<ID3D12Resource>           m_spTexture;
	UINT64                                           m_currentFenceValue;
	HANDLE                                           m_hFrameEvent;
	UINT64                                           m_currentRenderFence;         // used to track texture usage in rendering path
	UINT64                                           m_currentCompressionFence;    // used to track texture usage in compression path

	std::unique_ptr<DirectX::DescriptorHeap>         m_resourceDescriptorHeap;
	std::unique_ptr<DirectX::SpriteFont>             m_fontOverlay;
	std::unique_ptr<DirectX::SpriteFont>             m_fontController;
	std::unique_ptr<DirectX::SpriteBatch>            m_spriteBatch;

	std::unique_ptr<DX::RenderTexture>               m_scene;


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

	// Desriptors for m_resourceDescriptorHeap
	enum ResourceDescriptors
	{
		TextFont,
		ControllerFont,
		Image,
		Count
	};
};
