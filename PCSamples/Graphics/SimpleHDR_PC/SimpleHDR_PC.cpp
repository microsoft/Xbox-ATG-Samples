//--------------------------------------------------------------------------------------
// SimpleHDR_PC.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimpleHDR_PC.h"

#include "HDR\HDRCommon.h"

#include "ControllerFont.h"
#include "ReadData.h"
#include "FindMedia.h"

extern void ExitSample();

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;
using DirectX::Colors::Black;
using DirectX::Colors::White;

namespace
{
    inline DirectX::XMVECTOR MakeColor(float value) { DirectX::XMVECTORF32 color = { value, value, value, 1.0f }; return color; }

    // Clamp value between 0 and 1
    inline float Clamp(float value)
    {
        return std::min(std::max(value, 0.0f), 1.0f);
    }

    // Applies the sRGB gamma curve to a linear value. This function is only used to output UI values
    float LinearToSRGB(float hdrSceneValue)
    {
        const float Cutoff = 0.0031308f;
        const float Linear = 12.92f;
        const float Scale = 1.055f;
        const float Bias = 0.055f;
        const float Gamma = 2.4f;
        const float InvGamma = 1.0f / Gamma;

        // clamp values [0..1]
        hdrSceneValue = Clamp(hdrSceneValue);

        // linear piece for dark values
        if (hdrSceneValue < Cutoff)
        {
            return hdrSceneValue * Linear;
        }

        return Scale * std::pow(hdrSceneValue, InvGamma) - Bias;
    }
}

