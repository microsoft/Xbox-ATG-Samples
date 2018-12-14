//--------------------------------------------------------------------------------------
// FastBlockCompress.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "PerformanceTimersXbox.h"
#include "FullScreenQuad\FullScreenQuad.h"
#include "FBC_CPU.h"
#include "FBC_GPU.h"

class Image
{
public:
    Image(ID3D12Device* device, DirectX::ResourceUploadBatch& batch, DirectX::DescriptorPile* resourceDescriptors, const wchar_t* filename, DXGI_FORMAT compressedFormat);

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    Image(Image&&) = default;
    Image& operator=(Image&&) = default;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_sourceTexture;
    std::vector<size_t> m_srv;
    D3D12_RESOURCE_DESC m_desc;

	std::unique_ptr<uint8_t, aligned_deleter> m_rgbaTexture;
	std::vector<D3D12_SUBRESOURCE_DATA> m_rgbaSubresources;

    DXGI_FORMAT m_compressedFormat;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_bcTexture;
    std::vector<size_t> m_srvBC;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_bc7Texture;
    std::vector<size_t> m_srvBC7;

private:
	void MakeRGBATexture(uint32_t texSize, size_t levels, const D3D12_SUBRESOURCE_DATA* subresources, bool swizzle, bool ignorealpha);
};

// A basic sample implementation that creates a D3D12 device and
// provides a render loop.
class Sample
{
public:

    Sample() noexcept(false);

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

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    bool RMSError(ID3D12GraphicsCommandList* commandList,
        size_t originalTexture, size_t compressedTexture,
        uint32_t mipLevel, uint32_t width,
        bool reconstructZSource, bool reconstructZCompressed);

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
    std::unique_ptr<DirectX::DescriptorPile>    m_resourceDescriptors;
    std::unique_ptr<DirectX::SpriteBatch>       m_batch;
    std::unique_ptr<DirectX::SpriteFont>        m_font;
    std::unique_ptr<DirectX::SpriteFont>        m_ctrlFont;
    std::unique_ptr<DirectX::SpriteFont>        m_colorCtrlFont;

    enum Descriptors
    {
        Font,
        CtrlFont,
        ColorCtrlFont,
        RMS_Reduce_A_UAV,
        RMS_Reduce_B_UAV,
		CPU_MipBase,
		CPU_MipMax = CPU_MipBase + D3D12_REQ_MIP_LEVELS,
        Count
    };

    std::unique_ptr<DX::FullScreenQuad>         m_fullScreenQuad;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_quadPSO;

    // RMS computation
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rmsRootSig;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_rmsErrorPSO;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_rmsReducePSO;

    Microsoft::WRL::ComPtr<ID3D12Resource>      m_rmsReduceBufferA;
    Microsoft::WRL::ComPtr<ID3D12Resource>      m_rmsReduceBufferB;
    Microsoft::WRL::ComPtr<ID3D12Resource>      m_rmsResult;

    DirectX::XMFLOAT2                           m_RMSError; // .x RGB, .y ALPHA
    Microsoft::WRL::ComPtr<ID3D12Fence>         m_rmsFence;
    uint64_t                                    m_rmsFenceValue;
    Microsoft::WRL::Wrappers::Event             m_rmsFenceEvent;
    DirectX::GraphicsResource                   m_rmsCB;
    uint32_t                                    m_rmsWidth;
    bool                                        m_rmsPending;

	// Fast Block Compression
	CompressorCPU                               m_compressorCPU;
	CompressorGPU                               m_compressorGPU;

    Microsoft::WRL::ComPtr<ID3D12Resource>      m_fbcTexture;
    void*                                       m_fbcTextureMem;

	std::vector<size_t>							m_srvFBC;

	DX::GPUComputeTimer							m_gpuTimer;
	DX::CPUTimer								m_cpuTimer;

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>      m_computeAllocator;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>          m_computeCommandQueue;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>   m_computeCommandList;

    Microsoft::WRL::ComPtr<ID3D12Fence>                 m_computeFence; 
    uint64_t                                            m_computeFenceValue;
    Microsoft::WRL::Wrappers::Event                     m_computeFenceEvent;

    // Sample data
    std::vector<std::unique_ptr<Image>>         m_images;

    bool                                        m_fullscreen;
    bool                                        m_toggleOriginal;
    bool                                        m_highlightBlocks;
    bool                                        m_colorDiffs;
    bool                                        m_alphaDiffs;

    size_t                                      m_currentImage;
    int                                         m_currentMethod;
    int                                         m_mipLevel;

    float                                       m_zoom;
    float                                       m_offsetX;
    float                                       m_offsetY;

    enum
    {
        RTC_GPU,
		RTC_CPU,
        OFFLINE,
        OFFLINE_BC7,
        MAX_METHOD
    };
};
