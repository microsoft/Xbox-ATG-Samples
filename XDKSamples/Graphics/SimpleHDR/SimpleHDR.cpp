//--------------------------------------------------------------------------------------
// SimpleHDR.cpp
//
// This sample uses an API to determine if the attached display is HDR capable. If so, it
// will switch the display to HDR mode. A very simple HDR scene, with values above 1.0f,
// is rendered to a FP16 backbuffer and outputs to two different swapchains, one for HDR
// and one for SDR. Even if the consumer uses an HDR display, the SDR signal is still
// required for GameDVR and screenshots.
//
// Requirements for swapchain creation:
//  1) HDR swapchain has to be 10bit using DXGIX_SWAP_CHAIN_FLAG_COLORIMETRY_RGB_BT2020_ST2084
//  2) SDR swapchain has to use DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
// See DeviceResources.cpp for swapchain creation and presentation
//
// Refer to the white paper "HDR on Xbox One"
//
// Up to now, games were outputting and SDR signal using Rec.709 color primaries and Rec.709
// gamma curve. One new feature of UHD displays is a wider color gamut (WCG). To use this
// we need to use a new color space, Rec.2020 color primaries. Another new feature of UHD
// displays is high dynamic range (HDR). To use this we need to use a different curve, the
// ST.2084 curve. Therefore, to output an HDR signal, it needs to use Rec.2020 color primaries
// with ST.2084 curve.
//
// For displaying the SDR signal, a simple tonemapping shader is applied to simply clip all
// values above 1.0f in the HDR scene, and outputs 8bit values using Rec.709 color primaries,
// with the gamma curve applied.
//
// For displaying the HDR signal, a shader is used to rotate the Rec.709 color primaries to
// Rec.2020 color primaries, and then apply the ST.2084 curve to output 10bit values which
// the HDR display can correctly display. The whiteness and brightness of the output on an
// HDR display will be determined by the selected nits value for defining "paper white".
// SDR specs define "paper white" as 80nits, but this is for a cinema with a dark environment.
// Consumers today are used to much brighter whites, e.g. ~550 nits for a smartphone(so that it
// can be viewed in sunlight), 200-300 nits for a PC monitor, 120-150 nits for an SDR TV, etc.
// The nits for "paper white" can be adjusted in the sample using the DPad up/down. Displaying
// bright values next to white can be deceiving to the eye, so you can use the A button to
// toggle if you only want to see the "paper white" block.
//
// The sample has two modes:
//  1) Render blocks with specific values in the scene
//	2) Render ST.2084 curve with specific brightness values(nits)
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimpleHDR.h"

#include "HDR\HDRCommon.h"

#include "ReadData.h"
#include "ControllerFont.h"

#include <ppltasks.h>

extern void ExitSample();

using namespace DX;
using namespace DirectX;
using namespace SimpleMath;

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
    m_frame(0),
    m_bRender2084Curve(false),
    m_bShowOnlyPaperWhite(true),
    m_countDownToBright(5),
    m_currentPaperWhiteNits(100.f),
    m_current2084CurveRenderingNits(500.0f),
    m_hdrSceneValues{ 0.5f, 1.0f, 6.0f, 10.0f }
{
    // Renders only 2D, so no need for a depth buffer.
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        DXGI_FORMAT_B8G8R8A8_UNORM_SRGB /* SDR swapchain format */,
        DXGI_FORMAT_UNKNOWN,
        2,
        DX::DeviceResources::c_Enable4K_UHD);
}

#pragma region HDR

// Initialize, including trying to set the display to HDR mode
void Sample::Initialize(IUnknown* window)
{
    m_bIsTVInHDRMode = false;

    // Determine if attached display is HDR or SDR, if HDR, also set the TV in HDR mode. This is an async operation,
    // so we can do other initalizations in the meanwhile
    auto determineHDRAction = Windows::Xbox::Graphics::Display::DisplayConfiguration::TrySetHdrModeAsync();
    auto determineHDRTask = Concurrency::create_task(determineHDRAction);

    // Regular sample initialization
    Init(window);
    
    // Now wait until we know if display is in HDR mode
    try
    {
        // .get() will automatically .wait() until the async operation is done
        auto results = determineHDRTask.get();
        m_bIsTVInHDRMode = results->HdrEnabled;
    }
    catch (Platform::Exception^ e)
    {
        OutputDebugStringW(e->Message->Data());
        throw;
    }
}


