//
// DeviceResources.h - A wrapper for the Direct3D 11 device and swapchain
//

#pragma once

namespace DX
{
    // Controls all the DirectX device resources.
    class DeviceResources
    {
    public:
        static const unsigned int c_FastSemantics   = 0x1;
        static const unsigned int c_Enable4K_UHD    = 0x2;
        static const unsigned int c_EnableHDR       = 0x4;

        DeviceResources(DXGI_FORMAT backBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM,
                        DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT,
                        UINT backBufferCount = 2,
                        unsigned int flags = 0);

        void CreateDeviceResources();
        void CreateWindowSizeDependentResources();
        void SetWindow(IUnknown* window) { m_window = window; }
        void Prepare();
        void Present(UINT decompressFlags = D3D11X_DECOMPRESS_PROPAGATE_COLOR_CLEAR);

        // Device Accessors.
        RECT GetOutputSize() const { return m_outputSize; }

        // Direct3D Accessors.
        ID3D11DeviceX*          GetD3DDevice() const                    { return m_d3dDevice.Get(); }
        ID3D11DeviceContextX*   GetD3DDeviceContext() const             { return m_d3dContext.Get(); }
        IDXGISwapChain1*        GetSwapChain() const                    { return m_swapChain.Get(); }
        D3D_FEATURE_LEVEL       GetDeviceFeatureLevel() const           { return m_d3dFeatureLevel; }
        ID3D11Texture2D*        GetRenderTarget() const                 { return m_renderTarget.Get(); }
        ID3D11Texture2D*        GetDepthStencil() const                 { return m_depthStencil.Get(); }
        ID3D11RenderTargetView*	GetRenderTargetView() const             { return m_d3dRenderTargetView.Get(); }
        ID3D11DepthStencilView* GetDepthStencilView() const             { return m_d3dDepthStencilView.Get(); }
        DXGI_FORMAT             GetBackBufferFormat() const             { return m_backBufferFormat; }
        DXGI_FORMAT             GetDepthBufferFormat() const            { return m_depthBufferFormat; }
        D3D11_VIEWPORT          GetScreenViewport() const               { return m_screenViewport; }
        UINT                    GetBackBufferCount() const              { return m_backBufferCount; }
        unsigned int            GetDeviceOptions() const                { return m_options; }

        // Direct3D HDR Game DVR support for Xbox One.
        IDXGISwapChain1*        GetGameDVRSwapChain() const             { return m_swapChainGameDVR.Get(); }
        ID3D11Texture2D*        GetGameDVRRenderTarget() const          { return m_d3dGameDVRRenderTarget.Get(); }
        ID3D11RenderTargetView*	GetGameDVRRenderTargetView() const      { return m_d3dGameDVRRenderTargetView.Get(); }
        DXGI_FORMAT             GetGameDVRFormat() const                { return m_gameDVRFormat; }

    private:
        // Direct3D objects.
        Microsoft::WRL::ComPtr<ID3D11DeviceX>           m_d3dDevice;
        Microsoft::WRL::ComPtr<ID3D11DeviceContextX>    m_d3dContext;
        Microsoft::WRL::ComPtr<IDXGISwapChain1>         m_swapChain;

        // Direct3D rendering objects. Required for 3D.
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         m_renderTarget;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         m_depthStencil;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  m_d3dRenderTargetView;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView>  m_d3dDepthStencilView;
        D3D11_VIEWPORT                                  m_screenViewport;

        // Direct3D properties.
        DXGI_FORMAT                                     m_backBufferFormat;
        DXGI_FORMAT                                     m_depthBufferFormat;
        UINT                                            m_backBufferCount;

        // Cached device properties.
        IUnknown*                                       m_window;
        D3D_FEATURE_LEVEL                               m_d3dFeatureLevel;
        RECT                                            m_outputSize;

        // DeviceResources options (see flags above)
        unsigned int                                    m_options;

        // Direct3D HDR Game DVR support for Xbox One.
        Microsoft::WRL::ComPtr<IDXGISwapChain1>         m_swapChainGameDVR;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         m_d3dGameDVRRenderTarget;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  m_d3dGameDVRRenderTargetView;
        DXGI_FORMAT                                     m_gameDVRFormat;
    };
}