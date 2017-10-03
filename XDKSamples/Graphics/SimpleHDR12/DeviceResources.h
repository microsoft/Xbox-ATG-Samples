//
// DeviceResources.h - A wrapper for the Direct3D 12 device and swapchain
//
// NOTE: This is a customized version of DeviceResources.cpp that has specific implementation for HDR swapchain creation
//

#pragma once

namespace DX
{
    // Controls all the DirectX device resources.
    class DeviceResources
    {
    public:
        static const unsigned int c_Enable4K_UHD    = 0x1;

        DeviceResources(DXGI_FORMAT gameDVRFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
                        DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT,
                        UINT backBufferCount = 2,
                        unsigned int flags = 0);
        ~DeviceResources();

        void CreateDeviceResources();
        void CreateWindowSizeDependentResources();
        void SetWindow(IUnknown* window) { m_window = window; }
        void Prepare(D3D12_RESOURCE_STATES beforeState = D3D12_RESOURCE_STATE_PRESENT);
        void Present(D3D12_RESOURCE_STATES beforeState = D3D12_RESOURCE_STATE_RENDER_TARGET, bool isTVInHDRMode = false);
        void WaitForGpu() noexcept;

        // Device Accessors.
        RECT GetOutputSize() const { return m_outputSize; }

        // Direct3D Accessors.
        ID3D12Device*               GetD3DDevice() const            { return m_d3dDevice.Get(); }
        D3D_FEATURE_LEVEL           GetDeviceFeatureLevel() const   { return m_d3dFeatureLevel; }
        ID3D12Resource*             GetDepthStencil() const         { return m_depthStencil.Get(); }
        ID3D12CommandQueue*         GetCommandQueue() const         { return m_commandQueue.Get(); }
        ID3D12CommandAllocator*     GetCommandAllocator() const     { return m_commandAllocators[m_backBufferIndex].Get(); }
        ID3D12GraphicsCommandList*  GetCommandList() const          { return m_commandList.Get(); }
        DXGI_FORMAT                 GetDepthBufferFormat() const    { return m_depthBufferFormat; }
        D3D12_VIEWPORT              GetScreenViewport() const       { return m_screenViewport; }
        D3D12_RECT                  GetScissorRect() const          { return m_scissorRect; }
        UINT                        GetCurrentFrameIndex() const    { return m_backBufferIndex; }
        UINT                        GetBackBufferCount() const      { return m_backBufferCount; }
        unsigned int                GetDeviceOptions() const        { return m_options; }

        // Descriptors for m_rtvDescriptorHeap
        enum RTVDescriptors
        {
            HDR10,
            GameDVR,
            Count
        };

        CD3DX12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const
        {
            return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
        }

        // Direct3D HDR10 swapchain.
        IDXGISwapChain1*            GetHDR10SwapChain() const       { return m_swapChainHDR10.Get(); }
        ID3D12Resource*             GetHDR10RenderTarget() const    { return m_renderTargetsHDR10[m_backBufferIndex].Get(); }
        DXGI_FORMAT                 GetHDR10BackBufferFormat() const { return DXGI_FORMAT_R10G10B10A2_UNORM; }

        CD3DX12_CPU_DESCRIPTOR_HANDLE GetHDR10RenderTargetView() const
        {   // Heap containst: HDR10[m_backBufferIndex], GameDVR[m_backBufferIndex], HDR10[m_backBufferIndex + 1], GameDVR[m_backBufferIndex + 1], ...
            return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), (m_backBufferIndex * RTVDescriptors::Count) + RTVDescriptors::HDR10, m_rtvDescriptorSize);
        }

        // Direct3D HDR Game DVR support for Xbox One.
        IDXGISwapChain1*            GetGameDVRSwapChain() const     { return m_swapChainGameDVR.Get(); }
        ID3D12Resource*             GetGameDVRRenderTarget() const  { return m_renderTargetsGameDVR[m_backBufferIndex].Get(); }
        DXGI_FORMAT                 GetGameDVRFormat() const        { return m_gameDVRFormat; }

        CD3DX12_CPU_DESCRIPTOR_HANDLE GetGameDVRRenderTargetView() const
        {   // Heap containst: HDR10[m_backBufferIndex], GameDVR[m_backBufferIndex], HDR10[m_backBufferIndex + 1], GameDVR[m_backBufferIndex + 1], ...
            return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), (m_backBufferIndex * RTVDescriptors::Count) + RTVDescriptors::GameDVR, m_rtvDescriptorSize);
        }

    private:
        void MoveToNextFrame();

        const static size_t MAX_BACK_BUFFER_COUNT = 3;

        UINT                                                m_backBufferIndex;

        // Direct3D objects.
        Microsoft::WRL::ComPtr<ID3D12Device>                m_d3dDevice;
        Microsoft::WRL::ComPtr<ID3D12CommandQueue>          m_commandQueue;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>   m_commandList;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator>      m_commandAllocators[MAX_BACK_BUFFER_COUNT];

        // Swap chain objects.
        Microsoft::WRL::ComPtr<ID3D12Resource>              m_depthStencil;

        // Presentation fence objects.
        Microsoft::WRL::ComPtr<ID3D12Fence>                 m_fence;
        UINT64                                              m_fenceValues[MAX_BACK_BUFFER_COUNT];
        Microsoft::WRL::Wrappers::Event                     m_fenceEvent;

        // Direct3D rendering objects.
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>        m_rtvDescriptorHeap;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>        m_dsvDescriptorHeap;
        UINT                                                m_rtvDescriptorSize;
        D3D12_VIEWPORT                                      m_screenViewport;
        D3D12_RECT                                          m_scissorRect;

        // Direct3D properties.
        DXGI_FORMAT                                         m_depthBufferFormat;
        UINT                                                m_backBufferCount;

        // Cached device properties.
        IUnknown*                                           m_window;
        D3D_FEATURE_LEVEL                                   m_d3dFeatureLevel;
        RECT                                                m_outputSize;

        // DeviceResources options (see flags above)
        unsigned int                                        m_options;

        // Direct3D HDR10 swapchain.
        Microsoft::WRL::ComPtr<IDXGISwapChain1>             m_swapChainHDR10;
        Microsoft::WRL::ComPtr<ID3D12Resource>              m_renderTargetsHDR10[MAX_BACK_BUFFER_COUNT];

        // Direct3D HDR Game DVR support for Xbox One.
        Microsoft::WRL::ComPtr<IDXGISwapChain1>             m_swapChainGameDVR;
        Microsoft::WRL::ComPtr<ID3D12Resource>              m_renderTargetsGameDVR[MAX_BACK_BUFFER_COUNT];
        DXGI_FORMAT                                         m_gameDVRFormat;
    };
}