// Render
void Sample::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    auto d3dContext = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(d3dContext, PIX_COLOR_DEFAULT, L"Render");

    // Prepare the render target to render a new frame.
    m_deviceResources->Prepare();
    Clear();

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
    PrepareSwapChainBuffers();

    PIXEndEvent(d3dContext);

    // Show the new frame.
    PIXBeginEvent(d3dContext, PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present(m_bIsTVInHDRMode);
    m_graphicsMemory->Commit();
    PIXEndEvent(d3dContext);
}


// Process the HDR scene so that the swapchains can correctly be sent to HDR or SDR display
void Sample::PrepareSwapChainBuffers()
{
    auto d3dContext = m_deviceResources->GetD3DDeviceContext();

    PIXBeginEvent(d3dContext, PIX_COLOR_DEFAULT, L"PrepareSwapChainBuffers");

    ID3D11RenderTargetView* pRTVs[] =
    {
        m_deviceResources->GetHDR10RenderTargetView(),  // Writes out the HDR scene values with ST.2084 into HDR swapchain
        m_deviceResources->GetGameDVRRenderTargetView() // Writes out the HDR tonemapped values into SDR swapchain, then display HW applies gamma curve
    };
    d3dContext->OMSetRenderTargets(2, pRTVs, nullptr);

    auto viewport = m_deviceResources->GetScreenViewport();
    d3dContext->RSSetViewports(1, &viewport);

    // Render a fullscreen quad and apply the HDR/SDR shaders
    m_fullScreenQuad->Draw(d3dContext, *m_states, m_d3dHDRSceneSRV.Get(), [=]
    {
        d3dContext->PSSetShader(m_d3dPrepareSwapChainBuffersPS.Get(), nullptr, 0);
        d3dContext->PSSetConstantBuffers(0, 1, m_d3dNitsForPaperWhiteCB.GetAddressOf());
        d3dContext->PSSetSamplers(0, 1, m_d3dPointSampler.GetAddressOf());

    });

    PIXEndEvent(d3dContext);
}


// Render the HDR scene with four squares, each with a different HDR value. Values larger than 1.0f will be perceived as bright
void Sample::RenderHDRScene()
{
    auto d3dContext = m_deviceResources->GetD3DDeviceContext();

    auto viewportUI = m_deviceResources->GetScreenViewport();
    viewportUI.Width = 1920;
    viewportUI.Height = 1080;
    m_spriteBatch->SetViewport(viewportUI);

    PIXBeginEvent(d3dContext, PIX_COLOR_DEFAULT, L"RenderHDRScene");

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
            d3dContext->PSSetShader(m_d3dColorPS.Get(), nullptr, 0);
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
        float hdr2084 = LinearToST2084(hdrSceneValue, m_currentPaperWhiteNits);
        float hdrNits = CalcNits(hdrSceneValue, m_currentPaperWhiteNits);

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

    PIXEndEvent(d3dContext);
}