Sample::Sample() :
    m_ctrlConnected(false),
    m_bRender2084Curve(false),
    m_bShowOnlyPaperWhite(true),
    m_countDownToBright(5),
    m_currentPaperWhiteNits(100.f),
    m_current2084CurveRenderingNits(500.0f),
    m_hdrSceneValues{ 0.5f, 1.0f, 6.0f, 10.0f }
{
    // Renders only 2D, so no need for a depth buffer.
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        DXGI_FORMAT_R10G10B10A2_UNORM,
        DXGI_FORMAT_UNKNOWN,
        2,
        D3D_FEATURE_LEVEL_10_0, // We use Direct3D Hardware Feature Level 10.0 as our minimum for this sample
        DX::DeviceResources::c_EnableHDR);
    m_deviceResources->RegisterDeviceNotify(this);
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(HWND window, int width, int height)
{
    m_gamePad = std::make_unique<GamePad>();

    m_keyboard = std::make_unique<Keyboard>();

    m_deviceResources->SetWindow(window, width, height);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    CreateHDRSceneResources();
    Create2084CurveResources();

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
    if (m_countDownToBright >= 0)
    {
        m_countDownToBright -= timer.GetElapsedSeconds();
        if (m_countDownToBright < 0)
        {
            m_bShowOnlyPaperWhite = false;
        }
    }

    const float c_fastNitsDelta = 25.0f;
    const float c_slowNitsDelta = 1.0f;
    const float c_fastSceneValueDelta = 0.05f;
    const float c_slowSceneValueDelta = 0.005f;

    bool nitsChanged = false;

    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_ctrlConnected = true;

        m_gamePadButtons.Update(pad);

        if (pad.IsViewPressed())
        {
            ExitSample();
        }

        if (m_gamePadButtons.a == GamePad::ButtonStateTracker::PRESSED)
        {
            m_bRender2084Curve = !m_bRender2084Curve;
        }

        if (m_gamePadButtons.b == GamePad::ButtonStateTracker::PRESSED)
        {
            m_bShowOnlyPaperWhite = !m_bShowOnlyPaperWhite;
        }

        if (m_gamePadButtons.dpadDown == GamePad::ButtonStateTracker::PRESSED ||
            m_gamePadButtons.dpadLeft == GamePad::ButtonStateTracker::PRESSED)
        {
            m_currentPaperWhiteNits -= 20.0f;;
            m_currentPaperWhiteNits = std::max(m_currentPaperWhiteNits, 80.0f);

            nitsChanged = true;
        }

        if (m_gamePadButtons.dpadUp == GamePad::ButtonStateTracker::PRESSED ||
            m_gamePadButtons.dpadRight == GamePad::ButtonStateTracker::PRESSED)
        {
            m_currentPaperWhiteNits += 20.0f;
            m_currentPaperWhiteNits = std::min(m_currentPaperWhiteNits, DX::c_MaxNitsFor2084);

            nitsChanged = true;
        }

        if (pad.IsLeftThumbStickDown() || pad.IsLeftThumbStickLeft())
        {
            if (m_bRender2084Curve)
            {
                m_current2084CurveRenderingNits -= c_fastNitsDelta;
                m_current2084CurveRenderingNits = std::max(m_current2084CurveRenderingNits, 0.0f);
            }
            else
            {
                m_hdrSceneValues[c_CustomInputValueIndex] -= c_fastSceneValueDelta;
                m_hdrSceneValues[c_CustomInputValueIndex] = std::max(m_hdrSceneValues[c_CustomInputValueIndex], 0.0f);
            }
        }

        if (pad.IsRightThumbStickDown() || pad.IsRightThumbStickLeft())
        {
            if (m_bRender2084Curve)
            {
                m_current2084CurveRenderingNits -= c_slowNitsDelta;
                m_current2084CurveRenderingNits = std::max(m_current2084CurveRenderingNits, 0.0f);
            }
            else
            {
                m_hdrSceneValues[c_CustomInputValueIndex] -= c_slowSceneValueDelta;
                m_hdrSceneValues[c_CustomInputValueIndex] = std::max(m_hdrSceneValues[c_CustomInputValueIndex], 0.0f);
            }
        }

        if (pad.IsLeftThumbStickUp() || pad.IsLeftThumbStickRight())
        {
            if (m_bRender2084Curve)
            {
                m_current2084CurveRenderingNits += c_fastNitsDelta;
                m_current2084CurveRenderingNits = std::min(m_current2084CurveRenderingNits, DX::c_MaxNitsFor2084);
            }
            else
            {
                m_hdrSceneValues[c_CustomInputValueIndex] += c_fastSceneValueDelta;
                m_hdrSceneValues[c_CustomInputValueIndex] = std::min(m_hdrSceneValues[c_CustomInputValueIndex], 125.0f);
            }
        }

        if (pad.IsRightThumbStickUp() || pad.IsRightThumbStickRight())
        {
            if (m_bRender2084Curve)
            {
                m_current2084CurveRenderingNits += c_slowNitsDelta;
                m_current2084CurveRenderingNits = std::min(m_current2084CurveRenderingNits, DX::c_MaxNitsFor2084);
            }
            else
            {
                m_hdrSceneValues[c_CustomInputValueIndex] += c_slowSceneValueDelta;
                m_hdrSceneValues[c_CustomInputValueIndex] = std::min(m_hdrSceneValues[c_CustomInputValueIndex], 125.0f);
            }
        }
    }
    else
    {
        m_ctrlConnected = false;

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
        m_bRender2084Curve = !m_bRender2084Curve;
    }

    if (m_keyboardButtons.IsKeyPressed(Keyboard::Enter))
    {
        m_bShowOnlyPaperWhite = !m_bShowOnlyPaperWhite;
    }

    if (m_keyboardButtons.IsKeyPressed(Keyboard::OemMinus) || m_keyboardButtons.IsKeyPressed(Keyboard::Subtract))
    {
        m_currentPaperWhiteNits -= 20.0f;
        m_currentPaperWhiteNits = std::max(m_currentPaperWhiteNits, 80.0f);

        nitsChanged = true;
    }

    if (m_keyboardButtons.IsKeyPressed(Keyboard::OemPlus) || m_keyboardButtons.IsKeyPressed(Keyboard::Add))
    {
        m_currentPaperWhiteNits += 20.0f;
        m_currentPaperWhiteNits = std::min(m_currentPaperWhiteNits, DX::c_MaxNitsFor2084);

        nitsChanged = true;
    }

    if (kb.Down || kb.Left)
    {
        if (m_bRender2084Curve)
        {
            m_current2084CurveRenderingNits -= (kb.LeftShift || kb.RightShift) ? c_slowNitsDelta : c_fastNitsDelta;
            m_current2084CurveRenderingNits = std::max(m_current2084CurveRenderingNits, 0.0f);
        }
        else
        {
            m_hdrSceneValues[c_CustomInputValueIndex] -= (kb.LeftShift || kb.RightShift) ? c_slowSceneValueDelta : c_fastSceneValueDelta;
            m_hdrSceneValues[c_CustomInputValueIndex] = std::max(m_hdrSceneValues[c_CustomInputValueIndex], 0.0f);
        }
    }

    if (kb.Up || kb.Right)
    {
        if (m_bRender2084Curve)
        {
            m_current2084CurveRenderingNits += (kb.LeftShift || kb.RightShift) ? c_slowNitsDelta : c_fastNitsDelta;
            m_current2084CurveRenderingNits = std::min(m_current2084CurveRenderingNits, DX::c_MaxNitsFor2084);
        }
        else
        {
            m_hdrSceneValues[c_CustomInputValueIndex] += (kb.LeftShift || kb.RightShift) ? c_slowSceneValueDelta : c_fastSceneValueDelta;
            m_hdrSceneValues[c_CustomInputValueIndex] = std::min(m_hdrSceneValues[c_CustomInputValueIndex], 125.0f);
        }
    }

    // Update constant buffer
    if (nitsChanged)
    {
        auto d3dContext = m_deviceResources->GetD3DDeviceContext();
        auto d3dBuffer = m_d3dNitsForPaperWhiteCB.Get();
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        DX::ThrowIfFailed(d3dContext->Map(d3dBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource));
        XMFLOAT4* pData = (XMFLOAT4*)mappedResource.pData;
        *pData = { m_currentPaperWhiteNits, 0.0f, 0.0f, 0.0f };
        d3dContext->Unmap(d3dBuffer, 0);
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

    m_deviceResources->PIXBeginEvent(L"Render");

    if (m_bRender2084Curve)
    {
        // Render the ST.2084 curve
        Render2084Curve();
    }
    else
    {
        // Render the HDR scene with values larger than 1.0f, which will be perceived as bright
        RenderHDRScene();
    }

    // Render the UI with values of 1.0f, which will be perceived as white
    RenderUI();

    // Process the HDR scene so that the swapchains can correctly be sent to HDR or SDR display
    PrepareSwapChainBuffer();

    m_deviceResources->PIXEndEvent();

    // Show the new frame.
    m_deviceResources->Present();
}

