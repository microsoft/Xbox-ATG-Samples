//--------------------------------------------------------------------------------------
// SimpleComputePC.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimpleComputePC.h"

#include "ATGColors.h"
#include "FindMedia.h"
#include "ControllerFont.h"
#include "ReadData.h"

extern void ExitSample() noexcept;

using namespace DirectX;

using Microsoft::WRL::ComPtr;

namespace
{
    const uint32_t s_numShaderThreads = 8;  // make sure to update value in shader if this changes

    const wchar_t* g_SampleTitle = L"SimpleCompute";
    const wchar_t* g_SampleDescription = L"Demonstrates how to use DirectCompute";
    const ATG::HelpButtonAssignment g_HelpButtons[] = {
        { ATG::HelpID::MENU_BUTTON,         L"Show/Hide Help" },
        { ATG::HelpID::VIEW_BUTTON,         L"Exit" },
        { ATG::HelpID::LEFT_STICK,          L"Pan Viewport" },
        { ATG::HelpID::RIGHT_STICK,         L"Zoom Viewport" },
        { ATG::HelpID::RIGHT_TRIGGER,       L"Increase Zoom Speed" },
        { ATG::HelpID::Y_BUTTON,            L"Reset Viewport to Default" },
    };
}

Sample::Sample() noexcept(false) :
    m_showHelp(false),
    m_gamepadPresent(false)
{
    // Renders only 2D, so no need for a depth buffer.
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN, 2, D3D_FEATURE_LEVEL_11_0);
    m_deviceResources->RegisterDeviceNotify(this);

    m_help = std::make_unique<ATG::Help>(g_SampleTitle, g_SampleDescription, g_HelpButtons, _countof(g_HelpButtons));
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(HWND window, int width, int height)
{
    ResetWindow();

    m_gamePad = std::make_unique<GamePad>();

    m_keyboard = std::make_unique<Keyboard>();

    m_mouse = std::make_unique<Mouse>();

    m_deviceResources->SetWindow(window, width, height);

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
    float elapsedTime = float(timer.GetElapsedSeconds());

    m_renderFPS.Tick(elapsedTime);

    auto pad = m_gamePad->GetState(0);
    m_gamepadPresent = pad.IsConnected();
    if (m_gamepadPresent)
    {
        m_gamePadButtons.Update(pad);

        if (m_gamePadButtons.menu == GamePad::ButtonStateTracker::PRESSED)
        {
            m_showHelp = !m_showHelp;
        }
        else if (m_showHelp && m_gamePadButtons.b == GamePad::ButtonStateTracker::PRESSED)
        {
            m_showHelp = false;
        }

        if (!m_showHelp)
        {
            if (pad.IsViewPressed())
            {
                ExitSample();
            }

            const float ThumbLeftX = pad.thumbSticks.leftX;
            const float ThumbLeftY = pad.thumbSticks.leftY;
            const float ThumbRightY = pad.thumbSticks.rightY;
            const float RightTrigger = m_gamePadButtons.rightTrigger == DirectX::GamePad::ButtonStateTracker::HELD;

            if (m_gamePadButtons.y == DirectX::GamePad::ButtonStateTracker::PRESSED)
            {
                ResetWindow();
            }

            if (ThumbLeftX != 0.0f || ThumbLeftY != 0.0f || ThumbRightY != 0.0f)
            {
                const float ScaleSpeed = 1.0f + RightTrigger * 4.0f;
                const float WindowScale = 1.0f + ThumbRightY * -0.25f * ScaleSpeed * elapsedTime;
                m_window.x *= WindowScale;
                m_window.y *= WindowScale;
                m_window.z += m_window.x * ThumbLeftX * elapsedTime * 0.5f;
                m_window.w += m_window.y * ThumbLeftY * elapsedTime * 0.5f;
                m_windowUpdated = true;
            }

            m_windowUpdated = true;
        }
    }
    else
    {
        m_gamePadButtons.Reset();
    }

    auto kb = m_keyboard->GetState();
    m_keyboardButtons.Update(kb);

    if (m_keyboardButtons.IsKeyPressed(Keyboard::F1))
    {
        m_showHelp = !m_showHelp;
    }
    else if (m_showHelp && kb.Escape)
    {
        m_showHelp = false;
    }
    else
    {
        if (m_keyboardButtons.IsKeyPressed(Keyboard::Escape))
        {
            ExitSample();
        }

        if (m_keyboardButtons.IsKeyPressed(Keyboard::Home))
        {
            ResetWindow();
        }

        if (kb.W || kb.S || kb.A || kb.D || kb.PageUp || kb.PageDown)
        {
            const float ScaleSpeed = (kb.LeftShift || kb.RightShift) ? 4.f : 1.f;

            float zoom = kb.PageDown ? 1.f : (kb.PageUp ? -1.f : 0.f);
            float x = kb.D ? 1.f : (kb.A ? -1.f : 0.f);
            float y = kb.W ? 1.f : (kb.S ? -1.f : 0.f);

            const float WindowScale = 1.0f + zoom * ScaleSpeed * elapsedTime;
            m_window.x *= WindowScale;
            m_window.y *= WindowScale;
            m_window.z += m_window.x * x * elapsedTime * 0.5f;
            m_window.w += m_window.y * y * elapsedTime * 0.5f;
            m_windowUpdated = true;
        }
    }
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

    Clear();

    auto context = m_deviceResources->GetD3DDeviceContext();

    if (m_showHelp)
    {
        m_help->Render();
    }
    else
    {
        // Flip colors for which async compute buffer is being rendered
        m_deviceResources->PIXBeginEvent(L"Render");

        UpdateFractalData();

        context->CSSetConstantBuffers(0, 1, m_cbFractal.GetAddressOf());
        context->CSSetShaderResources(0, 1, m_fractalColorMapSRV.GetAddressOf());
        context->CSSetSamplers(0, 1, m_fractalBilinearSampler.GetAddressOf());
        context->CSSetShader(m_csFractal.Get(), nullptr, 0);
        context->CSSetUnorderedAccessViews(0, 1, m_fractalUAV.GetAddressOf(), nullptr);

        D3D11_TEXTURE2D_DESC texDesc = {};
        m_fractalTexture->GetDesc(&texDesc);

        const uint32_t threadGroupX = texDesc.Width / s_numShaderThreads;
        const uint32_t threadGroupY = texDesc.Height / s_numShaderThreads;
        context->Dispatch(threadGroupX, threadGroupY, 1);

        ID3D11UnorderedAccessView* nulluav[] = { nullptr };
        context->CSSetUnorderedAccessViews(0, 1, nulluav, nullptr);

        RECT outputSize = m_deviceResources->GetOutputSize();

        RECT safeRect = SimpleMath::Viewport::ComputeTitleSafeArea(outputSize.right, outputSize.bottom);
        XMFLOAT2 pos(float(safeRect.left), float(safeRect.top));

        m_spriteBatch->Begin();
        m_spriteBatch->Draw(m_fractalSRV.Get(), outputSize);

        wchar_t outputString[256] = {};
        swprintf_s(outputString, 256, L"Simple Compute Context %0.2f fps", m_renderFPS.GetFPS());

        m_font->DrawString(m_spriteBatch.get(), outputString, pos);
        pos.y += m_font->GetLineSpacing();

        swprintf_s(outputString, 256, L"Synchronous compute %0.2f fps", m_renderFPS.GetFPS());
        m_font->DrawString(m_spriteBatch.get(), outputString, pos);

        const wchar_t* legend = m_gamepadPresent
            ? L"[View] Exit   [Menu] Help"
            : L"WASD: Pan viewport   PageUp/Down: Zoom viewport   Esc: Exit";

        DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(),
            legend,
            XMFLOAT2(float(safeRect.left), float(safeRect.bottom) - m_font->GetLineSpacing()));

        m_spriteBatch->End();

        m_deviceResources->PIXEndEvent();
    }

    // Show the new frame.
    m_deviceResources->Present();
}