// Render the ST.2084 curve
void Sample::Render2084Curve()
{
    auto d3dContext = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(d3dContext, PIX_COLOR_DEFAULT, L"Render2084Curve");

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
    d3dContext->RSSetViewports(1, &viewport);

    Matrix proj = Matrix::CreateOrthographicOffCenter(0.0f, viewportWidth, viewportHeight, 0.0f, 0.0f, 1.0f);

    m_lineEffect->SetProjection(proj);
    d3dContext->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
    d3dContext->OMSetDepthStencilState(m_states->DepthNone(), 0);
    d3dContext->RSSetState(m_states->CullNone());
    m_lineEffect->Apply(d3dContext);
    d3dContext->IASetInputLayout(m_inputLayout.Get());
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
        float normalizedNonLinearValue = LinearToST2084(normalizedLinearValue);
        float y1 = viewportHeight - (normalizedNonLinearValue * viewportHeight);

        float x2 = x1 + 1;
        normalizedLinearValue = static_cast<float>(x2) / viewportWidth;
        normalizedNonLinearValue = LinearToST2084(normalizedLinearValue);
        float y2 = viewportHeight - (normalizedNonLinearValue * viewportHeight);

        m_primitiveBatch->DrawLine(VertexPositionColor(Vector3(x1, y1, 0), White), VertexPositionColor(Vector3(x2, y2, 0), White));
    }

    // Render the lines indication the current selection
    float normalizedLinearValue = m_current2084CurveRenderingNits / c_MaxNitsFor2084;
    float normalizedNonLinearValue = LinearToST2084(normalizedLinearValue);
    float x = normalizedLinearValue * viewportWidth;
    float y = viewportHeight - (normalizedNonLinearValue * viewportHeight);

    m_primitiveBatch->DrawLine(VertexPositionColor(Vector3(x, viewportHeight, 0), White), VertexPositionColor(Vector3(x, y, 0), White));
    m_primitiveBatch->DrawLine(VertexPositionColor(Vector3(x, y, 0), White), VertexPositionColor(Vector3(0, y, 0), White));

    m_primitiveBatch->End();

    // Restore viewport
    viewport = m_deviceResources->GetScreenViewport();
    d3dContext->RSSetViewports(1, &viewport);

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
    float hdrSceneValue = CalcHDRSceneValue(c_MaxNitsFor2084, m_currentPaperWhiteNits);
    swprintf_s(strText, L"%1.0f", hdrSceneValue);
    m_font->DrawString(m_spriteBatch.get(), strText, fontPos, White, 0.0f, g_XMZero, 0.4f); fontPos.y += 20;

    normalizedLinearValue = m_current2084CurveRenderingNits / c_MaxNitsFor2084;
    normalizedNonLinearValue = LinearToST2084(normalizedLinearValue);
    hdrSceneValue = CalcHDRSceneValue(m_current2084CurveRenderingNits, m_currentPaperWhiteNits);

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
        d3dContext->PSSetShader(m_d3dColorPS.Get(), nullptr, 0);
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

    PIXEndEvent(d3dContext); 
}


// Render the UI
void Sample::RenderUI()
{
    auto d3dContext = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(d3dContext, PIX_COLOR_DEFAULT, L"RenderUI");

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
        if (m_bIsTVInHDRMode)
        {
            m_font->DrawString(m_spriteBatch.get(), L"TV in HDR Mode: TRUE", fontPos, White, 0.0f, g_XMZero, fontScale);
        }
        else
        {
            m_font->DrawString(m_spriteBatch.get(), L"TV in HDR Mode: FALSE", fontPos, White, 0.0f, g_XMZero, fontScale);
        }
    }

    fontPos.x = startX;
    fontPos.y = 955;
    DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_controllerFont.get(), L"[A] - Toggle displaying ST.2084 curve", fontPos, White, 0.65f); fontPos.y += 35;
    DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_controllerFont.get(), L"[B] - Toggle displaying only paper white block", fontPos, White, 0.65f); fontPos.y += 35;

    fontPos.x = (1920.0f/2.0f) + startX;
    fontPos.y = 955;
    DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_controllerFont.get(), L"[DPad] - Adjust paper white nits", fontPos, White, 0.65f); fontPos.y += 35;
    DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_controllerFont.get(), L"[LThumb] - Adjust values quickly", fontPos, White, 0.65f); fontPos.y += 35;
    DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_controllerFont.get(), L"[RThumb] - Adjust values slowly", fontPos, White, 0.65f); fontPos.y += 35;

    if (m_countDownToBright >= 0)
    {
        fontPos.x = 1170;
        fontPos.y = 550;

        wchar_t strText[2048] = {};
        swprintf_s(strText, L"%1.0f", m_countDownToBright);
        m_font->DrawString(m_spriteBatch.get(), strText, fontPos, White, 0.0f, g_XMZero, 1.75f);
    }

    m_spriteBatch->End();

    PIXEndEvent(d3dContext);
}


