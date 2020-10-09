//--------------------------------------------------------------------------------------
// AdvancedESRAM12.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "AdvancedESRAM12.h"

extern void ExitSample() noexcept;

using namespace DirectX;
using namespace ATG;
using namespace SimpleMath;

using Microsoft::WRL::ComPtr;

#pragma region Constants & Helpers
namespace
{
    //--------------------------------------
    // Definitions
    
    enum DescriptorHeapIndex
    {
        SRV_Font = 0,
        SRV_CtrlFont,
        SRV_SceneColor,
        UAV_SceneColor,
        SRV_Outline0,
        SRV_Outline1,
        SRV_Bloom0,
        SRV_Bloom1,
        SRV_Count
    };

    enum TimerIndex
    {
        TI_Frame = 0,
        TI_Scene,
        TI_Color,
        TI_Depth,
        TI_Outline,
        TI_Bloom,
    };

    // Barebones definition of scene objects.
    struct ObjectDefinition
    {
        Matrix  world;
        size_t  modelIndex;
    };

    //--------------------------------------
    // Constants

    // Assest paths.
    const wchar_t* const s_modelPaths[] =
    {
        L"scanner.sdkmesh",
        L"occcity.sdkmesh",
        L"column.sdkmesh",
    };

    // Barebones definition of a scene.
    const ObjectDefinition s_sceneDefinition[] =
    {
        { XMMatrixIdentity(), 0 },
        { XMMatrixRotationY(XM_2PI * (1.0f / 6.0f)), 0 },
        { XMMatrixRotationY(XM_2PI * (2.0f / 6.0f)), 0 },
        { XMMatrixRotationY(XM_2PI * (3.0f / 6.0f)), 0 },
        { XMMatrixRotationY(XM_2PI * (4.0f / 6.0f)), 0 }, 
        { XMMatrixRotationY(XM_2PI * (5.0f / 6.0f)), 0 },
        { XMMatrixIdentity(), 1 },
        { XMMatrixIdentity(), 2 },
    };

    const Color s_visualizeColors[] = 
    {
        DirectX::Colors::Green,
        DirectX::Colors::Purple,
        DirectX::Colors::Orange,
        DirectX::Colors::Turquoise,
        DirectX::Colors::Red,
        DirectX::Colors::Blue,
    };

    static const wchar_t* s_textureNames[ST_Count] =
    {
        L"Scene Color",
        L"Scene Depth",
        L"Outline 0",
        L"Outline 1",
        L"Bloom 0",
        L"Bloom 1",
    };

    // Full screen triangle geometry definition.
    const std::vector<GeometricPrimitive::VertexType> s_triVertex =
    {
        { XMFLOAT3{ -1.0f,  1.0f, 0.0f }, XMFLOAT3{ 0.0f, 0.0f, -1.0f }, XMFLOAT2{ 0.0f, 0.0f } },  // Top-left
        { XMFLOAT3{ 3.0f,  1.0f, 0.0f }, XMFLOAT3{ 0.0f, 0.0f, -1.0f }, XMFLOAT2{ 2.0f, 0.0f } },   // Top-right
        { XMFLOAT3{ -1.0f, -3.0f, 0.0f }, XMFLOAT3{ 0.0f, 0.0f, -1.0f }, XMFLOAT2{ 0.0f, 2.0f } },  // Bottom-left
    };

    const std::vector<uint16_t> s_triIndex = { 0, 1, 2 };

    // Sample Constants
    const DXGI_FORMAT   c_colorFormat = DXGI_FORMAT_R11G11B10_FLOAT;
    const DXGI_FORMAT   c_depthFormat = DXGI_FORMAT_D32_FLOAT;

    const float c_defaultPhi = XM_2PI / 6.0f;
    const float c_defaultRadius = 3.3f;

    //-----------------------------------
    // Helper Functions

    void AddMod(int& value, int add, int mod)
    {
        int res = (value + add) % mod;
        value = res < 0 ? mod - 1 : res;
    }

    void IncrMod(int& value, int mod) { AddMod(value, 1, mod); }
    void DecrMod(int& value, int mod) { AddMod(value, -1, mod); }
    void Saturate(float& value) { value = std::max(0.0f, std::min(1.0f, value)); }
}
#pragma endregion

#pragma region Construction
Sample::Sample() 
    : m_displayWidth(0)
    , m_displayHeight(0)
    , m_frame(0)
    , m_theta(0.0f)
    , m_phi(c_defaultPhi)
    , m_radius(c_defaultRadius)
    , m_esramRatios{}
    , m_esramChangeRate(0.5f)
    , m_outlineObjectIndex(0)
    , m_updateStats(true)
    , m_visData{}
{
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN);

    std::fill_n(m_esramRatios, _countof(m_esramRatios), 1.0f);
}