// Helper method to clear the back buffers.
void Sample::Clear()
{
    m_deviceResources->PIXBeginEvent(L"Clear");

    auto context = m_deviceResources->GetD3DDeviceContext();

    // Clear the views.
    auto renderTarget = m_deviceResources->GetRenderTargetView();

    context->ClearRenderTargetView(renderTarget, ATG::Colors::Background);

    context->OMSetRenderTargets(1, &renderTarget, nullptr);

    // Set the viewport.
    auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    m_deviceResources->PIXEndEvent();
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
}

void Sample::OnResuming()
{
    m_timer.ResetElapsedTime();
    m_gamePadButtons.Reset();
    m_keyboardButtons.Reset();
}

void Sample::OnWindowMoved()
{
}

void Sample::OnWindowSizeChanged(int width, int height)
{
    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;

    CreateWindowSizeDependentResources();
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

    auto blob = DX::ReadData(L"Fractal.cso");
    DX::ThrowIfFailed(
        device->CreateComputeShader(blob.data(), blob.size(), nullptr, m_csFractal.ReleaseAndGetAddressOf()));

    RECT outputSize = m_deviceResources->GetOutputSize();
    CD3D11_TEXTURE2D_DESC TexDesc(
        DXGI_FORMAT_R32G32B32A32_FLOAT,
        outputSize.right,
        outputSize.bottom,
        1,
        1,
        D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE
    );

    DX::ThrowIfFailed(
        device->CreateTexture2D(&TexDesc, nullptr, m_fractalTexture.ReleaseAndGetAddressOf()));

    DX::ThrowIfFailed(
        device->CreateShaderResourceView(m_fractalTexture.Get(), nullptr, m_fractalSRV.ReleaseAndGetAddressOf()));

    CD3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc(D3D11_UAV_DIMENSION_TEXTURE2D, TexDesc.Format);
    DX::ThrowIfFailed(
        device->CreateUnorderedAccessView(m_fractalTexture.Get(), &UAVDesc, m_fractalUAV.ReleaseAndGetAddressOf()));

    CD3D11_BUFFER_DESC CBDesc(sizeof(CB_FractalCS), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
    DX::ThrowIfFailed(
        device->CreateBuffer(&CBDesc, nullptr, m_cbFractal.ReleaseAndGetAddressOf()));

    TexDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    TexDesc.Width = 8;
    TexDesc.Height = 1;
    TexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    const uint32_t GradientTexels[8] = { 0xFF000040, 0xFF000080, 0xFF0000C0, 0xFF0000FF, 0xFF0040FF, 0xFF0080FF, 0xFF00C0FF, 0xFF00FFFF };

    D3D11_SUBRESOURCE_DATA InitData = {};
    InitData.SysMemPitch = sizeof(GradientTexels);
    InitData.pSysMem = GradientTexels;
    DX::ThrowIfFailed(
        device->CreateTexture2D(&TexDesc, &InitData, m_fractalColorMap.ReleaseAndGetAddressOf()));

    DX::ThrowIfFailed(
        device->CreateShaderResourceView(m_fractalColorMap.Get(), nullptr, m_fractalColorMapSRV.ReleaseAndGetAddressOf()));

    CD3D11_SAMPLER_DESC SamplerDesc(D3D11_DEFAULT);
    SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    DX::ThrowIfFailed(
        device->CreateSamplerState(&SamplerDesc, m_fractalBilinearSampler.ReleaseAndGetAddressOf()));

    m_fractalMaxIterations = 300;

    m_spriteBatch = std::make_unique<SpriteBatch>(context);

    wchar_t strFilePath[MAX_PATH] = {};
    DX::FindMediaFile(strFilePath, MAX_PATH, L"SegoeUI_18.spritefont");
    m_font = std::make_unique<SpriteFont>(device, strFilePath);

    DX::FindMediaFile(strFilePath, MAX_PATH, L"XboxOneControllerLegendSmall.spritefont");
    m_ctrlFont = std::make_unique<SpriteFont>(device, strFilePath);

    m_help->RestoreDevice(context);
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    auto size = m_deviceResources->GetOutputSize();
    m_help->SetWindow(size);

    m_spriteBatch->SetViewport(m_deviceResources->GetScreenViewport());
}

void Sample::OnDeviceLost()
{
    m_cbFractal.Reset();
    m_csFractal.Reset();
    m_fractalTexture.Reset();
    m_fractalUAV.Reset();
    m_fractalSRV.Reset();
    m_fractalColorMap.Reset();
    m_fractalColorMapSRV.Reset();
    m_fractalBilinearSampler.Reset();

    m_spriteBatch.reset();
    m_font.reset();
    m_ctrlFont.reset();

    m_help->ReleaseDevice();
}

void Sample::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion

void Sample::ResetWindow()
{
    m_window = XMFLOAT4(4.0f, 2.25f, -0.65f, 0.0f);
    m_windowUpdated = true;
}

//--------------------------------------------------------------------------------------
// Name: UpdateFractalData
// Desc: Updates the dynamic constant buffer with fractal data
//--------------------------------------------------------------------------------------
void Sample::UpdateFractalData()
{
    D3D11_TEXTURE2D_DESC texDesc = {};
    m_fractalTexture->GetDesc(&texDesc);

    auto context = m_deviceResources->GetD3DDeviceContext();
    D3D11_MAPPED_SUBRESOURCE mapped;

    DX::ThrowIfFailed(
        context->Map(m_cbFractal.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)
    );

    auto pCBFractalData = reinterpret_cast<CB_FractalCS*> (mapped.pData);

    pCBFractalData->MaxThreadIter = XMFLOAT4(static_cast<float>(texDesc.Width), static_cast<float>(texDesc.Height), static_cast<float>(m_fractalMaxIterations), 0);
    pCBFractalData->Window = m_window;

    context->Unmap(m_cbFractal.Get(), 0);
}