// Process the HDR scene so that the swapchains can correctly be sent to HDR or SDR display
void Sample::PrepareSwapChainBuffer()
{
    m_deviceResources->PIXBeginEvent(L"PrepareSwapChainBuffer");
    auto context = m_deviceResources->GetD3DDeviceContext();

    ID3D11RenderTargetView* pRTVs[] = { m_deviceResources->GetRenderTargetView() };
    context->OMSetRenderTargets(1, pRTVs, nullptr);

    auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    // Render a fullscreen quad and apply the HDR/SDR shaders
    if (m_deviceResources->GetColorSpace() == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
    {
        m_fullScreenQuad->Draw(context, *m_states, m_d3dHDRSceneSRV.Get(), [=]
        {
            context->PSSetShader(m_d3dPrepareSwapChainBuffersPS.Get(), nullptr, 0);
            context->PSSetConstantBuffers(0, 1, m_d3dNitsForPaperWhiteCB.GetAddressOf());
            context->PSSetSamplers(0, 1, m_d3dPointSampler.GetAddressOf());
        });
    }
    else
    {
        m_fullScreenQuad->Draw(context, *m_states, m_d3dHDRSceneSRV.Get(), [=]
        {
            context->PSSetShader(m_d3dTonemapSwapChainBufferPS.Get(), nullptr, 0);
            context->PSSetSamplers(0, 1, m_d3dPointSampler.GetAddressOf());
        });
    }

    ID3D11ShaderResourceView* nullrtv[] = { nullptr };
    context->PSSetShaderResources(0, _countof(nullrtv), nullrtv);

    m_deviceResources->PIXEndEvent();
}


// Render the HDR scene with four squares, each with a different HDR value. Values larger than 1.0f will be perceived as bright
void Sample::RenderHDRScene()
{
    auto context = m_deviceResources->GetD3DDeviceContext();

    auto viewportUI = m_deviceResources->GetScreenViewport();
    viewportUI.Width = 1920;
    viewportUI.Height = 1080;
    m_spriteBatch->SetViewport(viewportUI);

    m_deviceResources->PIXBeginEvent(L"RenderHDRScene");

    const long step = static_cast<long>(1920.0f / (c_NumInputValues + 2.0f));

    float startX = 115;

    RECT position = {};
    position.left = static_cast<long>(startX);

    // Render each block with the specific HDR scene value
    for (int i = 0; i < c_NumInputValues; i++)
    {
        XMVECTOR hdrSceneColor = MakeColor(m_hdrSceneValues[i]);

        m_spriteBatch->Begin(SpriteSortMode_Immediate, nullptr, nullptr, nullptr, nullptr, [=]
        {
            context->PSSetShader(m_d3dColorPS.Get(), nullptr, 0);
        });

        position.left += step;
        position.top = 485;
        position.right = position.left + static_cast<long>(step / 1.25f);
        position.bottom = position.top + 250;

        if (!m_bShowOnlyPaperWhite)
        {
            m_spriteBatch->Draw(m_defaultTex.Get(), position, hdrSceneColor);
        }
        else if (XMVector2Equal(hdrSceneColor, White))
        {
            m_spriteBatch->Draw(m_defaultTex.Get(), position, hdrSceneColor);
        }

        m_spriteBatch->End();
    }

    // Render the text 
    wchar_t strText[2048] = {};
    const float startY = 40.0f;
    const float fontScale = 0.75f;

    Vector2 fontPos;

    startX = 50.0f;

    m_spriteBatch->Begin();

    fontPos.x = startX;
    fontPos.y = startY + 270.0f;
    m_font->DrawString(m_spriteBatch.get(), L"HDR Scene Values", fontPos, White, 0.0f, g_XMZero, fontScale); fontPos.y += 40;
    m_font->DrawString(m_spriteBatch.get(), L"SDR sRGB Curve", fontPos, White, 0.0f, g_XMZero, fontScale); fontPos.y += 40;
    m_font->DrawString(m_spriteBatch.get(), L"HDR ST.2084 Curve", fontPos, White, 0.0f, g_XMZero, fontScale); fontPos.y += 40;
    m_font->DrawString(m_spriteBatch.get(), L"HDR Nits Output", fontPos, White, 0.0f, g_XMZero, fontScale);

    fontPos.x = startX + 100.0f;

    for (int i = 0; i < c_NumInputValues; i++)
    {
        float hdrSceneValue = m_hdrSceneValues[i];
        float sdrGamma = LinearToSRGB(hdrSceneValue);
        float hdr2084 = DX::LinearToST2084(hdrSceneValue, m_currentPaperWhiteNits);
        float hdrNits = DX::CalcNits(hdrSceneValue, m_currentPaperWhiteNits);

        fontPos.x += step;
        fontPos.y = startY + 270.0f;
        swprintf_s(strText, L"%f", hdrSceneValue);
        m_font->DrawString(m_spriteBatch.get(), strText, fontPos, White, 0.0f, g_XMZero, fontScale); fontPos.y += 40;
        swprintf_s(strText, L"%f", sdrGamma);
        m_font->DrawString(m_spriteBatch.get(), strText, fontPos, White, 0.0f, g_XMZero, fontScale); fontPos.y += 40;
        swprintf_s(strText, L"%f", hdr2084);
        m_font->DrawString(m_spriteBatch.get(), strText, fontPos, White, 0.0f, g_XMZero, fontScale); fontPos.y += 40;
        swprintf_s(strText, L"%f", hdrNits);
        m_font->DrawString(m_spriteBatch.get(), strText, fontPos, White, 0.0f, g_XMZero, fontScale); fontPos.y += 40;
    }

    fontPos.y = startY + 700.0f;
    fontPos.x = startX + 100.0f + step + step - 15.0f;
    m_font->DrawString(m_spriteBatch.get(), L"Paper White", fontPos, White, 0.0f, g_XMZero, fontScale); fontPos.x += step + 45;

    if (!m_bShowOnlyPaperWhite)
    {
        m_font->DrawString(m_spriteBatch.get(), L"Bright", fontPos, White, 0.0f, g_XMZero, fontScale);
    }

    m_spriteBatch->End();

    m_deviceResources->PIXEndEvent();
}


// Render the ST.2084 curve
void Sample::Render2084Curve()
{
    m_deviceResources->PIXBeginEvent(L"Render2084Curve");
    auto context = m_deviceResources->GetD3DDeviceContext();

    auto outputSize = m_deviceResources->GetOutputSize();
    float scale = (outputSize.bottom - outputSize.top) / 1080.0f;

    float viewportWidth = 1675 * scale;
    float viewportHeight = 600 * scale;
    float startX = 150.0f;
    float startY = 250.0f;

    auto viewport = m_deviceResources->GetScreenViewport();
    viewport.TopLeftX = startX * scale;
    viewport.TopLeftY = startY * scale;
    viewport.Width = viewportWidth;
    viewport.Height = viewportHeight;
    context->RSSetViewports(1, &viewport);

    Matrix proj = Matrix::CreateOrthographicOffCenter(0.0f, viewportWidth, viewportHeight, 0.0f, 0.0f, 1.0f);

    m_lineEffect->SetProjection(proj);
    context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_states->DepthNone(), 0);
    context->RSSetState(m_states->CullNone());
    m_lineEffect->Apply(context);
    context->IASetInputLayout(m_inputLayout.Get());
    m_primitiveBatch->Begin();

    // Render the outline
    m_primitiveBatch->DrawLine(VertexPositionColor(Vector3(0.5f, 0.5f, 0), White), VertexPositionColor(Vector3(viewportWidth, 0.5f, 0), White));
    m_primitiveBatch->DrawLine(VertexPositionColor(Vector3(0.5f, viewportHeight, 0), White), VertexPositionColor(Vector3(viewportWidth, viewportHeight, 0), White));
    m_primitiveBatch->DrawLine(VertexPositionColor(Vector3(0.5f, 0.5f, 0), White), VertexPositionColor(Vector3(0.5f, viewportHeight, 0), White));
    m_primitiveBatch->DrawLine(VertexPositionColor(Vector3(viewportWidth, 0.5f, 0), White), VertexPositionColor(Vector3(viewportWidth, viewportHeight, 0), White));

    // Render horizontal tick marks
    float numSteps = 16;
    for (int i = 0; i < numSteps; i++)
    {
        float x = (i * (viewportWidth / numSteps)) + 0.5f;
        float y = viewportHeight;
        m_primitiveBatch->DrawLine(VertexPositionColor(Vector3(x, y, 0), White), VertexPositionColor(Vector3(x, y - 10, 0), White));
    }

    // Render the graph
    for (int i = 0; i < viewportWidth; i++)
    {
        float x1 = static_cast<float>(i) + 0.5f;
        float normalizedLinearValue = static_cast<float>(i) / viewportWidth;
        float normalizedNonLinearValue = DX::LinearToST2084(normalizedLinearValue);
        float y1 = viewportHeight - (normalizedNonLinearValue * viewportHeight);

        float x2 = x1 + 1;
        normalizedLinearValue = static_cast<float>(x2) / viewportWidth;
        normalizedNonLinearValue = DX::LinearToST2084(normalizedLinearValue);
        float y2 = viewportHeight - (normalizedNonLinearValue * viewportHeight);

        m_primitiveBatch->DrawLine(VertexPositionColor(Vector3(x1, y1, 0), White), VertexPositionColor(Vector3(x2, y2, 0), White));
    }

    // Render the lines indication the current selection
    float normalizedLinearValue = m_current2084CurveRenderingNits / DX::c_MaxNitsFor2084;
    float normalizedNonLinearValue = DX::LinearToST2084(normalizedLinearValue);
    float x = normalizedLinearValue * viewportWidth;
    float y = viewportHeight - (normalizedNonLinearValue * viewportHeight);

    m_primitiveBatch->DrawLine(VertexPositionColor(Vector3(x, viewportHeight, 0), White), VertexPositionColor(Vector3(x, y, 0), White));
    m_primitiveBatch->DrawLine(VertexPositionColor(Vector3(x, y, 0), White), VertexPositionColor(Vector3(0, y, 0), White));

    m_primitiveBatch->End();

    // Restore viewport
    viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    auto viewportUI = m_deviceResources->GetScreenViewport();
    viewportUI.Width = 1920;
    viewportUI.Height = 1080;
    m_spriteBatch->SetViewport(viewportUI);

    // Render text
    viewportWidth /= scale;
    viewportHeight /= scale;

    wchar_t strText[2048] = {};
    Vector2 fontPos;
    m_spriteBatch->Begin();

    fontPos.x = startX - 100;
    fontPos.y = startY + viewportHeight + 5;
    m_font->DrawString(m_spriteBatch.get(), L"Linear", fontPos, White, 0.0f, g_XMZero, 0.4f); fontPos.y += 20;
    m_font->DrawString(m_spriteBatch.get(), L"Nits", fontPos, White, 0.0f, g_XMZero, 0.4f); fontPos.y += 20;
    m_font->DrawString(m_spriteBatch.get(), L"HDR Scene", fontPos, White, 0.0f, g_XMZero, 0.4f); fontPos.y += 20;

    fontPos.x = startX + viewportWidth - 5;
    fontPos.y = startY + viewportHeight + 5;
    m_font->DrawString(m_spriteBatch.get(), L"1.0", fontPos, White, 0.0f, g_XMZero, 0.4f); fontPos.y += 20;     // Always [0..1]
    m_font->DrawString(m_spriteBatch.get(), L"10K", fontPos, White, 0.0f, g_XMZero, 0.4f); fontPos.y += 20;     // Spec defines 10K nits

                                                                                                                // Max HDR scene value changes as white paper nits change
    float hdrSceneValue = DX::CalcHDRSceneValue(DX::c_MaxNitsFor2084, m_currentPaperWhiteNits);
    swprintf_s(strText, L"%1.0f", hdrSceneValue);
    m_font->DrawString(m_spriteBatch.get(), strText, fontPos, White, 0.0f, g_XMZero, 0.4f); fontPos.y += 20;

    normalizedLinearValue = m_current2084CurveRenderingNits / DX::c_MaxNitsFor2084;
    normalizedNonLinearValue = DX::LinearToST2084(normalizedLinearValue);
    hdrSceneValue = DX::CalcHDRSceneValue(m_current2084CurveRenderingNits, m_currentPaperWhiteNits);

    x = normalizedLinearValue * viewportWidth + 1;
    y = viewportHeight - (normalizedNonLinearValue * viewportHeight);

    fontPos.x = startX + x;
    fontPos.y = startY + viewportHeight + 5;
    swprintf_s(strText, L"%1.2f", normalizedLinearValue);
    m_font->DrawString(m_spriteBatch.get(), strText, fontPos, White, 0.0f, g_XMZero, 0.4f); fontPos.y += 20;
    swprintf_s(strText, L"%1.0f", m_current2084CurveRenderingNits);
    m_font->DrawString(m_spriteBatch.get(), strText, fontPos, White, 0.0f, g_XMZero, 0.4f); fontPos.y += 20;
    swprintf_s(strText, L"%1.2f", hdrSceneValue);
    m_font->DrawString(m_spriteBatch.get(), strText, fontPos, White, 0.0f, g_XMZero, 0.4f); fontPos.y += 20;

    fontPos.x = startX - 25;
    fontPos.y = startY - 50;
    m_font->DrawString(m_spriteBatch.get(), L"ST.2084", fontPos, White, -XM_PIDIV2, g_XMZero, 0.4f); fontPos.x -= 20;
    m_font->DrawString(m_spriteBatch.get(), L"Nits", fontPos, White, -XM_PIDIV2, g_XMZero, 0.4f);

    fontPos.x = startX - 25;
    fontPos.y = y + startY;
    swprintf_s(strText, L"%1.2f", normalizedNonLinearValue);
    m_font->DrawString(m_spriteBatch.get(), strText, fontPos, White, -XM_PIDIV2, g_XMZero, 0.4f); fontPos.x -= 20;
    swprintf_s(strText, L"%1.0f", m_current2084CurveRenderingNits);
    m_font->DrawString(m_spriteBatch.get(), strText, fontPos, White, -XM_PIDIV2, g_XMZero, 0.4f);

    m_spriteBatch->End();

    // Render blocks
    const long size = 150;
    RECT position = {};
    position.left = 1920 - size * 4;
    position.top = 50;
    position.right = position.left + size;
    position.bottom = position.top + size;

    m_spriteBatch->Begin(SpriteSortMode_Immediate, nullptr, nullptr, nullptr, nullptr, [=]
    {
        context->PSSetShader(m_d3dColorPS.Get(), nullptr, 0);
    });

    m_spriteBatch->Draw(m_d3dHDRSceneSRV.Get(), position, White);

    position.left += size * 2;
    position.right = position.left + size;

    XMVECTORF32 color = { hdrSceneValue, hdrSceneValue, hdrSceneValue, 1.0f };
    m_spriteBatch->Draw(m_d3dHDRSceneSRV.Get(), position, color);

    m_spriteBatch->End();

    // Render text for blocks
    m_spriteBatch->Begin();

    fontPos.x = 1920 - size * 4 - 25;
    fontPos.y = static_cast<float>(position.bottom - 15);

    m_font->DrawString(m_spriteBatch.get(), L"Paper White", fontPos, White, -XM_PIDIV2, g_XMZero, 0.4f);

    fontPos.x = 1920 - size * 4 + 25;
    fontPos.y = static_cast<float>(position.bottom);

    swprintf_s(strText, L"%1.0f nits", m_currentPaperWhiteNits);
    m_font->DrawString(m_spriteBatch.get(), strText, fontPos, White, 0.0f, g_XMZero, 0.4f); fontPos.x += size * 2;
    swprintf_s(strText, L"%1.0f nits", m_current2084CurveRenderingNits);
    m_font->DrawString(m_spriteBatch.get(), strText, fontPos, White, 0.0f, g_XMZero, 0.4f);

    m_spriteBatch->End();

    m_deviceResources->PIXEndEvent();
}


