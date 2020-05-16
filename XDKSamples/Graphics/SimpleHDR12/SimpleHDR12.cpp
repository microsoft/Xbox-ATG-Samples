//--------------------------------------------------------------------------------------
// SimpleHDR12.cpp
//
// This sample uses an API to determine if the attached display is HDR capable. If so, it
// will switch the display to HDR mode. A very simple HDR scene, with values above 1.0f,
// is rendered to a FP16 backbuffer and outputs to two different swapchains, one for HDR
// and one for SDR. Even if the consumer uses an HDR display, the SDR signal is still
// required for GameDVR and screenshots.
//
// Requirements for swapchain creation:
//  1) HDR swapchain has to be DXGI_FORMAT_R10G10B10A2_UNORM
//  2) HDR swapchain has to use XGIX_SWAP_CHAIN_FLAG_COLORIMETRY_RGB_BT2020_ST2084
//
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
#include "SimpleHDR12.h"

#include "HDR\HDRCommon.h"

#include "ControllerFont.h"
#include "ReadData.h"

#include <ppltasks.h>

extern void ExitSample() noexcept;

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
    m_current2084CurveRenderingNits(500.0f),
    m_hdrSceneValues{ 0.5f, 1.0f, 6.0f, 10.0f },
    m_HDR10Data{ 100.f }
{
    // Renders only 2D, so no need for a depth buffer.
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        DXGI_FORMAT_B8G8R8A8_UNORM_SRGB /* SDR swapchain format */,
        DXGI_FORMAT_UNKNOWN,
        2,
        DX::DeviceResources::c_Enable4K_UHD);

    m_hdrScene = std::make_unique<DX::RenderTexture>(DXGI_FORMAT_R16G16B16A16_FLOAT);
    m_hdrScene->SetClearColor(Black);
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

    // Prepare the command list to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    auto d3dCommandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(d3dCommandList, PIX_COLOR_DEFAULT, L"Render");

    ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptorHeap->Heap() };
    d3dCommandList->SetDescriptorHeaps(_countof(heaps), heaps);

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

    PIXEndEvent(d3dCommandList);

    // Show the new frame.
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present(D3D12_RESOURCE_STATE_RENDER_TARGET, m_bIsTVInHDRMode);
    m_graphicsMemory->Commit(m_deviceResources->GetCommandQueue());
    PIXEndEvent();
}


