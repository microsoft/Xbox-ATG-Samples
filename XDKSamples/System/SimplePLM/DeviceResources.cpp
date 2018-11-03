//
// DeviceResources.cpp - A wrapper for the Direct3D 11 device and swapchain
//                       (requires DirectX 11.X Xbox One Monolithic Runtime)
//

#include "pch.h"
#include "DeviceResources.h"

using namespace DirectX;
using namespace DX;

using Microsoft::WRL::ComPtr;

// Constructor for DeviceResources.
DeviceResources::DeviceResources(DXGI_FORMAT backBufferFormat, DXGI_FORMAT depthBufferFormat, UINT backBufferCount) : 
    m_screenViewport{},
    m_backBufferFormat(backBufferFormat),
    m_depthBufferFormat(depthBufferFormat),
    m_backBufferCount(backBufferCount),
    m_window(nullptr),
    m_d3dFeatureLevel(D3D_FEATURE_LEVEL_11_1),
    m_outputSize{0, 0, 1920, 1080}
{
}

// Configures the Direct3D device, and stores handles to it and the device context.
void DeviceResources::CreateDeviceResources() 
{
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#ifdef _DEBUG
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#elif defined(PROFILE)
    creationFlags |= D3D11_CREATE_DEVICE_INSTRUMENTED;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = 
    {
        D3D_FEATURE_LEVEL_11_1,
    };

    // Create the Direct3D 11 API device object and a corresponding context.
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;

    ThrowIfFailed(D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        0,
        creationFlags,
        featureLevels,
        _countof(featureLevels),
        D3D11_SDK_VERSION,
        device.GetAddressOf(),      // Returns the Direct3D device created.
        &m_d3dFeatureLevel,         // Returns feature level of device created.
        context.GetAddressOf()      // Returns the device immediate context.
        ));

#ifndef NDEBUG
    ComPtr<ID3D11InfoQueue> d3dInfoQueue;
    if (SUCCEEDED(device.As(&d3dInfoQueue)))
    {
#ifdef _DEBUG
        d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
        d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
#endif
        D3D11_MESSAGE_ID hide[] =
        {
            D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS,
        };
        D3D11_INFO_QUEUE_FILTER filter = {};
        filter.DenyList.NumIDs = _countof(hide);
        filter.DenyList.pIDList = hide;
        d3dInfoQueue->AddStorageFilterEntries(&filter);
    }
#endif

    ThrowIfFailed(device.As(&m_d3dDevice));
    ThrowIfFailed(context.As(&m_d3dContext));
}

// These resources need to be recreated every time the window size is changed.
void DeviceResources::CreateWindowSizeDependentResources() 
{
    if (!m_window)
    {
        throw std::exception("Call SetWindow with a valid CoreWindow pointer");
    }

    // Clear the previous window size specific context.
    ID3D11RenderTargetView* nullViews[] = {nullptr};
    m_d3dContext->OMSetRenderTargets(_countof(nullViews), nullViews, nullptr);
    m_d3dRenderTargetView.Reset();
    m_d3dDepthStencilView.Reset();
    m_d3dContext->Flush();

    // Determine the render target size in pixels.
    UINT backBufferWidth = std::max<UINT>(m_outputSize.right - m_outputSize.left, 1);
    UINT backBufferHeight = std::max<UINT>(m_outputSize.bottom - m_outputSize.top, 1);

    if (m_swapChain)
    {
        // If the swap chain already exists, resize it.
        ThrowIfFailed(m_swapChain->ResizeBuffers(
            m_backBufferCount,
            backBufferWidth,
            backBufferHeight,
            m_backBufferFormat,
            0
            ));

        // Xbox One apps do not need to handle DXGI_ERROR_DEVICE_REMOVED or DXGI_ERROR_DEVICE_RESET.
    }
    else
    {
        // Otherwise, create a new one using the same adapter as the existing Direct3D device.

        // This sequence obtains the DXGI factory that was used to create the Direct3D device above.
        ComPtr<IDXGIDevice1> dxgiDevice;
        ThrowIfFailed(m_d3dDevice.As(&dxgiDevice));

        ComPtr<IDXGIAdapter> dxgiAdapter;
        ThrowIfFailed(dxgiDevice->GetAdapter(dxgiAdapter.GetAddressOf()));

        ComPtr<IDXGIFactory2> dxgiFactory;
        ThrowIfFailed(dxgiAdapter->GetParent(IID_GRAPHICS_PPV_ARGS(dxgiFactory.GetAddressOf())));

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = backBufferWidth;
        swapChainDesc.Height = backBufferHeight;
        swapChainDesc.Format = m_backBufferFormat;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = m_backBufferCount;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        swapChainDesc.Flags = DXGIX_SWAP_CHAIN_FLAG_QUANTIZATION_RGB_FULL;

        // Create a SwapChain from a CoreWindow.
        ThrowIfFailed(dxgiFactory->CreateSwapChainForCoreWindow(
            m_d3dDevice.Get(),
            m_window,
            &swapChainDesc,
            nullptr,
            m_swapChain.GetAddressOf()
            ));
    }

    // Create a render target view of the swap chain back buffer.
    ComPtr<ID3D11Texture2D> backBuffer;
    ThrowIfFailed(m_swapChain->GetBuffer(0, IID_GRAPHICS_PPV_ARGS(backBuffer.GetAddressOf())));

    ThrowIfFailed(m_d3dDevice->CreateRenderTargetView(
        backBuffer.Get(),
        nullptr,
        m_d3dRenderTargetView.ReleaseAndGetAddressOf()
        ));

    if (m_depthBufferFormat != DXGI_FORMAT_UNKNOWN)
    {
        // Create a depth stencil view for use with 3D rendering if needed.
        CD3D11_TEXTURE2D_DESC depthStencilDesc(
            m_depthBufferFormat,
            backBufferWidth,
            backBufferHeight,
            1, // This depth stencil view has only one texture.
            1, // Use a single mipmap level.
            D3D11_BIND_DEPTH_STENCIL
            );

        ComPtr<ID3D11Texture2D> depthStencil;
        ThrowIfFailed(m_d3dDevice->CreateTexture2D(
            &depthStencilDesc,
            nullptr,
            depthStencil.GetAddressOf()
            ));

        CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2D);
        ThrowIfFailed(m_d3dDevice->CreateDepthStencilView(
            depthStencil.Get(),
            &depthStencilViewDesc,
            m_d3dDepthStencilView.ReleaseAndGetAddressOf()
            ));
    }
    
    // Set the 3D rendering viewport to target the entire window.
    m_screenViewport = CD3D11_VIEWPORT(
        0.0f,
        0.0f,
        static_cast<float>(backBufferWidth),
        static_cast<float>(backBufferHeight)
        );

    m_outputSize.left = m_outputSize.top = 0;
    m_outputSize.right = backBufferWidth;
    m_outputSize.bottom = backBufferHeight;
}

// Prepare the render target for rendering.
void DeviceResources::Prepare()
{
}

// Present the contents of the swap chain to the screen.
void DeviceResources::Present() 
{
    // The first argument instructs DXGI to block until VSync, putting the application
    // to sleep until the next VSync. This ensures we don't waste any cycles rendering
    // frames that will never be displayed to the screen.
    ThrowIfFailed(m_swapChain->Present(1, 0));

    // Xbox One apps do not need to handle DXGI_ERROR_DEVICE_REMOVED or DXGI_ERROR_DEVICE_RESET.
}