// Render the UI
void Sample::RenderUI()
{
    m_deviceResources->PIXBeginEvent(L"RenderUI");

    auto viewportUI = m_deviceResources->GetScreenViewport();
    viewportUI.Width = 1920;
    viewportUI.Height = 1080;
    m_spriteBatch->SetViewport(viewportUI);

    const float startX = 50.0f;
    const float startY = 40.0f;
    const float fontScale = 0.75f;

    Vector2 fontPos(startX, startY);

    m_spriteBatch->Begin();
    m_font->DrawString(m_spriteBatch.get(), L"SimpleHDR Sample for DirectX 11", fontPos, White, 0.0f, g_XMZero, 1.0f);

    if (!m_bRender2084Curve)
    {
        fontPos.y = startY + 100.0f;
        if (m_deviceResources->GetColorSpace() == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
        {
            m_font->DrawString(m_spriteBatch.get(), L"TV in HDR Mode: TRUE", fontPos, White, 0.0f, g_XMZero, fontScale);
        }
        else
        {
            m_font->DrawString(m_spriteBatch.get(), L"TV in HDR Mode: FALSE", fontPos, White, 0.0f, g_XMZero, fontScale);
        }
    }

    if (m_ctrlConnected)
    {
        fontPos.x = startX;
        fontPos.y = 955;
        DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_controllerFont.get(), L"[A] - Toggle displaying ST.2084 curve", fontPos, White, 0.65f); fontPos.y += 35;
        DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_controllerFont.get(), L"[B] - Toggle displaying only paper white block", fontPos, White, 0.65f); fontPos.y += 35;

        fontPos.x = (1920.0f / 2.0f) + startX;
        fontPos.y = 955;
        DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_controllerFont.get(), L"[DPad] - Adjust paper white nits", fontPos, White, 0.65f); fontPos.y += 35;
        DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_controllerFont.get(), L"[LThumb] - Adjust values quickly", fontPos, White, 0.65f); fontPos.y += 35;
        DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_controllerFont.get(), L"[RThumb] - Adjust values slowly", fontPos, White, 0.65f); fontPos.y += 35;
    }
    else
    {
        fontPos.x = startX;
        fontPos.y = 955;
        DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_controllerFont.get(), L"Space - Toggle displaying ST.2084 curve", fontPos, White, 0.65f); fontPos.y += 35;
        DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_controllerFont.get(), L"Enter - Toggle displaying only paper white block", fontPos, White, 0.65f); fontPos.y += 35;

        fontPos.x = (1920.0f / 2.0f) + startX;
        fontPos.y = 955;
        DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_controllerFont.get(), L"+ / - - Adjust paper white nits", fontPos, White, 0.65f); fontPos.y += 35;
        DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_controllerFont.get(), L"Up/Down - Adjust values quickly", fontPos, White, 0.65f); fontPos.y += 35;
        DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_controllerFont.get(), L"Shift + Up/Down - Adjust values slowly", fontPos, White, 0.65f); fontPos.y += 35;
    }

    if (m_countDownToBright >= 0)
    {
        fontPos.x = 1170;
        fontPos.y = 550;

        wchar_t strText[2048] = {};
        swprintf_s(strText, L"%1.0f", m_countDownToBright);
        m_font->DrawString(m_spriteBatch.get(), strText, fontPos, White, 0.0f, g_XMZero, 1.75f);
    }

    m_spriteBatch->End();

    m_deviceResources->PIXEndEvent();
}

