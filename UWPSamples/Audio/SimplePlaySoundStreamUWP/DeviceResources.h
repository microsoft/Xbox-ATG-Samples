//
// DeviceResources.h - A wrapper for the Direct3D 11 device and swapchain
//

#pragma once

namespace DX
{
    // Provides an interface for an application that owns DeviceResources to be notified of the device being lost or created.
    interface IDeviceNotify
    {
        virtual void OnDeviceLost() = 0;
        virtual void OnDeviceRestored() = 0;
    };

    // Controls all the DirectX device resources.
    class DeviceResources
    {
    public:
        static const unsigned int c_AllowTearing    = 0x1;
        static const unsigned int c_EnableHDR       = 0x2;

        DeviceResources(DXGI_FORMAT backBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM,
                        DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D24_UNORM_S8_UINT,
                        UINT backBufferCount = 2,
                        D3D_FEATURE_LEVEL minFeatureLevel = D3D_FEATURE_LEVEL_9_3,
                        unsigned int flags = 0) noexcept;

        void CreateDeviceResources();
        void CreateWindowSizeDependentResources();
        void SetWindow(IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation);
        bool WindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation);
        void ValidateDevice();
        void HandleDeviceLost();
        void RegisterDeviceNotify(IDeviceNotify* deviceNotify) { m_deviceNotify = deviceNotify; }
        void Trim();
        void Present();

        // Device Accessors.
        RECT GetOutputSize() const             { return m_outputSize; }
        DXGI_MODE_ROTATION GetRotation() const { return m_rotation; }

        // Direct3D Accessors.
        ID3D11Device2*          GetD3DDevice() const                  { return m_d3dDevice.Get(); }
        ID3D11DeviceContext2*   GetD3DDeviceContext() const           { return m_d3dContext.Get(); }
        IDXGISwapChain3*        GetSwapChain() const                  { return m_swapChain.Get(); }
        D3D_FEATURE_LEVEL       GetDeviceFeatureLevel() const         { return m_d3dFeatureLevel; }
        ID3D11Texture2D*        GetRenderTarget() const               { return m_renderTarget.Get(); }
        ID3D11Texture2D*        GetDepthStencil() const               { return m_depthStencil.Get(); }
        ID3D11RenderTargetView* GetRenderTargetView() const           { return m_d3dRenderTargetView.Get(); }
        ID3D11DepthStencilView* GetDepthStencilView() const           { return m_d3dDepthStencilView.Get(); }
        DXGI_FORMAT             GetBackBufferFormat() const           { return m_backBufferFormat; }
        DXGI_FORMAT             GetDepthBufferFormat() const          { return m_depthBufferFormat; }
        D3D11_VIEWPORT          GetScreenViewport() const             { return m_screenViewport; }
        UINT                    GetBackBufferCount() const            { return m_backBufferCount; }
        DirectX::XMFLOAT4X4     GetOrientationTransform3D() const     { return m_orientationTransform3D; }
        DXGI_COLOR_SPACE_TYPE   GetColorSpace() const                 { return m_colorSpace; }
        unsigned int            GetDeviceOptions() const              { return m_options; }

    private:
        void GetHardwareAdapter(IDXGIAdapter1** ppAdapter);
        void UpdateColorSpace();

        // Direct3D objects.
        Microsoft::WRL::ComPtr<IDXGIFactory2>           m_dxgiFactory;
        Microsoft::WRL::ComPtr<ID3D11Device3>           m_d3dDevice;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext2>    m_d3dContext;
        Microsoft::WRL::ComPtr<IDXGISwapChain3>         m_swapChain;

        // Direct3D rendering objects. Required for 3D.
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         m_renderTarget;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         m_depthStencil;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>	m_d3dRenderTargetView;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView>	m_d3dDepthStencilView;
        D3D11_VIEWPORT                                  m_screenViewport;

        // Direct3D properties.
        DXGI_FORMAT                                     m_backBufferFormat;
        DXGI_FORMAT                                     m_depthBufferFormat;
        UINT                                            m_backBufferCount;
        D3D_FEATURE_LEVEL                               m_d3dMinFeatureLevel;

        // Cached device properties.
        IUnknown*                                       m_window;
        D3D_FEATURE_LEVEL                               m_d3dFeatureLevel;
        DXGI_MODE_ROTATION                              m_rotation;
        DWORD                                           m_dxgiFactoryFlags;
        RECT                                            m_outputSize;

        // Transforms used for display orientation.
        DirectX::XMFLOAT4X4                             m_orientationTransform3D;

        // HDR Support
        DXGI_COLOR_SPACE_TYPE                           m_colorSpace;

        // DeviceResources options (see flags above)
        unsigned int                                    m_options;

        // The IDeviceNotify can be held directly as it owns the DeviceResources.
        IDeviceNotify*                                  m_deviceNotify;
    };
}