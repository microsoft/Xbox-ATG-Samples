//--------------------------------------------------------------------------------------
// VideoTextureUWP.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "VideoTextureUWP.h"

#include "ATGColors.h"
#include "ControllerFont.h"

extern void ExitSample() noexcept;

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

Sample::Sample() noexcept(false) :
    m_show3D(true),
    m_videoWidth(0),
    m_videoHeight(0)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
    m_deviceResources->RegisterDeviceNotify(this);
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(::IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation)
{
    m_gamePad = std::make_unique<GamePad>();

    m_keyboard = std::make_unique<Keyboard>();
    m_keyboard->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));

    m_mouse = std::make_unique<Mouse>();
    m_mouse->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));

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

    m_world = Matrix::CreateRotationY(cosf(time) * 2.f);

    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);

        if (pad.IsViewPressed())
        {
            ExitSample();
        }

        if (m_gamePadButtons.a == GamePad::ButtonStateTracker::ButtonState::PRESSED)
        {
            m_show3D = !m_show3D;
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

    if (m_keyboardButtons.IsKeyPressed(Keyboard::Space))
    {
        m_show3D = !m_show3D;
    }

    if (m_player && m_player->IsFinished())
    {
        ExitSample();
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

    RECT r = { 0, 0, long(m_videoWidth), long(m_videoHeight) };
    MFVideoNormalizedRect rect = { 0.0f, 0.0f, 1.0f, 1.0f };
    m_player->TransferFrame(m_videoTexture.Get(), rect, r);

    Clear();

    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Render");

    RECT output = m_deviceResources->GetOutputSize();

    RECT safeRect = SimpleMath::Viewport::ComputeTitleSafeArea(output.right, output.bottom);

    if (m_show3D)
    {
        m_cube->Draw(m_world, m_view, m_proj, Colors::White, m_videoTextureView.Get());
    }
    else
    {
        m_spriteBatch->Begin(SpriteSortMode_Immediate, m_states->Opaque());

        m_spriteBatch->Draw(m_videoTextureView.Get(), XMFLOAT2(float(safeRect.left), float(safeRect.top)), nullptr, Colors::White);

        m_spriteBatch->End();
    }

    m_spriteBatch->Begin();

    DX::DrawControllerString(m_spriteBatch.get(),
        m_smallFont.get(), m_ctrlFont.get(),
        L"[View] / Esc  Exit   [A] / Space  Toggle texture vs. cutscene",
        XMFLOAT2(float(safeRect.left), float(safeRect.bottom) - m_smallFont->GetLineSpacing()),
        ATG::Colors::LightGrey);

    m_spriteBatch->End();

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
    auto renderTarget = m_deviceResources->GetRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    context->ClearRenderTargetView(renderTarget, ATG::Colors::Background);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

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

    m_states = std::make_unique<CommonStates>(device);

    m_player = std::make_unique<MediaEnginePlayer>();
    m_player->Initialize(device, DXGI_FORMAT_B8G8R8A8_UNORM);
    m_player->SetSource(L"SampleVideo.mp4");

    auto context = m_deviceResources->GetD3DDeviceContext();
    m_spriteBatch = std::make_unique<SpriteBatch>(context);

    m_cube = GeometricPrimitive::CreateCube(context);

    m_smallFont = std::make_unique<SpriteFont>(device, L"SegoeUI_18.spritefont");

    m_ctrlFont = std::make_unique<SpriteFont>(device, L"XboxOneControllerLegendSmall.spritefont");

    while (!m_player->IsInfoReady())
    {
        SwitchToThread();
    }

    m_player->GetNativeVideoSize(m_videoWidth, m_videoHeight);

#ifdef _DEBUG
    char buff[128] = {};
    sprintf_s(buff, "INFO: Video Size %u x %u\n", m_videoWidth, m_videoHeight);
    OutputDebugStringA(buff);
#endif

    CD3D11_TEXTURE2D_DESC desc(
        DXGI_FORMAT_B8G8R8A8_UNORM,
        m_videoWidth,
        m_videoHeight,
        1,
        1,
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
        D3D11_USAGE_DEFAULT,
        0,
        1,
        0,
        D3D11_RESOURCE_MISC_SHARED);

    DX::ThrowIfFailed(device->CreateTexture2D(&desc, nullptr, m_videoTexture.ReleaseAndGetAddressOf()));

    DX::ThrowIfFailed(device->CreateShaderResourceView(m_videoTexture.Get(), nullptr, m_videoTextureView.ReleaseAndGetAddressOf()));

    m_world = Matrix::Identity;
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    auto viewPort = m_deviceResources->GetScreenViewport();
    m_spriteBatch->SetViewport(viewPort);

    m_spriteBatch->SetRotation(m_deviceResources->GetRotation());

    m_view = Matrix::CreateLookAt(Vector3(2.f, 2.f, 2.f), Vector3::Zero, Vector3::UnitY);

    RECT output = m_deviceResources->GetOutputSize();

    m_proj = Matrix::CreatePerspectiveFieldOfView(XM_PI / 4.f, float(output.right) / float(output.bottom), 0.1f, 10.f);
}

void Sample::OnDeviceLost()
{
    m_player.reset();

    m_videoTexture.Reset();
    m_videoTextureView.Reset();

    m_spriteBatch.reset();
    m_states.reset();
    m_cube.reset();
    m_smallFont.reset();
    m_ctrlFont.reset();
}

void Sample::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