// Helper method to clear the back buffers.
void Sample::Clear()
{
    m_deviceResources->PIXBeginEvent(L"Clear");
    auto context = m_deviceResources->GetD3DDeviceContext();

    // Clear the views.
    auto pHDRSceneRTV = m_d3dHDRSceneRTV.Get();

    // Use linear clear color for gamma-correct rendering.
    context->ClearRenderTargetView(pHDRSceneRTV, Black);

    context->OMSetRenderTargets(1, &pHDRSceneRTV, nullptr);

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
    width = 1920;
    height = 1080;
}
#pragma endregion

#pragma region Direct3D Resources
// Create shaders, buffers, etc. for rendering 2084 curve
void Sample::Create2084CurveResources()
{
    auto device = m_deviceResources->GetD3DDevice();

    {
        auto pixelShaderBlob = DX::ReadData(L"PrepareSwapChainBuffersPS.cso");
        DX::ThrowIfFailed(
            device->CreatePixelShader(pixelShaderBlob.data(), pixelShaderBlob.size(), nullptr, m_d3dPrepareSwapChainBuffersPS.ReleaseAndGetAddressOf())
        );
    }

    {
        auto pixelShaderBlob = DX::ReadData(L"ToneMapSDRPS.cso");
        DX::ThrowIfFailed(
            device->CreatePixelShader(pixelShaderBlob.data(), pixelShaderBlob.size(), nullptr, m_d3dTonemapSwapChainBufferPS.ReleaseAndGetAddressOf())
        );
    }

    // Point sampler
    D3D11_SAMPLER_DESC samplerDesc;
    ZeroMemory(&samplerDesc, sizeof(samplerDesc));
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.MaxAnisotropy = 1;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    DX::ThrowIfFailed(device->CreateSamplerState(&samplerDesc, m_d3dPointSampler.ReleaseAndGetAddressOf()));

    // Constant buffer for setting nits for paper white
    XMFLOAT4 data = { m_currentPaperWhiteNits, 0.0f, 0.0f, 0.0f };
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = &data;

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.ByteWidth = (sizeof(XMFLOAT4) + 15) & ~15;     // has to be a multiple of 16 otherwise D3D is unhappy
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    DX::ThrowIfFailed(device->CreateBuffer(&bufferDesc, &initData, m_d3dNitsForPaperWhiteCB.ReleaseAndGetAddressOf()));
    SetDebugObjectName(m_d3dNitsForPaperWhiteCB.Get(), "NitsForPaperWhite");
}

