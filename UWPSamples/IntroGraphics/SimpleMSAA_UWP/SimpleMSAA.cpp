//--------------------------------------------------------------------------------------
// SimpleMSAA.cpp
//
// This sample demonstrates setting up a MSAA render target for DirectX 11
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimpleMSAA.h"

#include "ATGColors.h"
#include "ControllerFont.h"

extern void ExitSample();

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

namespace
{
    const DXGI_FORMAT c_backBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    const DXGI_FORMAT c_depthBufferFormat = DXGI_FORMAT_D32_FLOAT;

    unsigned int c_targetSampleCount = 4;
}

Sample::Sample() :
    m_sampleCount(0),
    m_msaa(true),
    m_gamepadPresent(false)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_backBufferFormat,
        c_depthBufferFormat, /* If we were only doing MSAA rendering, we could skip the non-MSAA depth/stencil buffer with DXGI_FORMAT_UNKNOWN */
        2);
    m_deviceResources->RegisterDeviceNotify(this);

    //
    // In Win32 'classic' DirectX 11, you can create the 'swapchain' backbuffer as a multisample buffer.  Present took care of the
    // resolve as part of the swapchain management.  This implicit resolve behavior is not supported for UWP.
    //
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(::IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation)
{
    m_gamePad = std::make_unique<GamePad>();

    m_keyboard = std::make_unique<Keyboard>();
    m_keyboard->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));

    m_deviceResources->SetWindow(window, width, height, rotation);

    m_deviceResources->CreateDeviceResources();  	
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();
}

#pragma region Frame Update
// Executes basic render loop.
void Sample::Tick()
{
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();
}

// Updates the world.
void Sample::Update(DX::StepTimer const& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    float time = float(timer.GetTotalSeconds());

    m_world = Matrix::CreateRotationZ(cosf(time / 4.f));

    auto pad = m_gamePad->GetState(0);
    m_gamepadPresent = pad.IsConnected();
    if (m_gamepadPresent)
    {
        m_gamePadButtons.Update(pad);

        if (pad.IsViewPressed())
        {
            ExitSample();
        }

        if (m_gamePadButtons.a == GamePad::ButtonStateTracker::PRESSED)
        {
            m_msaa = !m_msaa;
        }
    }
    else
    {
        m_gamePadButtons.Reset();
    }

    auto kb = m_keyboard->GetState();
    m_keyboardButtons.Update(kb);

    if (kb.Escape)
    {
        ExitSample();
    }

    if (m_keyboardButtons.IsKeyPressed(Keyboard::Keys::Space))
    {
        m_msaa = !m_msaa;
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

    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Render");

    // Draw the scene.
    m_model->Draw(context, *m_states, m_world, m_view, m_proj);

    PIXEndEvent(context);

    if (m_msaa)
    {
        // Resolve the MSAA render target.
        PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Resolve");

        auto backBuffer = m_deviceResources->GetRenderTarget();
        context->ResolveSubresource(backBuffer, 0, m_msaaRenderTarget.Get(), 0, c_backBufferFormat);

        PIXEndEvent(context);

        // Set render target for UI which is typically rendered without MSAA.
        auto renderTarget = m_deviceResources->GetRenderTargetView();
        context->OMSetRenderTargets(1, &renderTarget, nullptr);
    }

    // Draw UI
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Draw UI");

    auto size = m_deviceResources->GetOutputSize();
    auto safe = SimpleMath::Viewport::ComputeTitleSafeArea(size.right, size.bottom);

    m_batch->Begin();

    wchar_t str[32] = {};
    swprintf_s(str, L"Sample count: %u", m_msaa ? m_sampleCount : 1);
    m_smallFont->DrawString(m_batch.get(), str, XMFLOAT2(float(safe.left), float(safe.top)), ATG::Colors::White);

    const wchar_t* legend = m_gamepadPresent
        ? L"[A] Toggle MSAA   [View] Exit"
        : L"Space: Toggle MSAA   Esc: Exit";

    DX::DrawControllerString(m_batch.get(),
        m_smallFont.get(), m_ctrlFont.get(),
        legend,
        XMFLOAT2(float(safe.left),
            float(safe.bottom) - m_smallFont->GetLineSpacing()),
        ATG::Colors::LightGrey);

    m_batch->End();

    PIXEndEvent(context);

    // Show the new frame.
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    PIXEndEvent();
}

// Helper method to clear the back buffers.
void Sample::Clear()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.

    if (m_msaa)
    {
        //
        // Rather than operate on the swapchain render target, we set up to render the scene to our MSAA resources instead.
        //

        context->ClearRenderTargetView(m_msaaRenderTargetView.Get(), ATG::ColorsLinear::Background);
        context->ClearDepthStencilView(m_msaaDepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

        context->OMSetRenderTargets(1, m_msaaRenderTargetView.GetAddressOf(), m_msaaDepthStencilView.Get());
    }
    else
    {
        auto renderTarget = m_deviceResources->GetRenderTargetView();
        auto depthStencil = m_deviceResources->GetDepthStencilView();

        context->ClearRenderTargetView(renderTarget, ATG::ColorsLinear::Background);
        context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

        context->OMSetRenderTargets(1, &renderTarget, depthStencil);
    }

    // Set the viewport.
    auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    PIXEndEvent(context);
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Sample::OnActivated()
{
}

void Sample::OnDeactivated()
{
}

void Sample::OnSuspending()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    context->ClearState();

    m_deviceResources->Trim();
}

void Sample::OnResuming()
{
    m_timer.ResetElapsedTime();
    m_gamePadButtons.Reset();
    m_keyboardButtons.Reset();
}

void Sample::OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation)
{
    if (!m_deviceResources->WindowSizeChanged(width, height, rotation))
        return;

    CreateWindowSizeDependentResources();
}