// Process the HDR scene so that the swapchains can correctly be sent to HDR or SDR display
void Sample::PrepareSwapChainBuffers()
{
    auto d3dCommandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(d3dCommandList, PIX_COLOR_DEFAULT, L"PrepareSwapChainBuffers");

    // We need to sample from the HDR backbuffer
    m_hdrScene->TransitionTo(d3dCommandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // Set RTVs
    D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptor[2] = {m_deviceResources->GetHDR10RenderTargetView(), m_deviceResources->GetGameDVRRenderTargetView() };
    d3dCommandList->OMSetRenderTargets(2, rtvDescriptor, FALSE, nullptr);

    // Update constant buffer and render
    auto hdr10Data = m_graphicsMemory->AllocateConstant<HDR10Data>(m_HDR10Data);
    auto hdrSRV = m_resourceDescriptorHeap->GetGpuHandle(static_cast<int>(ResourceDescriptors::HDRScene));
    m_fullScreenQuad->Draw(d3dCommandList, m_d3dPrepareSwapChainBufferPSO.Get(), hdrSRV, hdr10Data.GpuAddress());

    PIXEndEvent(d3dCommandList);
}


// Render the HDR scene with four squares, each with a different HDR value. Values larger than 1.0f will be perceived as bright
void Sample::RenderHDRScene()
{
    auto d3dCommandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(d3dCommandList, PIX_COLOR_DEFAULT, L"RenderHDRScene");

    auto viewportUI = m_deviceResources->GetScreenViewport();
    viewportUI.Width = 1920;
    viewportUI.Height = 1080;
    m_spriteBatch->SetViewport(viewportUI);
    m_fontBatch->SetViewport(viewportUI);

    const long step = static_cast<long>(1920.0f / (c_NumInputValues + 2.0f));

    float startX = 115;

    // SpriteBatch requires a texture, otherwise it will assert, but we just want to draw a color, so give it a dummy texture
    XMUINT2 dummyTextureSize = { 1, 1 };
    auto dummyTexture = m_resourceDescriptorHeap->GetGpuHandle(static_cast<int>(ResourceDescriptors::HDRScene));

    RECT position = {};
    position.left = static_cast<long>(startX);  

    // Render each block with the specific HDR scene value
    for (int i = 0; i < c_NumInputValues; i++)
    {        
        XMVECTOR hdrSceneColor = MakeColor(m_hdrSceneValues[i]);

        m_spriteBatch->Begin(d3dCommandList, SpriteSortMode_Immediate);

        position.left += step;
        position.top = 485;
        position.right = position.left + static_cast<long>(step / 1.25f);
        position.bottom = position.top + 250;

        if (!m_bShowOnlyPaperWhite)
        {
            m_spriteBatch->Draw(dummyTexture, dummyTextureSize, position, hdrSceneColor);
        }
        else if (XMVector2Equal(hdrSceneColor, White))
        {
            m_spriteBatch->Draw(dummyTexture, dummyTextureSize, position, hdrSceneColor);
        }

        m_spriteBatch->End();
    }

    // Render the text 
    wchar_t strText[2048] = {};
    const float startY = 40.0f;
    const float fontScale = 0.75f;

    Vector2 fontPos;

    startX = 50.0f;
   
    m_fontBatch->Begin(d3dCommandList);
    
    fontPos.x = startX;
    fontPos.y = startY + 270.0f;
    m_textFont->DrawString(m_fontBatch.get(), L"HDR Scene Values", fontPos, White, 0.0f, g_XMZero, fontScale); fontPos.y += 40;
    m_textFont->DrawString(m_fontBatch.get(), L"SDR sRGB Curve", fontPos, White, 0.0f, g_XMZero, fontScale); fontPos.y += 40;
    m_textFont->DrawString(m_fontBatch.get(), L"HDR ST.2084 Curve", fontPos, White, 0.0f, g_XMZero, fontScale); fontPos.y += 40;
    m_textFont->DrawString(m_fontBatch.get(), L"HDR Nits Output", fontPos, White, 0.0f, g_XMZero, fontScale);

    fontPos.x = startX + 100.0f;

    for (int i = 0; i < c_NumInputValues; i++)
    {
        float hdrSceneValue = m_hdrSceneValues[i];
        float sdrGamma = LinearToSRGB(hdrSceneValue);
        float hdr2084 = LinearToST2084(hdrSceneValue, m_HDR10Data.PaperWhiteNits);
        float hdrNits = CalcNits(hdrSceneValue, m_HDR10Data.PaperWhiteNits);

        fontPos.x += step;
        fontPos.y = startY + 270.0f;
        swprintf_s(strText, L"%f", hdrSceneValue);
        m_textFont->DrawString(m_fontBatch.get(), strText, fontPos, White, 0.0f, g_XMZero, fontScale); fontPos.y += 40;
        swprintf_s(strText, L"%f", sdrGamma);
        m_textFont->DrawString(m_fontBatch.get(), strText, fontPos, White, 0.0f, g_XMZero, fontScale); fontPos.y += 40;
        swprintf_s(strText, L"%f", hdr2084);
        m_textFont->DrawString(m_fontBatch.get(), strText, fontPos, White, 0.0f, g_XMZero, fontScale); fontPos.y += 40;
        swprintf_s(strText, L"%f", hdrNits);
        m_textFont->DrawString(m_fontBatch.get(), strText, fontPos, White, 0.0f, g_XMZero, fontScale); fontPos.y += 40;
    }

    fontPos.y = startY + 700.0f;
    fontPos.x = startX + 100.0f + step + step - 15.0f;
    m_textFont->DrawString(m_fontBatch.get(), L"Paper White", fontPos, White, 0.0f, g_XMZero, fontScale); fontPos.x += step + 45;

    if (!m_bShowOnlyPaperWhite)
    {
        m_textFont->DrawString(m_fontBatch.get(), L"Bright", fontPos, White, 0.0f, g_XMZero, fontScale);
    }

    m_fontBatch->End();

    PIXEndEvent(d3dCommandList);
}


// Render the ST.2084 curve
void Sample::Render2084Curve()
{
    auto d3dCommandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(d3dCommandList, PIX_COLOR_DEFAULT, L"Render2084Curve");

    auto outputSize = m_deviceResources->GetOutputSize();
    float scale = (outputSize.bottom - outputSize.top) / 1080.0f;

    float viewportWidth = 1675 * scale;
    float viewportHeight = 600 * scale;
    float startX = 150.0f;
    float startY = 250.0f;

    auto viewport = m_deviceResources->GetScreenViewport();
    auto scissorRect = m_deviceResources->GetScissorRect();

    viewport.TopLeftX = startX * scale;
    viewport.TopLeftY = startY * scale;
    viewport.Width = viewportWidth;
    viewport.Height = viewportHeight;

    scissorRect.left = static_cast<LONG>(viewport.TopLeftX);
    scissorRect.top = static_cast<LONG>(viewport.TopLeftY);
    scissorRect.right = static_cast<LONG>(scissorRect.left + viewport.Width);
    scissorRect.bottom = static_cast<LONG>(scissorRect.top + viewport.Height);

    d3dCommandList->RSSetViewports(1, &viewport);
    d3dCommandList->RSSetScissorRects(1, &scissorRect);

    Matrix proj = Matrix::CreateOrthographicOffCenter(0.0f, viewportWidth, viewportHeight, 0.0f, 0.0f, 1.0f);

    m_lineEffect->SetProjection(proj);
    m_lineEffect->Apply(d3dCommandList);
    m_primitiveBatch->Begin(d3dCommandList);

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
    scissorRect = m_deviceResources->GetScissorRect();
    d3dCommandList->RSSetViewports(1, &viewport);   
    d3dCommandList->RSSetScissorRects(1, &scissorRect);

    auto viewportUI = m_deviceResources->GetScreenViewport();
    viewportUI.Width = 1920;
    viewportUI.Height = 1080;
    m_fontBatch->SetViewport(viewportUI);
    m_spriteBatch->SetViewport(viewportUI);

    // Render text
    viewportWidth /= scale;
    viewportHeight /= scale;

    wchar_t strText[2048] = {};
    Vector2 fontPos;
    m_fontBatch->Begin(d3dCommandList);

    fontPos.x = startX - 100;
    fontPos.y = startY + viewportHeight + 5;
    m_textFont->DrawString(m_fontBatch.get(), L"Linear", fontPos, White, 0.0f, g_XMZero, 0.4f); fontPos.y += 20;
    m_textFont->DrawString(m_fontBatch.get(), L"Nits", fontPos, White, 0.0f, g_XMZero, 0.4f); fontPos.y += 20;
    m_textFont->DrawString(m_fontBatch.get(), L"HDR Scene", fontPos, White, 0.0f, g_XMZero, 0.4f); fontPos.y += 20;

    fontPos.x = startX + viewportWidth - 5;
    fontPos.y = startY + viewportHeight + 5;
    m_textFont->DrawString(m_fontBatch.get(), L"1.0", fontPos, White, 0.0f, g_XMZero, 0.4f); fontPos.y += 20;     // Always [0..1]
    m_textFont->DrawString(m_fontBatch.get(), L"10K", fontPos, White, 0.0f, g_XMZero, 0.4f); fontPos.y += 20;     // Spec defines 10K nits

    // Max HDR scene value changes as white paper nits change
    float hdrSceneValue = CalcHDRSceneValue(c_MaxNitsFor2084, m_HDR10Data.PaperWhiteNits);
    swprintf_s(strText, L"%1.0f", hdrSceneValue);
    m_textFont->DrawString(m_fontBatch.get(), strText, fontPos, White, 0.0f, g_XMZero, 0.4f); fontPos.y += 20;

    normalizedLinearValue = m_current2084CurveRenderingNits / c_MaxNitsFor2084;
    normalizedNonLinearValue = LinearToST2084(normalizedLinearValue);
    hdrSceneValue = CalcHDRSceneValue(m_current2084CurveRenderingNits, m_HDR10Data.PaperWhiteNits);

    x = normalizedLinearValue * viewportWidth + 1;
    y = viewportHeight - (normalizedNonLinearValue * viewportHeight);

    fontPos.x = startX + x;
    fontPos.y = startY + viewportHeight + 5;
    swprintf_s(strText, L"%1.2f", normalizedLinearValue);
    m_textFont->DrawString(m_fontBatch.get(), strText, fontPos, White, 0.0f, g_XMZero, 0.4f); fontPos.y += 20;
    swprintf_s(strText, L"%1.0f", m_current2084CurveRenderingNits);
    m_textFont->DrawString(m_fontBatch.get(), strText, fontPos, White, 0.0f, g_XMZero, 0.4f); fontPos.y += 20;
    swprintf_s(strText, L"%1.2f", hdrSceneValue);
    m_textFont->DrawString(m_fontBatch.get(), strText, fontPos, White, 0.0f, g_XMZero, 0.4f); fontPos.y += 20;

    fontPos.x = startX - 25;
    fontPos.y = startY - 50;
    m_textFont->DrawString(m_fontBatch.get(), L"ST.2084", fontPos, White, -XM_PIDIV2, g_XMZero, 0.4f); fontPos.x -= 20;
    m_textFont->DrawString(m_fontBatch.get(), L"Nits", fontPos, White, -XM_PIDIV2, g_XMZero, 0.4f);

    fontPos.x = startX - 25;
    fontPos.y = y + startY;
    swprintf_s(strText, L"%1.2f", normalizedNonLinearValue);
    m_textFont->DrawString(m_fontBatch.get(), strText, fontPos, White, -XM_PIDIV2, g_XMZero, 0.4f); fontPos.x -= 20;
    swprintf_s(strText, L"%1.0f", m_current2084CurveRenderingNits);
    m_textFont->DrawString(m_fontBatch.get(), strText, fontPos, White, -XM_PIDIV2, g_XMZero, 0.4f);

    m_fontBatch->End();

    // Render blocks
    const long size = 150;
    RECT position = {};
    position.left = 1920 - size * 4;
    position.top = 50;
    position.right = position.left + size;
    position.bottom = position.top + size;

    // SpriteBatch requires a texture, otherwise it will assert, but we just want to draw a color, so give it a dummy texture
    XMUINT2 dummyTextureSize = { 1, 1 };
    auto dummyTexture = m_resourceDescriptorHeap->GetGpuHandle(static_cast<int>(ResourceDescriptors::HDRScene));

    m_spriteBatch->Begin(d3dCommandList, SpriteSortMode_Immediate);
    m_spriteBatch->Draw(dummyTexture, dummyTextureSize, position, White);

    position.left += size * 2;
    position.right = position.left + size;

    XMVECTORF32 color = { hdrSceneValue, hdrSceneValue, hdrSceneValue, 1.0f };
    m_spriteBatch->Draw(dummyTexture, dummyTextureSize, position, color);

    m_spriteBatch->End();

    // Render text for blocks
    m_fontBatch->Begin(d3dCommandList);

    fontPos.x = 1920 - size * 4 - 25;
    fontPos.y = static_cast<float>(position.bottom - 15);

    m_textFont->DrawString(m_fontBatch.get(), L"Paper White", fontPos, White, -XM_PIDIV2, g_XMZero, 0.4f);

    fontPos.x = 1920 - size * 4 + 25;
    fontPos.y = static_cast<float>(position.bottom);

    swprintf_s(strText, L"%1.0f nits", m_HDR10Data.PaperWhiteNits);
    m_textFont->DrawString(m_fontBatch.get(), strText, fontPos, White, 0.0f, g_XMZero, 0.4f); fontPos.x += size * 2;
    swprintf_s(strText, L"%1.0f nits", m_current2084CurveRenderingNits);
    m_textFont->DrawString(m_fontBatch.get(), strText, fontPos, White, 0.0f, g_XMZero, 0.4f);

    m_fontBatch->End();

    PIXEndEvent(d3dCommandList);
}


// Render the UI
void Sample::RenderUI()
{
    auto d3dCommandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(d3dCommandList, PIX_COLOR_DEFAULT, L"RenderUI");

    auto viewportUI = m_deviceResources->GetScreenViewport();
    viewportUI.Width = 1920;
    viewportUI.Height = 1080;
    m_fontBatch->SetViewport(viewportUI);

    const float startX = 50.0f;
    const float startY = 40.0f;
    const float fontScale = 0.75f;

    Vector2 fontPos(startX, startY);

    m_fontBatch->Begin(d3dCommandList);
    m_textFont->DrawString(m_fontBatch.get(), L"SimpleHDR Sample for DirectX 12", fontPos, White, 0.0f, g_XMZero, 1.0f);

    if (!m_bRender2084Curve)
    {
        fontPos.y = startY + 100.0f;
        if (m_bIsTVInHDRMode)
        {
            m_textFont->DrawString(m_fontBatch.get(), L"TV in HDR Mode: TRUE", fontPos, White, 0.0f, g_XMZero, fontScale);
        }
        else
        {
            m_textFont->DrawString(m_fontBatch.get(), L"TV in HDR Mode: FALSE", fontPos, White, 0.0f, g_XMZero, fontScale);
        }
    }

    fontPos.x = startX;
    fontPos.y = 955;
    DX::DrawControllerString(m_fontBatch.get(), m_textFont.get(), m_controllerFont.get(), L"[A] - Toggle displaying ST.2084 curve", fontPos, White, 0.65f); fontPos.y += 35;
    DX::DrawControllerString(m_fontBatch.get(), m_textFont.get(), m_controllerFont.get(), L"[B] - Toggle displaying only paper white block", fontPos, White, 0.65f); fontPos.y += 35;

    fontPos.x = (1920.0f/2.0f) + startX;
    fontPos.y = 955;
    DX::DrawControllerString(m_fontBatch.get(), m_textFont.get(), m_controllerFont.get(), L"[DPad] - Adjust paper white nits", fontPos, White, 0.65f); fontPos.y += 35;
    DX::DrawControllerString(m_fontBatch.get(), m_textFont.get(), m_controllerFont.get(), L"[LThumb] - Adjust values quickly", fontPos, White, 0.65f); fontPos.y += 35;
    DX::DrawControllerString(m_fontBatch.get(), m_textFont.get(), m_controllerFont.get(), L"[RThumb] - Adjust values slowly", fontPos, White, 0.65f); fontPos.y += 35;

    if (m_countDownToBright >= 0)
    {
        fontPos.x = 1170;
        fontPos.y = 550;

        wchar_t strText[2048] = {};
        swprintf_s(strText, L"%1.0f", m_countDownToBright);
        m_textFont->DrawString(m_fontBatch.get(), strText, fontPos, White, 0.0f, g_XMZero, 1.75f);
    }

    m_fontBatch->End();

    PIXEndEvent(d3dCommandList);
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
            m_HDR10Data.PaperWhiteNits -= 20.0f;
            m_HDR10Data.PaperWhiteNits = std::max(m_HDR10Data.PaperWhiteNits, 80.0f);
        }

        if (m_gamePadButtons.dpadUp == GamePad::ButtonStateTracker::PRESSED ||
            m_gamePadButtons.dpadRight == GamePad::ButtonStateTracker::PRESSED)
        {
            m_HDR10Data.PaperWhiteNits += 20.0f;
            m_HDR10Data.PaperWhiteNits = std::min(m_HDR10Data.PaperWhiteNits, c_MaxNitsFor2084);
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
}


// Helper method to clear the back buffers.
void Sample::Clear()
{
    auto d3dCommandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(d3dCommandList, PIX_COLOR_DEFAULT, L"Clear");

    m_hdrScene->TransitionTo(d3dCommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);

    auto rtvDescriptor = m_rtvDescriptorHeap->GetCpuHandle(static_cast<int>(ResourceDescriptors::HDRScene));
    d3dCommandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, nullptr);

    // Use linear clear color for gamma-correct rendering.
    d3dCommandList->ClearRenderTargetView(m_deviceResources->GetHDR10RenderTargetView(), Black, 0, nullptr);
    d3dCommandList->ClearRenderTargetView(m_deviceResources->GetGameDVRRenderTargetView(), Black, 0, nullptr);
    d3dCommandList->ClearRenderTargetView(rtvDescriptor, Black, 0, nullptr);

    // Set the viewport and scissor rect.
    auto viewport = m_deviceResources->GetScreenViewport();
    auto scissorRect = m_deviceResources->GetScissorRect();
    d3dCommandList->RSSetViewports(1, &viewport);
    d3dCommandList->RSSetScissorRects(1, &scissorRect);

    PIXEndEvent(d3dCommandList);
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Sample::OnSuspending()
{
    auto queue = m_deviceResources->GetCommandQueue();
    queue->SuspendX(0);
}

void Sample::OnResuming()
{
    auto queue = m_deviceResources->GetCommandQueue();
    queue->ResumeX();
    m_timer.ResetElapsedTime();
    m_gamePadButtons.Reset();
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Sample::CreateDeviceDependentResources()
{
    auto d3dDevice = m_deviceResources->GetD3DDevice();

    m_graphicsMemory = std::make_unique<GraphicsMemory>(d3dDevice);

    m_fullScreenQuad = std::make_unique<DX::FullScreenQuad>();
    m_fullScreenQuad->Initialize(d3dDevice);

    // Create descriptor heap for RTVs 
    m_rtvDescriptorHeap = std::make_unique<DescriptorHeap>(d3dDevice,
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        static_cast<UINT>(RTVDescriptors::Count));

    // Create descriptor heap for resources
    m_resourceDescriptorHeap = std::make_unique<DescriptorHeap>(d3dDevice,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        static_cast<UINT>(ResourceDescriptors::Count));

    ResourceUploadBatch resourceUpload(d3dDevice);
    resourceUpload.Begin();

    // Create HDR backbuffer resources
    m_hdrScene->SetDevice(d3dDevice,
        m_resourceDescriptorHeap->GetCpuHandle(static_cast<int>(ResourceDescriptors::HDRScene)),
        m_rtvDescriptorHeap->GetCpuHandle(static_cast<int>(RTVDescriptors::HDRScene)));

    auto size = m_deviceResources->GetOutputSize();
    m_hdrScene->SetWindow(size);

    // Init fonts
    RenderTargetState rtState(m_hdrScene->GetFormat(), m_deviceResources->GetDepthBufferFormat());
    InitializeSpriteFonts(d3dDevice, resourceUpload, rtState);

    // SpriteBatch for rendering HDR values into the backbuffer
    {
        auto pixelShaderBlob = DX::ReadData(L"ColorPS.cso");
        SpriteBatchPipelineStateDescription pd(rtState);
        pd.customPixelShader.BytecodeLength = pixelShaderBlob.size();
        pd.customPixelShader.pShaderBytecode = pixelShaderBlob.data();
        m_spriteBatch = std::make_unique<SpriteBatch>(d3dDevice, resourceUpload, pd);
    }

    // PrimitiveBatch for rendering lines into the backbuffer
    {       
        D3D12_RASTERIZER_DESC state = CommonStates::CullNone;
        state.MultisampleEnable = FALSE;
        EffectPipelineStateDescription pd(&VertexPositionColor::InputLayout, CommonStates::Opaque, CommonStates::DepthNone, state, rtState, D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
        m_lineEffect = std::make_unique<BasicEffect>(d3dDevice, EffectFlags::VertexColor, pd);
        m_primitiveBatch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(d3dDevice);
    }

    // PSO for rendering the HDR10 and GameDVR swapchain buffers
    {
        auto pixelShaderBlob = DX::ReadData(L"PrepareSwapChainBuffersPS.cso");
        auto vertexShaderBlob = DX::ReadData(L"FullScreenQuadVS.cso");
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = m_fullScreenQuad->GetRootSignature();
        psoDesc.VS = { vertexShaderBlob.data(), vertexShaderBlob.size() };
        psoDesc.PS = { pixelShaderBlob.data(), pixelShaderBlob.size() };
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.DSVFormat = m_deviceResources->GetDepthBufferFormat();
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 2;
        psoDesc.RTVFormats[0] = m_deviceResources->GetHDR10BackBufferFormat();
        psoDesc.RTVFormats[1] = m_deviceResources->GetGameDVRFormat();
        psoDesc.SampleDesc.Count = 1;
        DX::ThrowIfFailed(d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_GRAPHICS_PPV_ARGS(m_d3dPrepareSwapChainBufferPSO.ReleaseAndGetAddressOf())));
    }

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());   
    uploadResourcesFinished.wait();     // Wait for resources to upload
}

// Initialize all the fonts used
void Sample::InitializeSpriteFonts(ID3D12Device* d3dDevice, ResourceUploadBatch& resourceUpload, RenderTargetState& rtState)
{
    SpriteBatchPipelineStateDescription pd(rtState, &CommonStates::AlphaBlend);
    m_fontBatch = std::make_unique<SpriteBatch>(d3dDevice, resourceUpload, pd);

    auto cpuDescHandleText = m_resourceDescriptorHeap->GetCpuHandle(static_cast<int>(ResourceDescriptors::TextFont));
    auto gpuDescHandleText = m_resourceDescriptorHeap->GetGpuHandle(static_cast<int>(ResourceDescriptors::TextFont));
    m_textFont = std::make_unique<SpriteFont>(d3dDevice, resourceUpload, L"Courier_36.spritefont", cpuDescHandleText, gpuDescHandleText);

    auto cpuDescHandleController = m_resourceDescriptorHeap->GetCpuHandle(static_cast<int>(ResourceDescriptors::ControllerFont));
    auto gpuDescHandleController = m_resourceDescriptorHeap->GetGpuHandle(static_cast<int>(ResourceDescriptors::ControllerFont));
    m_controllerFont = std::make_unique<SpriteFont>(d3dDevice, resourceUpload, L"XboxOneControllerLegendSmall.spritefont", cpuDescHandleController, gpuDescHandleController);
}

#pragma endregion