// Clear scene
void Sample::Clear()
{
    auto d3dContext = m_deviceResources->GetD3DDeviceContext();
    auto pHDRSceneRTV = m_d3dHDRSceneRTV.Get();

    PIXBeginEvent(d3dContext, PIX_COLOR_DEFAULT, L"Clear");

    d3dContext->ClearRenderTargetView(pHDRSceneRTV, Colors::Black);

    d3dContext->OMSetRenderTargets(1, &pHDRSceneRTV, nullptr);

    auto viewport = m_deviceResources->GetScreenViewport();
    d3dContext->RSSetViewports(1, &viewport);

    PIXEndEvent(d3dContext);
}

#pragma endregion

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
void Sample::Update(DX::StepTimer const& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    static bool bCountingDownToBright = true;
    m_countDownToBright -= timer.GetElapsedSeconds();
    if (bCountingDownToBright && (m_countDownToBright < 0))
    {
        bCountingDownToBright = false;
        m_bShowOnlyPaperWhite = false;
    }

    auto gamepad = m_gamePad->GetState(0);
    
    bool nitsChanged = false;

    if (gamepad.IsConnected())
    {
        m_gamePadButtons.Update(gamepad);

        if (gamepad.IsViewPressed())
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
            m_currentPaperWhiteNits -= 20.0f;
            m_currentPaperWhiteNits = std::max(m_currentPaperWhiteNits, 80.0f);

            nitsChanged = true;
        }

        if (m_gamePadButtons.dpadUp == GamePad::ButtonStateTracker::PRESSED ||
            m_gamePadButtons.dpadRight == GamePad::ButtonStateTracker::PRESSED)
        {
            m_currentPaperWhiteNits += 20.0f;
            m_currentPaperWhiteNits = std::min(m_currentPaperWhiteNits, c_MaxNitsFor2084);

            nitsChanged = true;
        }

        const float c_fastNitsDelta = 25.0f;
        const float c_slowNitsDelta = 1.0f;
        const float c_fastSceneValueDelta = 0.05f;
        const float c_slowSceneValueDelta = 0.005f;

        if (gamepad.IsLeftThumbStickDown() || gamepad.IsLeftThumbStickLeft())
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

        if ( gamepad.IsRightThumbStickDown() || gamepad.IsRightThumbStickLeft())
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

        if (gamepad.IsLeftThumbStickUp() || gamepad.IsLeftThumbStickRight())
        {
            if (m_bRender2084Curve)
            {
                m_current2084CurveRenderingNits += c_fastNitsDelta;
                m_current2084CurveRenderingNits = std::min(m_current2084CurveRenderingNits, c_MaxNitsFor2084);
            }
            else
            {
                m_hdrSceneValues[c_CustomInputValueIndex] += c_fastSceneValueDelta;
                m_hdrSceneValues[c_CustomInputValueIndex] = std::min(m_hdrSceneValues[c_CustomInputValueIndex], 125.0f);
            }
        }

        if (gamepad.IsRightThumbStickUp() || gamepad.IsRightThumbStickRight())
        {
            if (m_bRender2084Curve)
            {
                m_current2084CurveRenderingNits += c_slowNitsDelta;
                m_current2084CurveRenderingNits = std::min(m_current2084CurveRenderingNits, c_MaxNitsFor2084);
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
        m_gamePadButtons.Reset();
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

    PIXEndEvent();
}

#pragma endregion


#pragma region Direct3D Resources
// Initialize the Direct3D resources required to run.
void Sample::Init(IUnknown* window)
{
    m_gamePad = std::make_unique<GamePad>();
    m_deviceResources->SetWindow(window);
    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();
    m_deviceResources->CreateWindowSizeDependentResources();

    CreateHDRSceneResources();
    Create2084CurveResources();
}


// Create shaders, buffers, etc. for rendering 2084 curve
void Sample::Create2084CurveResources()
{
    auto d3dDevice = m_deviceResources->GetD3DDevice();
    auto pixelShaderBlob = DX::ReadData(L"PrepareSwapChainBuffersPS.cso");
    DX::ThrowIfFailed(d3dDevice->CreatePixelShader(pixelShaderBlob.data(), pixelShaderBlob.size(), nullptr, m_d3dPrepareSwapChainBuffersPS.ReleaseAndGetAddressOf()));

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
    DX::ThrowIfFailed(d3dDevice->CreateSamplerState(&samplerDesc, m_d3dPointSampler.ReleaseAndGetAddressOf()));

    // Constant buffer for setting nits for paper white
    XMFLOAT4 data = { m_currentPaperWhiteNits, 0.0f, 0.0f, 0.0f };
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = &data;

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.ByteWidth = (sizeof(XMFLOAT4) + 15) & ~15;     // has to be a multiple of 16 otherwise D3D is unhappy
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    DX::ThrowIfFailed(d3dDevice->CreateBuffer(&bufferDesc, &initData, m_d3dNitsForPaperWhiteCB.ReleaseAndGetAddressOf()));
    m_d3dNitsForPaperWhiteCB->SetName(L"NitsForPaperWhite");
}


// Create resource for HDR scene rendering, i.e. not the swapchains, etc.
void Sample::CreateHDRSceneResources()
{
    auto d3dDevice = m_deviceResources->GetD3DDevice();

    auto outputSize = m_deviceResources->GetOutputSize();
    int width = outputSize.right - outputSize.left;
    int height = outputSize.bottom - outputSize.top;
    
    auto pixelShaderBlob = DX::ReadData(L"ColorPS.cso");
    DX::ThrowIfFailed(d3dDevice->CreatePixelShader(pixelShaderBlob.data(), pixelShaderBlob.size(), nullptr, m_d3dColorPS.ReleaseAndGetAddressOf()));

    CD3D11_TEXTURE2D_DESC descTex(DXGI_FORMAT_R16G16B16A16_FLOAT, width, height, 1, 1, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    DX::ThrowIfFailed(d3dDevice->CreateTexture2D(&descTex, nullptr, m_d3dHDRSceneTexture.ReleaseAndGetAddressOf()));

    CD3D11_RENDER_TARGET_VIEW_DESC descRTV(D3D11_RTV_DIMENSION_TEXTURE2D, descTex.Format);
    DX::ThrowIfFailed(d3dDevice->CreateRenderTargetView(m_d3dHDRSceneTexture.Get(), &descRTV, m_d3dHDRSceneRTV.ReleaseAndGetAddressOf()));

    CD3D11_SHADER_RESOURCE_VIEW_DESC descSRV(D3D11_SRV_DIMENSION_TEXTURE2D, descTex.Format, 0, 1);
    DX::ThrowIfFailed(d3dDevice->CreateShaderResourceView(m_d3dHDRSceneTexture.Get(), &descSRV, m_d3dHDRSceneSRV.ReleaseAndGetAddressOf()));
}


// These are the resources that depend on the device.
void Sample::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto context = m_deviceResources->GetD3DDeviceContext();

    m_fullScreenQuad = std::make_unique<DX::FullScreenQuad>();
    m_fullScreenQuad->Initialize(device);

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device, m_deviceResources->GetBackBufferCount());
    m_font = std::make_unique<SpriteFont>(device, L"Courier_36.spritefont");
    m_controllerFont = std::make_unique<SpriteFont>(device, L"XboxOneControllerLegendSmall.spritefont");
    m_spriteBatch = std::make_unique<SpriteBatch>(context);

    m_states = std::make_unique<CommonStates>(device);
    m_lineEffect = std::make_unique<BasicEffect>(device);
    m_lineEffect->SetVertexColorEnabled(true);

    {
        void const* shaderByteCode;
        size_t byteCodeLength;
        m_lineEffect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);

        DX::ThrowIfFailed(
            device->CreateInputLayout(VertexPositionColor::InputElements, VertexPositionColor::InputElementCount,
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

#pragma endregion

#pragma region Message Handlers

// Message handlers
void Sample::OnSuspending()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    context->Suspend(0);
}

void Sample::OnResuming()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    context->Resume();
    m_timer.ResetElapsedTime();
    m_gamePadButtons.Reset();
}

#pragma endregion