// Create resource for HDR scene rendering, i.e. not the swapchains, etc.
void Sample::CreateHDRSceneResources()
{
    auto device = m_deviceResources->GetD3DDevice();

    auto pixelShaderBlob = DX::ReadData(L"ColorPS.cso");
    DX::ThrowIfFailed(device->CreatePixelShader(pixelShaderBlob.data(), pixelShaderBlob.size(), nullptr, m_d3dColorPS.ReleaseAndGetAddressOf()));
}

// These are the resources that depend on the device.
void Sample::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto context = m_deviceResources->GetD3DDeviceContext();

    m_fullScreenQuad = std::make_unique<DX::FullScreenQuad>();
    m_fullScreenQuad->Initialize(device);
    
    wchar_t strFilePath[MAX_PATH] = {};
    DX::FindMediaFile(strFilePath, MAX_PATH, L"Media\\Fonts\\Courier_36.spritefont");
    m_font = std::make_unique<SpriteFont>(device, strFilePath);
    
    DX::FindMediaFile(strFilePath, MAX_PATH, L"Media\\Fonts\\XboxOneControllerLegendSmall.spritefont");
    m_controllerFont = std::make_unique<SpriteFont>(device, strFilePath);
    
    m_spriteBatch = std::make_unique<SpriteBatch>(context);

    m_states = std::make_unique<CommonStates>(device);
    m_lineEffect = std::make_unique<BasicEffect>(device);
    m_lineEffect->SetVertexColorEnabled(true);

    {
        void const* shaderByteCode;
        size_t byteCodeLength;
        m_lineEffect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);

        DX::ThrowIfFailed(device->CreateInputLayout(
            VertexPositionColor::InputElements, VertexPositionColor::InputElementCount,
            shaderByteCode, byteCodeLength, m_inputLayout.ReleaseAndGetAddressOf())
        );
    }

    m_primitiveBatch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(context);

    // Create 1x1 white default textue
    {
        static const uint32_t s_pixel = 0xffffffff;

        D3D11_SUBRESOURCE_DATA initData = { &s_pixel, sizeof(uint32_t), 0 };

        CD3D11_TEXTURE2D_DESC texDesc(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_IMMUTABLE);

        ComPtr<ID3D11Texture2D> tex;
        DX::ThrowIfFailed(device->CreateTexture2D(&texDesc, &initData, tex.GetAddressOf()));

        DX::ThrowIfFailed(device->CreateShaderResourceView(tex.Get(), nullptr, m_defaultTex.ReleaseAndGetAddressOf()));
    }
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();

    auto outputSize = m_deviceResources->GetOutputSize();
    int width = outputSize.right - outputSize.left;
    int height = outputSize.bottom - outputSize.top;

    CD3D11_TEXTURE2D_DESC descTex(DXGI_FORMAT_R16G16B16A16_FLOAT, width, height, 1, 1, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    DX::ThrowIfFailed(device->CreateTexture2D(&descTex, nullptr, m_d3dHDRSceneTexture.ReleaseAndGetAddressOf()));

    CD3D11_RENDER_TARGET_VIEW_DESC descRTV(D3D11_RTV_DIMENSION_TEXTURE2D, descTex.Format);
    DX::ThrowIfFailed(device->CreateRenderTargetView(m_d3dHDRSceneTexture.Get(), &descRTV, m_d3dHDRSceneRTV.ReleaseAndGetAddressOf()));

    CD3D11_SHADER_RESOURCE_VIEW_DESC descSRV(D3D11_SRV_DIMENSION_TEXTURE2D, descTex.Format, 0, 1);
    DX::ThrowIfFailed(device->CreateShaderResourceView(m_d3dHDRSceneTexture.Get(), &descSRV, m_d3dHDRSceneSRV.ReleaseAndGetAddressOf()));
    
    // Reset the counter
    if (!m_bRender2084Curve)
    {
        m_countDownToBright = 5;
        m_bShowOnlyPaperWhite = true;
    }
}

void Sample::OnDeviceLost()
{
    m_font.reset();
    m_controllerFont.reset();
    m_spriteBatch.reset();
    m_states.reset();
    m_lineEffect.reset();
    m_inputLayout.Reset();
    m_primitiveBatch.reset();

    m_d3dColorPS.Reset();
    m_d3dHDRSceneTexture.Reset();
    m_d3dHDRSceneRTV.Reset();
    m_d3dHDRSceneSRV.Reset();
    m_defaultTex.Reset();

    m_d3dPointSampler.Reset();
    m_d3dPrepareSwapChainBuffersPS.Reset();
    m_d3dTonemapSwapChainBufferPS.Reset();
    m_d3dNitsForPaperWhiteCB.Reset();

    m_fullScreenQuad->ReleaseDevice();
}

void Sample::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
