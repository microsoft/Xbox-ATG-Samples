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

    protected:
        ~IDeviceNotify() = default;
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
        ~DeviceResources() = default;

        DeviceResources(DeviceResources&&) = default;
        DeviceResources& operator= (DeviceResources&&) = default;

        DeviceResources(DeviceResources const&) = delete;
        DeviceResources& operator= (DeviceResources const&) = delete;

        void CreateDeviceResources();
        void CreateWindowSizeDependentResources();
        void SetWindow(IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation) noexcept;
        bool WindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation);
        void ValidateDevice();
        void HandleDeviceLost();
        void RegisterDeviceNotify(IDeviceNotify* deviceNotify) noexcept { m_deviceNotify = deviceNotify; }
        void Trim() noexcept;
        void Present();
        void UpdateColorSpace();

        // Device Accessors.
        RECT GetOutputSize() const noexcept             { return m_outputSize; }
        DXGI_MODE_ROTATION GetRotation() const noexcept { return m_rotation; }

        // Direct3D Accessors.
        auto                    GetD3DDevice() const noexcept              { return m_d3dDevice.Get(); }
        auto                    GetD3DDeviceContext() const noexcept       { return m_d3dContext.Get(); }
        auto                    GetSwapChain() const noexcept              { return m_swapChain.Get(); }
        auto                    GetDXGIFactory() const noexcept            { return m_dxgiFactory.Get(); }
        D3D_FEATURE_LEVEL       GetDeviceFeatureLevel() const noexcept     { return m_d3dFeatureLevel; }
        ID3D11Texture2D*        GetRenderTarget() const noexcept           { return m_renderTarget.Get(); }
        ID3D11Texture2D*        GetDepthStencil() const noexcept           { return m_depthStencil.Get(); }
        ID3D11RenderTargetView* GetRenderTargetView() const noexcept       { return m_d3dRenderTargetView.Get(); }
        ID3D11DepthStencilView* GetDepthStencilView() const noexcept       { return m_d3dDepthStencilView.Get(); }
        DXGI_FORMAT             GetBackBufferFormat() const noexcept       { return m_backBufferFormat; }
        DXGI_FORMAT             GetDepthBufferFormat() const noexcept      { return m_depthBufferFormat; }
        D3D11_VIEWPORT          GetScreenViewport() const noexcept         { return m_screenViewport; }
        UINT                    GetBackBufferCount() const noexcept        { return m_backBufferCount; }
        DirectX::XMFLOAT4X4     GetOrientationTransform3D() const noexcept { return m_orientationTransform3D; }
        DXGI_COLOR_SPACE_TYPE   GetColorSpace() const noexcept             { return m_colorSpace; }
        unsigned int            GetDeviceOptions() const noexcept          { return m_options; }

    private:
        void GetHardwareAdapter(IDXGIAdapter1** ppAdapter);

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
