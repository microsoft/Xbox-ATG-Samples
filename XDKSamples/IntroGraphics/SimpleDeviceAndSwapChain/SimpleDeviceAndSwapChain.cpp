//--------------------------------------------------------------------------------------
// SimpleDeviceAndSwapChain.cpp
//
// Setting up a Direct3D 11 device and swapchain for a Xbox One app
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimpleDeviceAndSwapChain.h"

#include "ATGColors.h"

extern void ExitSample() noexcept;

using namespace DirectX;

using Microsoft::WRL::ComPtr;

#define USE_FAST_SEMANTICS

#define ENABLE_4K

namespace
{
    const DXGI_FORMAT c_backBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    const DXGI_FORMAT c_depthBufferFormat = DXGI_FORMAT_D32_FLOAT;
    const UINT c_backBufferCount = 2;
}

Sample::Sample() :
    m_window(nullptr),
    m_outputWidth(1920),
    m_outputHeight(1080),
    m_featureLevel(D3D_FEATURE_LEVEL_11_1),
    m_frame(0)
{
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(IUnknown* window)
{
    m_window = window;

    CreateDevice();

    CreateResources();

    m_gamePad = std::make_unique<GamePad>();
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
void Sample::Update(DX::StepTimer const&)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        if (pad.IsViewPressed())
        {
            ExitSample();
        }
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
    Clear();

    // Render the frame.
    PIXBeginEvent(m_d3dContext.Get(), PIX_COLOR_DEFAULT, L"Render");
    
    m_batch->Begin();

    RECT fullscreen = { 0, 0, m_outputWidth, m_outputHeight };
    m_batch->Draw(m_background.Get(), fullscreen);

    m_batch->End();

    PIXEndEvent(m_d3dContext.Get());

    // Show the new frame.
    PIXBeginEvent(m_d3dContext.Get(), PIX_COLOR_DEFAULT, L"Present");
    Present();
    m_graphicsMemory->Commit();
    PIXEndEvent(m_d3dContext.Get());
}

// Helper method to clear the back buffers.
void Sample::Clear()
{
#ifdef USE_FAST_SEMANTICS
    // When using 11.X Fast Semantics, you need to do the swapchain rotation explicitly
    DX::ThrowIfFailed(m_swapChain->GetBuffer(0, IID_GRAPHICS_PPV_ARGS(m_renderTarget.ReleaseAndGetAddressOf())));

    m_d3dDevice->PlaceSwapChainView(m_renderTarget.Get(), m_renderTargetView.Get());

    m_d3dContext->InsertWaitOnPresent(0, m_renderTarget.Get());
#endif

    PIXBeginEvent(m_d3dContext.Get(), PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.
    m_d3dContext->ClearRenderTargetView(m_renderTargetView.Get(), ATG::Colors::Background);
    m_d3dContext->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    m_d3dContext->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), m_depthStencilView.Get());

    // Set the viewport.
    CD3D11_VIEWPORT viewport(0.0f, 0.0f, static_cast<float>(m_outputWidth), static_cast<float>(m_outputHeight));
    m_d3dContext->RSSetViewports(1, &viewport);

    m_batch->SetViewport(viewport);

    PIXEndEvent(m_d3dContext.Get());
}