Sample::~Sample()
{
    if (m_deviceResources)
    {
        m_deviceResources->WaitForGpu();
    }
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(IUnknown* window)
{
    m_deviceResources->SetWindow(window);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();
}
#pragma endregion

#pragma region Frame Update
// Executes basic render loop.
void Sample::Tick()
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Frame %llu", m_frame);

    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();

    PIXEndEvent();
    ++m_frame;
}

void Sample::Update(const DX::StepTimer& timer)
{
    using ButtonState = DirectX::GamePad::ButtonStateTracker::ButtonState;

    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    float elapsedTime = float(timer.GetElapsedSeconds());
    m_visData.time = float(timer.GetTotalSeconds());

    auto pad = m_gamePad.GetState(0);
    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);

        if (pad.IsViewPressed())
        {
            ExitSample();
        }

        // Change the selected object for outline.
        if (m_gamePadButtons.rightShoulder == ButtonState::PRESSED)
        {
            IncrMod(m_outlineObjectIndex, int(m_scene.size()));
            m_updateStats = true;
        }
        else if (m_gamePadButtons.leftShoulder == ButtonState::PRESSED)
        {
            DecrMod(m_outlineObjectIndex, int(m_scene.size()));
            m_updateStats = true;
        }

        // Change the selected texture.
        if (m_gamePadButtons.dpadRight == ButtonState::PRESSED)
        {
            IncrMod(m_visData.selectedIndex, ST_Count);
            m_updateStats = true;
        }
        else if (m_gamePadButtons.dpadLeft == ButtonState::PRESSED)
        {
            DecrMod(m_visData.selectedIndex, ST_Count);
            m_updateStats = true;
        }

        // Change ESRAM allotment for the currently selected texture.
        if (pad.IsDPadUpPressed())
        {
            int index = m_visData.selectedIndex;

            m_esramRatios[index] += elapsedTime * m_esramChangeRate;
            Saturate(m_esramRatios[index]);

            m_updateStats = true;
        }
        else if (pad.IsDPadDownPressed())
        {
            int index = m_visData.selectedIndex;

            m_esramRatios[index] -= elapsedTime * m_esramChangeRate;
            Saturate(m_esramRatios[index]);

            m_updateStats = true;
        }

        if (m_gamePadButtons.a == ButtonState::PRESSED)
        {
            // Queue off a timing update.
            m_visData.duration = 0.0f;
        }

        if (pad.IsRightStickPressed())
        {
            m_theta = 0.f;
            m_phi = c_defaultPhi;
            m_radius = c_defaultRadius;
        }
        else
        {
            m_theta += pad.thumbSticks.rightX * XM_PI * elapsedTime;
            m_phi -= pad.thumbSticks.rightY * XM_PI * elapsedTime;
            m_radius -= pad.thumbSticks.leftY * 5.f * elapsedTime;
        }
    }
    else
    {
        m_gamePadButtons.Reset();
    }

    if (std::abs(m_visData.duration - m_profiler->GetElapsedMS(TI_Frame)) > 0.5f)
    {
        UpdateVisualizerTimings();
    }

    // Limit to avoid looking directly up or down
    m_phi = std::max(1e-2f, std::min(XM_PIDIV2, m_phi));
    m_radius = std::max(1.f, std::min(10.f, m_radius));

    if (m_theta > XM_PI)
    {
        m_theta -= XM_PI * 2.f;
    }
    else if (m_theta < -XM_PI)
    {
        m_theta += XM_PI * 2.f;
    }

    XMVECTOR lookFrom = XMVectorSet(
        m_radius * sinf(m_phi) * cosf(m_theta),
        m_radius * cosf(m_phi),
        m_radius * sinf(m_phi) * sinf(m_theta),
        0);

    m_view = XMMatrixLookAtLH(lookFrom, g_XMZero, g_XMIdentityR1);

    // Update the scene.
    m_emissiveEffect->SetMatrices(m_scene[m_outlineObjectIndex].world, m_view, m_proj);

    for (int i = 0; i < m_scene.size(); ++i)
    {
        Model::UpdateEffectMatrices(m_scene[i].effects, m_scene[i].world, m_view, m_proj);
    }

    PIXEndEvent();
}
#pragma endregion