void Sample::ValidateDevice()
{
    m_deviceResources->ValidateDevice();
}

// Properties
void Sample::GetDefaultSize(int& width, int& height) const
{
    width = 1280;
    height = 720;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Sample::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();

    auto context = m_deviceResources->GetD3DDeviceContext();
    m_batch = std::make_unique<SpriteBatch>(context);

    m_states = std::make_unique<CommonStates>(device);

    m_fxFactory = std::make_unique<EffectFactory>(device);

    m_model = Model::CreateFromSDKMESH(device, L"CityBlockConcrete.sdkmesh", *m_fxFactory);

    m_world = Matrix::Identity;

    // Load UI.
    m_smallFont = std::make_unique<SpriteFont>(device, L"SegoeUI_18.spritefont");
    m_ctrlFont = std::make_unique<SpriteFont>(device, L"XboxOneControllerLegendSmall.spritefont");

    //
    // Check for MSAA support.
    //
    // Note that 4x MSAA is required for Direct3D Feature Level 10.1 or better
    //           8x MSAA is required for Direct3D Feature Level 11.0 or better
    //

    for (m_sampleCount = c_targetSampleCount; m_sampleCount > 1; m_sampleCount--)
    {
        UINT levels = 0;
        if (FAILED(device->CheckMultisampleQualityLevels(c_backBufferFormat, m_sampleCount, &levels)))
            continue;

        if (levels > 0)
            break;
    }

    if (m_sampleCount < 2)
    {
        throw std::exception("MSAA not supported");
    }
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    auto output = m_deviceResources->GetOutputSize();

    // Determine the render target size in pixels.
    UINT backBufferWidth = std::max<UINT>(output.right - output.left, 1);
    UINT backBufferHeight = std::max<UINT>(output.bottom - output.top, 1);

    // Create an MSAA render target.
    CD3D11_TEXTURE2D_DESC renderTargetDesc(
        c_backBufferFormat,
        backBufferWidth,
        backBufferHeight,
        1, // The render target view has only one texture.
        1, // Use a single mipmap level.
        D3D11_BIND_RENDER_TARGET,
        D3D11_USAGE_DEFAULT,
        0,
        m_sampleCount
    );

    auto device = m_deviceResources->GetD3DDevice();
    DX::ThrowIfFailed(device->CreateTexture2D(
        &renderTargetDesc,
        nullptr,
        m_msaaRenderTarget.ReleaseAndGetAddressOf()
    ));

    CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc(D3D11_RTV_DIMENSION_TEXTURE2DMS, c_backBufferFormat);

    DX::ThrowIfFailed(device->CreateRenderTargetView(
        m_msaaRenderTarget.Get(),
        &renderTargetViewDesc,
        m_msaaRenderTargetView.ReleaseAndGetAddressOf()
    ));

    // Create an MSAA depth stencil view.
    CD3D11_TEXTURE2D_DESC depthStencilDesc(
        c_depthBufferFormat,
        backBufferWidth,
        backBufferHeight,
        1, // This depth stencil view has only one texture.
        1, // Use a single mipmap level.
        D3D11_BIND_DEPTH_STENCIL,
        D3D11_USAGE_DEFAULT,
        0,
        m_sampleCount
    );

    ComPtr<ID3D11Texture2D> depthStencil;
    DX::ThrowIfFailed(device->CreateTexture2D(
        &depthStencilDesc,
        nullptr,
        depthStencil.GetAddressOf()
    ));

    DX::ThrowIfFailed(device->CreateDepthStencilView(
        depthStencil.Get(),
        nullptr,
        m_msaaDepthStencilView.ReleaseAndGetAddressOf()
    ));

    // Setup test scene.
    m_view = Matrix::CreateLookAt(Vector3(0, -211.f, -23.f), Vector3(6.f, 0.f, -37.f), -Vector3::UnitZ);

    m_proj = Matrix::CreatePerspectiveFieldOfView(XM_PI / 4.f,
        float(backBufferWidth) / float(backBufferHeight), 0.1f, 1000.f);

    auto viewport = m_deviceResources->GetScreenViewport();
    m_batch->SetViewport(viewport);
}

void Sample::OnDeviceLost()
{
    m_msaaRenderTarget.Reset();
    m_msaaRenderTargetView.Reset();
    m_msaaDepthStencilView.Reset();

    m_batch.reset();
    m_smallFont.reset();
    m_ctrlFont.reset();
    m_states.reset();
    m_model.reset();
    m_fxFactory.reset();
}

void Sample::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