// Presents the back buffer contents to the screen.
void Sample::Present()
{
    PIXBeginEvent(m_d3dContext.Get(), PIX_COLOR_DEFAULT, L"Present");

#ifdef USE_FAST_SEMANTICS
    // When using 11.X Fast Semantics, you need to decompress the render target before presenting
    m_d3dContext->DecompressResource(
        m_renderTarget.Get(), 0, nullptr,
        m_renderTarget.Get(), 0, nullptr,
        c_backBufferFormat, D3D11X_DECOMPRESS_PROPAGATE_COLOR_CLEAR);
#endif

    // The first argument instructs DXGI to block until VSync, putting the application
    // to sleep until the next VSync. This ensures we don't waste any cycles rendering
    // frames that will never be displayed to the screen.
    DX::ThrowIfFailed(m_swapChain->Present(1, 0));

    // Xbox One apps do not need to handle DXGI_ERROR_DEVICE_REMOVED or DXGI_ERROR_DEVICE_RESET.

    PIXEndEvent(m_d3dContext.Get());
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Sample::OnSuspending()
{
    //
    // Xbox One apps need to explictly suspend the GPU.
    //
    // Ensure that no other threads are rendering when this call is made.
    //

    m_d3dContext->Suspend(0);
}

void Sample::OnResuming()
{
    m_d3dContext->Resume();
    m_timer.ResetElapsedTime();
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Sample::CreateDevice()
{
    //
    // Classic Win32 and UWP apps use D3D11CreateDevice which is supported on Xbox One, but
    // use of D3D11XCreateDeviceX is recommended.
    //
    // Note that D3D11CreateDeviceAndSwapChain is not supported for Xbox One and is not recommended generally
    // 

    D3D11X_CREATE_DEVICE_PARAMETERS params = {};
    params.Version = D3D11_SDK_VERSION;

#ifdef _DEBUG
    // Enable the debug layer.
    params.Flags = D3D11_CREATE_DEVICE_DEBUG;
#elif defined(PROFILE)
    // Enable the instrumented driver.
    params.Flags = D3D11_CREATE_DEVICE_INSTRUMENTED;
#endif

#ifdef USE_FAST_SEMANTICS
    // Opt-in to 11.X Fast Semantics
    params.Flags |= D3D11_CREATE_DEVICE_IMMEDIATE_CONTEXT_FAST_SEMANTICS;
#endif

    // Create the Direct3D 11 API device object and a corresponding context.
    DX::ThrowIfFailed(D3D11XCreateDeviceX(
        &params,
        m_d3dDevice.ReleaseAndGetAddressOf(),
        m_d3dContext.ReleaseAndGetAddressOf()
    ));

    // Recommended debug layer settings
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

#if defined(ENABLE_4K) && (_XDK_VER >= 0x3F6803F3 /* XDK Edition 170600 */)
    // Opt-in to 4K swapchains on Xbox One X
    D3D11X_GPU_HARDWARE_CONFIGURATION hwConfig = {};
    m_d3dDevice->GetGpuHardwareConfiguration(&hwConfig);
    if (hwConfig.HardwareVersion >= D3D11X_HARDWARE_VERSION_XBOX_ONE_X)
    {
        m_outputWidth = 3840;
        m_outputHeight = 2160;
#ifdef _DEBUG
        OutputDebugStringA("INFO: Swapchain using 4k (3840 x 2160) on Xbox One X\n");
#endif
    }
    else
#endif

    {
#ifdef _DEBUG
        OutputDebugStringA("INFO: Swapchain using 1080p (1920 x 1080)\n");
#endif
    }

    // Initialize device dependent objects here (independent of window size).
    m_graphicsMemory = std::make_unique<GraphicsMemory>(m_d3dDevice.Get(), c_backBufferCount);

    m_batch = std::make_unique<SpriteBatch>(m_d3dContext.Get());
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateResources()
{
    // Determine the render target size in pixels.
    UINT backBufferWidth = static_cast<UINT>(m_outputWidth);
    UINT backBufferHeight = static_cast<UINT>(m_outputHeight);

    // If the swap chain already exists, resize it, otherwise create one.
    if (m_swapChain)
    {
        DX::ThrowIfFailed(m_swapChain->ResizeBuffers(c_backBufferCount, backBufferWidth, backBufferHeight, c_backBufferFormat, 0));

        // Xbox One apps do not need to handle DXGI_ERROR_DEVICE_REMOVED or DXGI_ERROR_DEVICE_RESET.
    }
    else
    {
        // First, retrieve the underlying DXGI Device from the D3D Device.
        ComPtr<IDXGIDevice1> dxgiDevice;
        DX::ThrowIfFailed(m_d3dDevice.As(&dxgiDevice));

        // Identify the physical adapter (GPU or card) this device is running on.
        ComPtr<IDXGIAdapter> dxgiAdapter;
        DX::ThrowIfFailed(dxgiDevice->GetAdapter(dxgiAdapter.GetAddressOf()));

        // And obtain the factory object that created it.
        ComPtr<IDXGIFactory2> dxgiFactory;
        DX::ThrowIfFailed(dxgiAdapter->GetParent(IID_GRAPHICS_PPV_ARGS(dxgiFactory.GetAddressOf())));

        // Create a descriptor for the swap chain.
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = backBufferWidth;
        swapChainDesc.Height = backBufferHeight;
        swapChainDesc.Format = c_backBufferFormat;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = c_backBufferCount;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        swapChainDesc.Flags = DXGIX_SWAP_CHAIN_FLAG_QUANTIZATION_RGB_FULL;

        // Create a SwapChain from a CoreWindow.
        DX::ThrowIfFailed(dxgiFactory->CreateSwapChainForCoreWindow(
            m_d3dDevice.Get(),
            m_window,
            &swapChainDesc,
            nullptr,
            m_swapChain.GetAddressOf()
        ));
    }

    // Obtain the backbuffer for this window which will be the final 3D rendertarget.
    DX::ThrowIfFailed(m_swapChain->GetBuffer(0, IID_GRAPHICS_PPV_ARGS(m_renderTarget.ReleaseAndGetAddressOf())));

    // Create a view interface on the rendertarget to use on bind.
    DX::ThrowIfFailed(m_d3dDevice->CreateRenderTargetView(m_renderTarget.Get(), nullptr, m_renderTargetView.ReleaseAndGetAddressOf()));

    // Allocate a 2-D surface as the depth/stencil buffer and
    // create a DepthStencil view on this surface to use on bind.
    CD3D11_TEXTURE2D_DESC depthStencilDesc(c_depthBufferFormat, backBufferWidth, backBufferHeight, 1, 1, D3D11_BIND_DEPTH_STENCIL);

    DX::ThrowIfFailed(m_d3dDevice->CreateTexture2D(&depthStencilDesc, nullptr, m_depthStencil.ReleaseAndGetAddressOf()));

    CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2D);
    DX::ThrowIfFailed(m_d3dDevice->CreateDepthStencilView(m_depthStencil.Get(), &depthStencilViewDesc, m_depthStencilView.ReleaseAndGetAddressOf()));

    // Initialize windows-size dependent objects here.
    DX::ThrowIfFailed(CreateDDSTextureFromFileEx(m_d3dDevice.Get(),
        m_outputHeight > 1080 ? L"3840x2160.dds" : L"1920x1080.dds", 0,
        D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0, true,
        nullptr, m_background.ReleaseAndGetAddressOf()));
}
#pragma endregion