#pragma region Frame Render
void Sample::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    // Prepare the command list to render a new frame.
    m_deviceResources->Prepare();
    auto commandList = m_deviceResources->GetCommandList();

    // Reset the transient allocator's resource cache and page allocators to fully unallocated.
    m_allocator->NextFrame();

    // Acquire resource instances for our main scene color & depth.
    auto depthTex = AcquireTransientTexture(commandList, m_depthDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, ST_Depth);
    auto colorTex = AcquireTransientTexture(commandList, m_colorDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, ST_Color);

    auto srvHandle = m_srvPile->WriteDescriptors(m_deviceResources->GetD3DDevice(), SRV_SceneColor, &colorTex.SRV, 1);
    auto uavHandle = m_srvPile->WriteDescriptors(m_deviceResources->GetD3DDevice(), UAV_SceneColor, &colorTex.UAV, 1);

    ResourceHandle handles[ST_Count];
    handles[ST_Color] = colorTex.handle;
    handles[ST_Depth] = depthTex.handle;

    // Begin frame
    m_profiler->BeginFrame(commandList);
    m_profiler->Start(commandList, TI_Frame);
    m_profiler->Start(commandList, TI_Color);
    m_profiler->Start(commandList, TI_Depth);

    // Set descriptor heaps
    ID3D12DescriptorHeap* heaps[] = { m_srvPile->Heap(), m_commonStates->Heap() };
    commandList->SetDescriptorHeaps(UINT(_countof(heaps)), heaps);

    {
        ScopedPixEvent Clear(commandList, PIX_COLOR_DEFAULT, L"Clear");

        // Set the viewport and scissor rect.
        auto viewport = m_deviceResources->GetScreenViewport();
        auto scissorRect = m_deviceResources->GetScissorRect();

        commandList->RSSetViewports(1, &viewport);
        commandList->RSSetScissorRects(1, &scissorRect);

        commandList->ClearRenderTargetView(colorTex.RTV, ATG::ColorsLinear::Background, 0, nullptr);
        commandList->ClearDepthStencilView(depthTex.DSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        commandList->OMSetRenderTargets(1, &colorTex.RTV, false, &depthTex.DSV);
    }

    {
        ScopedPixEvent Render(commandList, PIX_COLOR_DEFAULT, L"Render");

        //---------------------------------------------
        // Main Scene Rendering

        {
            ScopedPixEvent Scene(commandList, PIX_COLOR_DEFAULT, L"Scene");

            m_profiler->Start(commandList, TI_Scene);

            // Draw the scene.
            for (int i = 0; i < m_scene.size(); ++i)
            {
                if (i != m_outlineObjectIndex)
                {
                    m_scene[i].model->DrawOpaque(commandList, m_scene[i].effects.begin());
                }
            }

            m_profiler->Stop(commandList, TI_Scene);
        }


        //---------------------------------------------
        // Outline Effect

        {
            ScopedPixEvent Outline(commandList, PIX_COLOR_DEFAULT, L"Outline");

            // Acquire resource instances that we'll use to render out the outline effect.
            TransientResource outlineTex[2];
            D3D12_GPU_DESCRIPTOR_HANDLE srvHandles[2];

            for (int i = 0; i < _countof(outlineTex); ++i)
            {
                outlineTex[i] = AcquireTransientTexture(commandList, m_outlineDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, SceneTexture(ST_Outline0 + i));
                srvHandles[i] = m_srvPile->WriteDescriptors(m_deviceResources->GetD3DDevice(), SRV_Outline0 + i, &outlineTex[i].SRV, 1);

                handles[ST_Outline0 + i] = outlineTex[i].handle;
            }
            m_profiler->Start(commandList, TI_Outline);

            commandList->OMSetRenderTargets(1, &outlineTex[0].RTV, false, nullptr);
            commandList->ClearRenderTargetView(outlineTex[0].RTV, m_outlineDesc.clear.Color, 0, nullptr);

            auto& obj = m_scene[m_outlineObjectIndex];
            obj.model->DrawOpaque(commandList, m_emissiveEffect.get());


            //-----------------------------------------------------
            // Blur the outline buffer

            commandList->OMSetRenderTargets(1, &outlineTex[1].RTV, false, nullptr);

            TransitionResource(commandList, outlineTex[0].resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            m_blurEffect->SetSourceTexture(srvHandles[0], outlineTex[0].resource);

            m_blurEffect->Process(commandList);


            //-----------------------------------------------------
            // Alpha-composite back onto the scene texture.

            commandList->OMSetRenderTargets(1, &colorTex.RTV, false, nullptr);

            TransitionResource(commandList, outlineTex[1].resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            m_alphaCompositeEffect->SetTexture(srvHandles[1], m_commonStates->LinearClamp());

            m_alphaCompositeEffect->Apply(commandList);
            m_fullScreenTri->Draw(commandList);

            commandList->OMSetRenderTargets(1, &colorTex.RTV, false, &depthTex.DSV);
            obj.model->DrawOpaque(commandList, obj.effects.begin());

            // Release the outline textures' memory pages back to the allocator.
            for (int i = 0; i < _countof(outlineTex); ++i)
            {
                m_allocator->Release(commandList, outlineTex[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            }
            m_profiler->Stop(commandList, TI_Outline);
        }

        // We're done with depth here - release the depth texture's memory pages back to the allocator.
        m_allocator->Release(commandList, depthTex, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        m_profiler->Stop(commandList, TI_Depth);


        //---------------------------------------------
        // Bloom Effect

        {
            ScopedPixEvent Bloom(commandList, PIX_COLOR_DEFAULT, L"Bloom");

            // Acquire resource instances that we'll use to render out the bloom effect.
            TransientResource bloomTex[2];
            D3D12_GPU_DESCRIPTOR_HANDLE srvHandles[2];

            for (int i = 0; i < _countof(bloomTex); ++i)
            {
                bloomTex[i] = AcquireTransientTexture(commandList, m_bloomDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, SceneTexture(ST_Bloom0 + i));
                srvHandles[i] = m_srvPile->WriteDescriptors(m_deviceResources->GetD3DDevice(), SRV_Bloom0 + i, &bloomTex[i].SRV, 1);

                handles[ST_Bloom0 + i] = bloomTex[i].handle;
            }
            m_profiler->Start(commandList, TI_Bloom);

            //-----------------------------------------------------
            // Extract values to the bloom buffer

            commandList->OMSetRenderTargets(1, &bloomTex[0].RTV, false, nullptr);

            TransitionResource(commandList, colorTex.resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            m_bloomExtractEffect->SetSourceTexture(srvHandle, colorTex.resource);

            m_bloomExtractEffect->Process(commandList);


            //-----------------------------------------------------
            // Blur the bloom buffer

            // Horizontal
            commandList->OMSetRenderTargets(1, &bloomTex[1].RTV, false, nullptr);

            TransitionResource(commandList, bloomTex[0].resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            m_bloomBlurEffect->SetSourceTexture(srvHandles[0], bloomTex[0].resource);
            m_bloomBlurEffect->SetBloomBlurParameters(true, 2.0f, 1.0f);

            m_bloomBlurEffect->Process(commandList);

            // Vertical
            TransitionResource(commandList, bloomTex[0].resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
            commandList->OMSetRenderTargets(1, &bloomTex[0].RTV, false, nullptr);

            TransitionResource(commandList, bloomTex[1].resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            m_bloomBlurEffect->SetSourceTexture(srvHandles[1], bloomTex[1].resource);
            m_bloomBlurEffect->SetBloomBlurParameters(false, 2.0f, 1.0f);

            m_bloomBlurEffect->Process(commandList);


            //-----------------------------------------------------
            // Composite onto new target

            TransitionResource(commandList, bloomTex[1].resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
            commandList->OMSetRenderTargets(1, &bloomTex[1].RTV, false, nullptr);

            TransitionResource(commandList, bloomTex[0].resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
            m_bloomCombineEffect->SetSourceTexture(srvHandle);
            m_bloomCombineEffect->SetSourceTexture2(srvHandles[0]);

            m_bloomCombineEffect->Process(commandList);


            //-----------------------------------------------------
            // Copy back to colorTex to make our lives simpler

            TransitionResource(commandList, bloomTex[1].resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
            TransitionResource(commandList, colorTex.resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
            commandList->CopyResource(colorTex.resource, bloomTex[1].resource);

            // We're finished with the bloom textures - release their memory pages back to the allocator.
            for (int i = 0; i < _countof(bloomTex); ++i)
            {
                m_allocator->Release(commandList, bloomTex[i], D3D12_RESOURCE_STATE_GENERIC_READ);
            }
            m_profiler->Stop(commandList, TI_Bloom);
        }


        //---------------------------------------------
        // ESRAM Visualization

        if (SupportsEsram())
        {
            ScopedPixEvent Visualize(commandList, PIX_COLOR_DEFAULT, L"ESRAM Visualization");

            TransitionResource(commandList, colorTex.resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            m_esramVisualizeEffect->SetTexture(uavHandle);
            m_esramVisualizeEffect->SetConstants(m_visData);

            m_esramVisualizeEffect->Process(commandList);
        }


        //---------------------------------------------
        // Tonemap Effect

        {
            ScopedPixEvent Tonemap(commandList, PIX_COLOR_DEFAULT, L"Tonemap");

            auto backBufferRTV = m_deviceResources->GetRenderTargetView();
            commandList->OMSetRenderTargets(1, &backBufferRTV, false, nullptr);

            TransitionResource(commandList, colorTex.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            m_tonemapEffect->SetHDRSourceTexture(srvHandle);

            m_tonemapEffect->Process(commandList);
        }

        // Release the color texture's memory pages back to the allocator.
        m_allocator->Release(commandList, colorTex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        m_profiler->Stop(commandList, TI_Color);

        // Only profile ESRAM usage
        m_profiler->Stop(commandList, TI_Frame);
        m_profiler->EndFrame(commandList);


        //---------------------------------------------
        // HUD Rendering

        DrawHUD(commandList);
    }

    // Show the new frame.
    {
        // Inform the transient allocator the command list is about to kicked off, so that it can submit its page mapping beforehand.
        m_allocator->Finalize(m_deviceResources->GetCommandQueue());

        m_deviceResources->Present();
        m_graphicsMemory->Commit(m_deviceResources->GetCommandQueue());
    }

    if (m_updateStats)
    {
        UpdateVisualizerRanges(handles);
        m_updateStats = false;
    }
}

void Sample::DrawHUD(ID3D12GraphicsCommandList* commandList)
{
    m_hudBatch->Begin(commandList);

    auto safe = SimpleMath::Viewport::ComputeTitleSafeArea(m_displayWidth, m_displayHeight);

    wchar_t textBuffer[128] = {};
    XMFLOAT2 textPos = XMFLOAT2(float(safe.left), float(safe.top));
    XMVECTOR textColor = DirectX::Colors::DarkKhaki;

    // Draw title.
    m_smallFont->DrawString(m_hudBatch.get(), L"Advanced ESRAM", textPos, textColor);
    textPos.y += m_smallFont->GetLineSpacing();

    // Draw Frame Stats
    _snwprintf_s(textBuffer, _TRUNCATE, L"GPU Duration = %3.3lf ms", m_profiler->GetAverageMS(TI_Frame));
    m_smallFont->DrawString(m_hudBatch.get(), textBuffer, textPos, textColor);
    textPos.y += m_smallFont->GetLineSpacing();

    const wchar_t* controls = nullptr;
    if (SupportsEsram())
    {
        // ESRAM Percentages
        textPos.y = m_displayHeight / 2 - m_smallFont->GetLineSpacing() * 6;

        float flashBlend = cos(6.2832f * m_visData.time * m_visData.flashRate) * 0.5f + 0.5f;
        for (int i = 0; i < ST_Count; ++i)
        {
            XMVECTOR color = s_visualizeColors[i];
            if (i == m_visData.selectedIndex) color = DirectX::XMVectorLerp(color, DirectX::g_XMOne, flashBlend);

            _snwprintf_s(textBuffer, _TRUNCATE, L"%s - %3.1lf", s_textureNames[i], m_esramRatios[i] * 100.0f);
            m_smallFont->DrawString(m_hudBatch.get(), textBuffer, textPos, color);
            textPos.y += m_smallFont->GetLineSpacing();
        }

        // Draw the graph axes
        XMFLOAT4 bounds = { float(m_visData.bounds.x), float(m_visData.bounds.y), float(m_visData.bounds.z), float(m_visData.bounds.w) };

        float width = (bounds.z - bounds.x);
        float height = (bounds.w - bounds.y);
        float charLen = m_smallFont->MeasureString(L"_").m128_f32[0];

        // ESRAM axis
        textPos.x = bounds.x - charLen * 8.0f;
        textPos.y = bounds.y - m_smallFont->GetLineSpacing() / 2.0f;
        m_smallFont->DrawString(m_hudBatch.get(), L"0x000000", textPos, textColor);

        textPos.x = bounds.x - m_smallFont->GetLineSpacing();
        textPos.y = bounds.y + (height + charLen * 7.0f) / 2.0f;
        m_smallFont->DrawString(m_hudBatch.get(), L"ESRAM", textPos, textColor, -3.14159f / 2.0f);

        textPos.x = bounds.x - charLen * 8.0f;
        textPos.y = bounds.y + height - m_smallFont->GetLineSpacing() / 2.0f;
        m_smallFont->DrawString(m_hudBatch.get(), L"0x200000", textPos, textColor);

        // Time axis
        textPos.x = bounds.x;
        textPos.y = bounds.y + height;
        m_smallFont->DrawString(m_hudBatch.get(), L"0.0 ms", textPos, textColor);

        textPos.x = bounds.x + width / 2.0f - charLen * 4.0f;
        m_smallFont->DrawString(m_hudBatch.get(), L"Time", textPos, textColor);

        textPos.x = bounds.x + width - charLen * 6.0f;
        _snwprintf_s(textBuffer, _TRUNCATE, L"%3.1lf ms", m_visData.duration);
        m_smallFont->DrawString(m_hudBatch.get(), textBuffer, textPos, textColor);

        // Events
        float offset = 0;
        float duration = (m_visData.textures[2].timeRange.x - m_visData.textures[0].timeRange.x) / m_visData.duration;
        textPos.x = bounds.x + width * (offset + duration / 2.0f) - charLen * 3.0f;
        textPos.y = bounds.y - m_smallFont->GetLineSpacing();
        m_smallFont->DrawString(m_hudBatch.get(), L"Scene", textPos, textColor);

        offset += duration;
        duration = (m_visData.textures[2].timeRange.y - m_visData.textures[2].timeRange.x) / m_visData.duration;
        textPos.x = bounds.x + width * (offset + duration / 2.0f) - charLen * 3.0f;
        m_smallFont->DrawString(m_hudBatch.get(), L"Outline", textPos, textColor);

        offset += duration;
        duration = (m_visData.textures[4].timeRange.y - m_visData.textures[4].timeRange.x) / m_visData.duration;
        textPos.x = bounds.x + width * (offset + duration / 2.0f) - charLen * 3.0f;
        m_smallFont->DrawString(m_hudBatch.get(), L"Bloom", textPos, textColor);

        offset += duration;
        duration = (m_visData.duration - m_visData.textures[4].timeRange.y) / m_visData.duration;
        textPos.x = bounds.x + width * (offset + duration / 2.0f) - charLen * 4.0f;
        m_smallFont->DrawString(m_hudBatch.get(), L"Tonemap", textPos, textColor);

        controls = L"[LThumb] Toward/Away   [RThumb]: Orbit Camera   [DPad] Switch Texture / Change ESRAM Percent    [LB][RB] Switch Highlight Object    [A] Refresh Timings    [View] Exit";
    }
    else
    {
        controls = L"[LThumb] Toward/Away   [RThumb]: Orbit Camera   [View] Exit";
    }

    // Draw Controllers
    textPos.x = float(safe.left);
    textPos.y = float(safe.bottom) - 2.0f * m_smallFont->GetLineSpacing();
    DX::DrawControllerString(m_hudBatch.get(), m_smallFont.get(), m_ctrlFont.get(), controls, textPos, textColor);

    m_hudBatch->End();
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
void Sample::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device);
    m_profiler = std::make_unique<DX::GPUTimer>(device, m_deviceResources->GetCommandQueue());
    m_allocator = std::make_unique<ATG::TransientAllocator>(device, Gibibytes<uint64_t>(8), true);

    // State objects
    m_commonStates = std::make_unique<DirectX::CommonStates>(device);

    // Create heap
    m_srvPile = std::make_unique<DescriptorPile>(device,
        128,
        DescriptorHeapIndex::SRV_Count);
    
    // Load models from disk.
    m_models.resize(_countof(s_modelPaths));
    for (int i = 0; i < m_models.size(); ++i)
    {
        m_models[i] = Model::CreateFromSDKMESH(device, s_modelPaths[i]);
    }

    ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin();

    // Optimize meshes for rendering
    for (int i = 0; i < m_models.size(); ++i)
    {
        m_models[i]->LoadStaticBuffers(device, resourceUpload);
    }

    // Upload textures to GPU.
    m_textureFactory = std::make_unique<EffectTextureFactory>(device, resourceUpload, m_srvPile->Heap());

    auto texOffsets = std::vector<size_t>(m_models.size());
    for (int i = 0; i < m_models.size(); ++i)
    {
        size_t _;
        m_srvPile->AllocateRange(m_models[i]->textureNames.size(), texOffsets[i], _);

        m_models[i]->LoadTextures(*m_textureFactory, int(texOffsets[i]));
    }

    // HUD
    auto backBufferRts = RenderTargetState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());
    auto spritePSD = SpriteBatchPipelineStateDescription(backBufferRts, &CommonStates::AlphaBlend);
    m_hudBatch = std::make_unique<SpriteBatch>(device, resourceUpload, spritePSD);

    auto finished = resourceUpload.End(m_deviceResources->GetCommandQueue());
    finished.wait();


    //-------------------------------------------------------
    // Instantiate objects from basic scene definition.

    auto effectFactory = EffectFactory(m_srvPile->Heap(), m_commonStates->Heap());

    auto objectRTState = RenderTargetState(c_colorFormat, c_depthFormat);
    auto objectPSD = EffectPipelineStateDescription(
        nullptr,
        CommonStates::Opaque,
        CommonStates::DepthDefault,
        CommonStates::CullCounterClockwise,
        objectRTState,
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);

    m_scene.resize(_countof(s_sceneDefinition));
    for (int i = 0; i < m_scene.size(); i++)
    {
        size_t index = s_sceneDefinition[i].modelIndex;

        assert(index < m_models.size());
        auto& model = *m_models[index];

        m_scene[i].world = s_sceneDefinition[i].world;
        m_scene[i].model = &model;
        m_scene[i].effects = model.CreateEffects(effectFactory, objectPSD, objectPSD, int(texOffsets[index]));

        std::for_each(
            m_scene[i].effects.begin(), 
            m_scene[i].effects.end(), 
            [&](std::shared_ptr<IEffect>& e) 
            {
                static_cast<BasicEffect*>(e.get())->SetEmissiveColor(XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f));
            });
    }

    auto outlineRtState = RenderTargetState(c_colorFormat, DXGI_FORMAT_UNKNOWN);
    auto outlinePSD = EffectPipelineStateDescription(
        &VertexPositionNormalTexture::InputLayout,
        CommonStates::Opaque,
        CommonStates::DepthNone,
        CommonStates::CullCounterClockwise,
        outlineRtState,
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);

    m_emissiveEffect = std::make_unique<BasicEffect>(device, EffectFlags::None, outlinePSD);
    m_emissiveEffect->SetDiffuseColor(DirectX::Colors::Green);

    //----------------------------------------
    // Create post processing resources

    m_fullScreenTri = GeometricPrimitive::CreateCustom(s_triVertex, s_triIndex);

    auto postRtState = RenderTargetState(c_colorFormat, DXGI_FORMAT_UNKNOWN);
    m_blurEffect = std::make_unique<BasicPostProcess>(device, postRtState, BasicPostProcess::GaussianBlur_5x5);
    m_blurEffect->SetGaussianParameter(12.0f);

    auto combinePSD = EffectPipelineStateDescription(
        &VertexPositionNormalTexture::InputLayout,
        CommonStates::NonPremultiplied,
        CommonStates::DepthNone,
        CommonStates::CullNone,
        postRtState,
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);

    // Manipulate BasicEffect shader's math to perform direct, single-color blend.
    m_alphaCompositeEffect = std::make_unique<BasicEffect>(device, EffectFlags::Texture, combinePSD);
    m_alphaCompositeEffect->SetDiffuseColor(XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f));
    m_alphaCompositeEffect->SetAlpha(0.6f);

    auto backBufferRtState = RenderTargetState(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN);

    m_bloomExtractEffect = std::make_unique<BasicPostProcess>(device, postRtState, BasicPostProcess::BloomExtract);
    m_bloomExtractEffect->SetBloomExtractParameter(0.9f);

    m_bloomCombineEffect = std::make_unique<DualPostProcess>(device, postRtState, DualPostProcess::BloomCombine);
    m_bloomCombineEffect->SetBloomCombineParameters(2.5f, 1.0f, 1.0f, 1.0f);

    m_bloomBlurEffect = std::make_unique<BasicPostProcess>(device, postRtState, BasicPostProcess::BloomBlur);
    m_tonemapEffect = std::make_unique<ToneMapPostProcess>(device, backBufferRtState, ToneMapPostProcess::Reinhard, ToneMapPostProcess::SRGB);
    m_esramVisualizeEffect = std::make_unique<EsramVisualizeEffect>(device);
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();
    const auto size = m_deviceResources->GetOutputSize();

    // Calculate display dimensions.
    m_displayWidth = size.right - size.left;
    m_displayHeight = size.bottom - size.top;
    
    XMINT2 visSize = { int(m_displayWidth * 0.8f), int(m_displayHeight * 0.15f) };
    XMINT2 visPad = { int(m_displayWidth * 0.1f), int(m_displayHeight * 0.17f) };

    // Initialize the constant visualization values.
    m_visData.bounds.x = std::max<int>(m_displayWidth / 2 - visSize.x, visPad.x);
    m_visData.bounds.y = m_displayHeight - visPad.y - visSize.y;
    m_visData.bounds.z = std::min<int>(m_displayWidth / 2 + visSize.x, m_displayWidth - visPad.x);
    m_visData.bounds.w = m_displayHeight - visPad.y;

    m_visData.backgroundColor = Vector4(DirectX::Colors::DimGray);
    m_visData.backgroundBlend = 0.25f;
    m_visData.foregroundBlend = 0.75;

    m_visData.pageCount = c_esramPageCount;
    m_visData.flashRate = 0.5f;
    m_visData.factor = 0.15f;

    for (int i = 0; i < ST_Count; ++i)
    {
        m_visData.textures[i].color = s_visualizeColors[i];
    }

    // Set hud sprite viewport
    m_hudBatch->SetViewport(m_deviceResources->GetScreenViewport());

    // Set camera parameters.
    m_proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, float(m_displayWidth) / float(m_displayHeight), 0.1f, 500.0f);

    // Begin uploading texture resources
    {
        ResourceUploadBatch resourceUpload(device);
        resourceUpload.Begin();

        m_smallFont = std::make_unique<SpriteFont>(device, resourceUpload,
            L"SegoeUI_18.spritefont",
            m_srvPile->GetCpuHandle(DescriptorHeapIndex::SRV_Font),
            m_srvPile->GetGpuHandle(DescriptorHeapIndex::SRV_Font));

        m_ctrlFont = std::make_unique<SpriteFont>(device, resourceUpload,
             L"XboxOneControllerLegendSmall.spritefont",
            m_srvPile->GetCpuHandle(DescriptorHeapIndex::SRV_CtrlFont),
            m_srvPile->GetGpuHandle(DescriptorHeapIndex::SRV_CtrlFont));

        auto finished = resourceUpload.End(m_deviceResources->GetCommandQueue());
        finished.wait();
    }

    m_colorDesc = ATG::TransientDesc
    {
        CD3DX12_RESOURCE_DESC::Tex2D(
            c_colorFormat,                              // DXGI_FORMAT Format;
            UINT(m_displayWidth),                       // UINT Width;
            UINT(m_displayHeight),                      // UINT Height;
            1,                                          // UINT ArraySize;
            1,                                          // UINT MipLevels;
            1,                                          // UINT SampleCount;
            UINT(D3D11_STANDARD_MULTISAMPLE_PATTERN),   // UINT Quality;
            D3D12_RESOURCE_FLAG_NONE,                   // D3D12_RESOURCE_FLAGS MiscFlags;
            D3D12_TEXTURE_LAYOUT_UNKNOWN,               // D3D12_TEXTURE_LAYOUT Layout;
            c_pageSizeBytes                             // UINT64 Alignment;
        ),
        CD3DX12_CLEAR_VALUE(c_colorFormat, ATG::Colors::Background),
        BindFlags::RTV | BindFlags::SRV | BindFlags::UAV
    };

    m_depthDesc = ATG::TransientDesc
    {
        CD3DX12_RESOURCE_DESC::Tex2D(
            c_depthFormat,                              // DXGI_FORMAT Format;
            UINT(m_displayWidth),                       // UINT Width;
            UINT(m_displayHeight),                      // UINT Height;
            1,                                          // UINT ArraySize;
            1,                                          // UINT MipLevels;
            1,                                          // UINT SampleCount;
            UINT(D3D11_STANDARD_MULTISAMPLE_PATTERN),   // UINT Quality;
            D3D12_RESOURCE_FLAG_NONE,                   // D3D12_RESOURCE_FLAGS MiscFlags;
            D3D12_TEXTURE_LAYOUT_UNKNOWN,               // D3D12_TEXTURE_LAYOUT Layout;
            c_pageSizeBytes                             // UINT64 Alignment;
        ),
        CD3DX12_CLEAR_VALUE(c_depthFormat, 1.0f, 0),
        BindFlags::DSV
    };

    float zeros[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_outlineDesc = m_colorDesc;
    m_outlineDesc.clear = CD3DX12_CLEAR_VALUE(m_outlineDesc.d3dDesc.Format, zeros);
    m_outlineDesc.flags = BindFlags::RTV | BindFlags::SRV;

    m_bloomDesc = m_colorDesc;
    m_bloomDesc.d3dDesc.Flags |= D3D12XBOX_RESOURCE_FLAG_DENY_COMPRESSION_DATA;
    m_bloomDesc.flags = BindFlags::RTV | BindFlags::SRV;
}
#pragma endregion

TransientResource Sample::AcquireTransientTexture(ID3D12GraphicsCommandList* list, const TransientDesc& desc, const D3D12_RESOURCE_STATES& initialState, SceneTexture tex)
{
    int pages = int(float(desc.getPageCount()) * m_esramRatios[tex]);

    auto token = esramToken(pages);
    return m_allocator->Acquire(list, desc, initialState, &token, 1, s_textureNames[tex]);
}

void Sample::UpdateVisualizerRanges(const ResourceHandle(&resources)[ST_Count])
{
    // Calculate ESRAM page ranges for each texture
    std::vector<Range> ranges;

    for (int i = 0; i < _countof(resources); ++i)
    {
        m_allocator->GetEsramRanges(resources[i], ranges);

        m_visData.textures[i].pageRangeCount = int(ranges.size());
        for (int j = 0; j < ranges.size(); ++j)
        {
            m_visData.textures[i].pageRanges[j].x = ranges[j].start;
            m_visData.textures[i].pageRanges[j].y = ranges[j].start + ranges[j].count;
        }
    }
}

void Sample::UpdateVisualizerTimings()
{
    m_visData.duration = (float)m_profiler->GetElapsedMS(TI_Frame);

    // Calculate Timings
    float colorTime = (float)m_profiler->GetElapsedMS(TI_Color);
    float depthTime = (float)m_profiler->GetElapsedMS(TI_Depth);
    float sceneTime = (float)m_profiler->GetElapsedMS(TI_Scene);
    float outlineTime = (float)m_profiler->GetElapsedMS(TI_Outline);
    float bloomTime = (float)m_profiler->GetElapsedMS(TI_Bloom);

    float outlineStart = sceneTime;
    float bloomStart = outlineStart + outlineTime;

    m_visData.textures[ST_Color].timeRange = XMFLOAT2(0.0f, colorTime);
    m_visData.textures[ST_Depth].timeRange = XMFLOAT2(0.0f, depthTime);
    m_visData.textures[ST_Outline0].timeRange = XMFLOAT2(outlineStart, outlineStart + outlineTime);
    m_visData.textures[ST_Outline1].timeRange = m_visData.textures[ST_Outline0].timeRange;
    m_visData.textures[ST_Bloom0].timeRange = XMFLOAT2(bloomStart, bloomStart + bloomTime);
    m_visData.textures[ST_Bloom1].timeRange = m_visData.textures[ST_Bloom0].timeRange;
}
