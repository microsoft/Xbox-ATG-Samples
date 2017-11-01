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
DeviceResources::DeviceResources(DXGI_FORMAT backBufferFormat, DXGI_FORMAT depthBufferFormat, UINT backBufferCount, unsigned int flags) :
    m_screenViewport{},
    m_backBufferFormat((flags & c_EnableHDR) ? DXGI_FORMAT_R10G10B10A2_UNORM : backBufferFormat),
    m_depthBufferFormat(depthBufferFormat),
    m_backBufferCount(backBufferCount),
    m_window(nullptr),
    m_d3dFeatureLevel(D3D_FEATURE_LEVEL_11_1),
    m_outputSize{0, 0, 1920, 1080},
    m_options(flags),
    m_gameDVRFormat((flags & c_EnableHDR) ? backBufferFormat : DXGI_FORMAT_UNKNOWN)
{
}

// Configures the Direct3D device, and stores handles to it and the device context.
void DeviceResources::CreateDeviceResources()
{
    D3D11X_CREATE_DEVICE_PARAMETERS params = {};
    params.Version = D3D11_SDK_VERSION;

#ifdef _DEBUG
    // Enable the debug layer.
    params.Flags = D3D11_CREATE_DEVICE_DEBUG;
#elif defined(PROFILE)
    // Enable the instrumented driver.
    params.Flags = D3D11_CREATE_DEVICE_INSTRUMENTED;
#endif

    if (m_options & c_FastSemantics)
    {
        params.Flags |= D3D11_CREATE_DEVICE_IMMEDIATE_CONTEXT_FAST_SEMANTICS;
    }

    // Create the Direct3D 11 API device object and a corresponding context.
    ThrowIfFailed(D3D11XCreateDeviceX(
        &params,
        m_d3dDevice.ReleaseAndGetAddressOf(),
        m_d3dContext.ReleaseAndGetAddressOf()
        ));

#ifndef NDEBUG
    ComPtr<ID3D11InfoQueue> d3dInfoQueue;
    if (SUCCEEDED(m_d3dDevice.As(&d3dInfoQueue)))
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

    if (m_options & c_Enable4K_UHD)
    {
#if _XDK_VER >= 0x3F6803F3 /* XDK Edition 170600 */
        D3D11X_GPU_HARDWARE_CONFIGURATION hwConfig = {};
        m_d3dDevice->GetGpuHardwareConfiguration(&hwConfig);
        if (hwConfig.HardwareVersion >= D3D11X_HARDWARE_VERSION_XBOX_ONE_X)
        {
            m_outputSize = { 0, 0, 3840, 2160 };
#ifdef _DEBUG
            OutputDebugStringA("INFO: Swapchain using 4k (3840 x 2160) on Xbox One X\n");
#endif
        }
        else
        {
            m_options &= ~c_Enable4K_UHD;
#ifdef _DEBUG
            OutputDebugStringA("INFO: Swapchain using 1080p (1920 x 1080) on Xbox One or Xbox One S\n");
#endif
        }
#else
        m_options &= ~c_Enable4K_UHD;
#ifdef _DEBUG
        OutputDebugStringA("WARNING: Hardware detection not supported on this XDK edition; Swapchain using 1080p (1920 x 1080)\n");
#endif
#endif
    }
}

// These resources need to be recreated every time the window size is changed.
void DeviceResources::CreateWindowSizeDependentResources()
{
    if (!m_window)
    {
        throw std::exception("Call SetWindow with a valid CoreWindow pointer");
    }

    // Clear the previous window size specific context.
    ID3D11RenderTargetView* nullViews[] = {nullptr, nullptr};
    m_d3dContext->OMSetRenderTargets(_countof(nullViews), nullViews, nullptr);
    m_d3dRenderTargetView.Reset();
    m_d3dDepthStencilView.Reset();
    m_renderTarget.Reset();
    m_depthStencil.Reset();
    m_d3dGameDVRRenderTargetView.Reset();
    m_d3dGameDVRRenderTarget.Reset();
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

        if (m_swapChainGameDVR)
        {
            ThrowIfFailed(m_swapChainGameDVR->ResizeBuffers(
                m_backBufferCount,
                backBufferWidth,
                backBufferHeight,
                m_gameDVRFormat,
                0
                ));
        }
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
        swapChainDesc.Flags = (m_options & c_EnableHDR) ? DXGIX_SWAP_CHAIN_FLAG_COLORIMETRY_RGB_BT2020_ST2084 : DXGIX_SWAP_CHAIN_FLAG_QUANTIZATION_RGB_FULL;

        // Create a SwapChain from a CoreWindow.
        ThrowIfFailed(dxgiFactory->CreateSwapChainForCoreWindow(
            m_d3dDevice.Get(),
            m_window,
            &swapChainDesc,
            nullptr,
            m_swapChain.GetAddressOf()
            ));

        if ((m_options & c_EnableHDR) && !m_swapChainGameDVR)
        {
            swapChainDesc.Format = m_gameDVRFormat;
            swapChainDesc.Flags = DXGIX_SWAP_CHAIN_FLAG_QUANTIZATION_RGB_FULL;

            // Create a SwapChain from a CoreWindow.
            ThrowIfFailed(dxgiFactory->CreateSwapChainForCoreWindow(
                m_d3dDevice.Get(),
                m_window,
                &swapChainDesc,
                nullptr,
                m_swapChainGameDVR.GetAddressOf()
                ));
        }
    }

    // Create a render target view of the swap chain back buffer.
    ThrowIfFailed(m_swapChain->GetBuffer(0, IID_GRAPHICS_PPV_ARGS(m_renderTarget.ReleaseAndGetAddressOf())));

    m_renderTarget->SetName(L"Render target");

    ThrowIfFailed(m_d3dDevice->CreateRenderTargetView(
        m_renderTarget.Get(),
        nullptr,
        m_d3dRenderTargetView.ReleaseAndGetAddressOf()
        ));

    if (m_swapChainGameDVR)
    {
        ThrowIfFailed(m_swapChainGameDVR->GetBuffer(0, IID_GRAPHICS_PPV_ARGS(m_d3dGameDVRRenderTarget.ReleaseAndGetAddressOf())));

        m_d3dGameDVRRenderTarget->SetName(L"GameDVR Render target");

        ThrowIfFailed(m_d3dDevice->CreateRenderTargetView(
            m_d3dGameDVRRenderTarget.Get(),
            nullptr,
            m_d3dGameDVRRenderTargetView.ReleaseAndGetAddressOf()
            ));
    }

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

        ThrowIfFailed(m_d3dDevice->CreateTexture2D(
            &depthStencilDesc,
            nullptr,
            m_depthStencil.ReleaseAndGetAddressOf()
            ));

        CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2D);
        ThrowIfFailed(m_d3dDevice->CreateDepthStencilView(
            m_depthStencil.Get(),
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
    if (m_options & c_FastSemantics)
    {
        ThrowIfFailed(m_swapChain->GetBuffer(0, IID_GRAPHICS_PPV_ARGS(m_renderTarget.ReleaseAndGetAddressOf())));

        m_d3dDevice->PlaceSwapChainView(m_renderTarget.Get(), m_d3dRenderTargetView.Get());

        m_d3dContext->InsertWaitOnPresent(0, m_renderTarget.Get());

        if (m_swapChainGameDVR)
        {
            ThrowIfFailed(m_swapChainGameDVR->GetBuffer(0, IID_GRAPHICS_PPV_ARGS(m_d3dGameDVRRenderTarget.ReleaseAndGetAddressOf())));

            m_d3dDevice->PlaceSwapChainView(m_d3dGameDVRRenderTarget.Get(), m_d3dGameDVRRenderTargetView.Get());

            m_d3dContext->InsertWaitOnPresent(0, m_d3dGameDVRRenderTarget.Get());
        }
    }
}

// Present the contents of the swap chain to the screen.
void DeviceResources::Present(UINT decompressFlags)
{
    if ((m_options & c_FastSemantics) != 0 && decompressFlags != 0)
    {
        m_d3dContext->DecompressResource(
            m_renderTarget.Get(), 0, nullptr,
            m_renderTarget.Get(), 0, nullptr,
            m_backBufferFormat, decompressFlags);

        if (m_d3dGameDVRRenderTarget)
        {
            m_d3dContext->DecompressResource(
                m_d3dGameDVRRenderTarget.Get(), 0, nullptr,
                m_d3dGameDVRRenderTarget.Get(), 0, nullptr,
                m_gameDVRFormat, decompressFlags);
        }
    }

    if (m_swapChainGameDVR)
    {
        IDXGISwapChain1* ppSwapChains[2] = { m_swapChain.Get(), m_swapChainGameDVR.Get() };

        DXGIX_PRESENTARRAY_PARAMETERS presentParameterSets[2] = {};
        presentParameterSets[0].SourceRect = m_outputSize;
        presentParameterSets[0].ScaleFactorHorz = 1.0f;
        presentParameterSets[0].ScaleFactorVert = 1.0f;

        presentParameterSets[1] = presentParameterSets[0];

        DXGIXPresentArray(1, 0, 0, _countof(presentParameterSets), ppSwapChains, presentParameterSets);
    }
    else
    {
        ThrowIfFailed(m_swapChain->Present(1, 0));
    }

    // Xbox One apps do not need to handle DXGI_ERROR_DEVICE_REMOVED or DXGI_ERROR_DEVICE_RESET.
}
