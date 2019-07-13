//--------------------------------------------------------------------------------------
// D3D12RaytracingAO.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "CommonStates.h"
#include "DeviceResources.h"
#include "FindMedia.h"
#include "Menus.h"
#include "SimpleMath.h"

using namespace DirectX;

// Setup descriptor heaps.
void Menus::CreateDescriptorHeaps()
{
    auto device = m_deviceResources->GetD3DDevice();

    // Allocate a csu heap.
    {
        const uint32_t c_csuCount = MenuCSUDesc::CSUCount;
        m_csuDescriptors = std::make_unique<DescriptorHeap>(
            device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, c_csuCount);
    }
}

void Menus::Setup(std::shared_ptr<DX::DeviceResources> pDeviceResources)
{
    // Store passed vars.
    {
        m_deviceResources = pDeviceResources;
    }

    auto device = m_deviceResources->GetD3DDevice();

    // Setup heaps
    {
        CreateDescriptorHeaps();
    }

    // Upload Fonts.
    ResourceUploadBatch uploadBatch(device);
    {
        uploadBatch.Begin();

        // Setup text.
        {
            RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), DXGI_FORMAT_UNKNOWN);
            SpriteBatchPipelineStateDescription pd(rtState, &CommonStates::AlphaBlend);

            m_batch = std::make_unique<SpriteBatch>(device, uploadBatch, pd);

            wchar_t strFilePath[MAX_PATH] = {};
            DX::FindMediaFile(strFilePath, MAX_PATH, L"SegoeUI_18.spritefont");
            m_smallFont = std::make_unique<SpriteFont>(
                device,
                uploadBatch,
                strFilePath,
                m_csuDescriptors->GetCpuHandle(MenuCSUDesc::SRVSmallFont),
                m_csuDescriptors->GetGpuHandle(MenuCSUDesc::SRVSmallFont)
                );
        }

        auto finish = uploadBatch.End(m_deviceResources->GetCommandQueue());
        finish.wait();
    }

    // Create backing square.
    {
        m_primitiveBatch = std::make_unique<PrimitiveBatch<VertexPosition>>(device);

        RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), DXGI_FORMAT_UNKNOWN);
        EffectPipelineStateDescription pd(
            &VertexPosition::InputLayout,
            CommonStates::AlphaBlend,
            CommonStates::DepthNone,
            CommonStates::CullNone,
            rtState
        );

        m_basicEffect = std::make_unique<BasicEffect>(device, EffectFlags::None, pd);
    }
}

void Menus::OnSizeChanged()
{
    auto output = m_deviceResources->GetOutputSize();
    float screenWidth = float(output.right - output.left);
    float screenHeight = float(output.bottom - output.top);

    m_batch->SetViewport(m_deviceResources->GetScreenViewport());

    m_basicEffect->SetProjection(
        XMMatrixOrthographicOffCenterRH(
            0,
            screenWidth,
            screenHeight,
            0,
            0,
            1)
    );
}

void Menus::DrawMainMenu(bool center)
{
    auto commandList = m_deviceResources->GetCommandList();

    auto output = m_deviceResources->GetOutputSize();
    float screenWidth = float(output.right - output.left);
    float screenHeight = float(output.bottom - output.top);

    auto safe = SimpleMath::Viewport::ComputeTitleSafeArea(output.right, output.bottom);
    auto startOffset = XMFLOAT2(0.f, 0.f);
    auto offset = startOffset;

    std::vector<std::tuple<std::wstring, XMFLOAT2, XMVECTORF32>> deferredDraw;

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Main Menu");

    // Skip if help is not up.
    if (!m_showHelp)
        return;

    float wExtent = 0.f;
    m_batch->Begin(commandList);
    {
        m_basicEffect->SetAlpha(.8f);
        m_basicEffect->SetDiffuseColor({ 0.f, 0.f, 0.f, 1.f });

        m_basicEffect->Apply(commandList);
        m_primitiveBatch->Begin(commandList);
        {
            // Text.
            {
                unsigned int index = 0;

                auto RenderString =
                    [this, &deferredDraw, &offset, &wExtent, &index](
                        wchar_t const* string0,
                        wchar_t const* string1 = L"",
                        Option* option = nullptr,
                        bool isValue = false,
                        bool isOption = false)
                {
                    std::wstringstream wStr;
                    wStr << string0 << string1;

                    if (isValue)
                    {
                        if (option->IsInt())
                            wStr << std::fixed << std::setprecision(0) << option->Value();
                        else
                            wStr << std::fixed << std::setprecision(2) << option->Value();
                    }

                    deferredDraw.push_back(
                        std::make_tuple(
                            wStr.str(),
                            offset,
                            !isOption ? ATG::Colors::OffWhite
                            : (index == selection)
                            ? (option)
                            ? option->SlectionColor() : ATG::Colors::White
                            : ATG::Colors::DarkGrey
                        )
                    );

                    offset.y += m_smallFont->GetLineSpacing();
                    wExtent = std::max(
                        XMVectorGetByIndex(m_smallFont->MeasureString(wStr.str().c_str()), 0),
                        wExtent
                    );

                    if (isOption)
                        index++;
                };

                {
                    // AO
                    {
                        RenderString(L"AO:");
                        RenderString(L"  Attenuation:");
                        RenderString(L"      Distance: ", L"", &m_aoDistance, true, true);
                        RenderString(L"      Falloff: ", L"", &m_aoFalloff, true, true);
                        RenderString(L"  Sampling: ");
                        RenderString(L"      Num Samples (n^2): ", L"", &m_aoNumSamples, true, true);
                        RenderString(L"      Type: ", m_aoSampleNames[unsigned int(m_aoSampleType.Value())].c_str(), nullptr, false, true);
                    }

                    // SSAO
                    {
                        RenderString(L"SSAO:");
                        RenderString(L"  Tolerance:");
                        RenderString(L"     Noise Threshold (log10): ", L"", &m_ssaoNoiseFilterTolerance, true, true);
                        RenderString(L"     Blur Tolerance (log10): ", L"", &m_ssaoBlurTolerance, true, true);
                        RenderString(L"     Upsample Tolerance (log10): ", L"", &m_ssaoUpsampleTolerance, true, true);
                        RenderString(L"  Misc: ");
                        RenderString(L"     Normal Factor: ", L"", &m_ssaoNormalMultiply, true, true);
                    }
                }

                // Draw.
                {
                    // Apply offset based on mode.
                    if (center)
                    {
                        startOffset = {
                            (screenWidth - wExtent) / 2.f,
                            (screenHeight - offset.y) / 2.f
                        };
                    }
                    else
                    {
                        startOffset = { float(safe.left), float(safe.top) };
                    }


                    offset.x += startOffset.x;
                    offset.y += startOffset.y;

                    for (auto& el : deferredDraw)
                    {
                        m_smallFont->DrawString(
                            m_batch.get(),
                            std::get<0>(el).c_str(),
                            XMFLOAT2(
                                std::get<1>(el).x + startOffset.x,
                                std::get<1>(el).y + startOffset.y),
                            std::get<2>(el)
                        );
                    }
                }
            }

            // Backing.
            {
                m_basicEffect->Apply(commandList);

                m_primitiveBatch->DrawQuad(
                    { { startOffset.x - m_border, startOffset.y - m_border, 0, 1 } },
                    { { startOffset.x + wExtent + m_border, startOffset.y - m_border, 0, 1 } },
                    { { startOffset.x + wExtent + m_border, offset.y + m_border, 0, 1 } },
                    { { startOffset.x - m_border, offset.y + m_border, 0, 1 } }
                );
            }
        }
        m_primitiveBatch->End();
    }
    m_batch->End();


    PIXEndEvent(commandList);
}

void Menus::DrawCenterLine()
{
    auto commandList = m_deviceResources->GetCommandList();

    auto output = m_deviceResources->GetOutputSize();
    float screenWidth = float(output.right - output.left);
    float screenHeight = float(output.bottom - output.top);

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Center Line");

    // Center Line
    {
        m_basicEffect->SetAlpha(1.0f);
        m_basicEffect->SetDiffuseColor({ .1f, .1f, .1f, 1.f });

        m_basicEffect->Apply(commandList);
        m_primitiveBatch->Begin(commandList);
        {
            float start = float(screenWidth - m_lineThickness) / 2.f;
            float step = float(m_lineThickness);
            m_primitiveBatch->DrawQuad(
                { { start, 0, 0, 1 } },
                { { start + step, 0, 0, 1 } },
                { { start + step, float(screenHeight), 0, 1 } },
                { { start, float(screenHeight), 0, 1 } }
            );
        }
        m_primitiveBatch->End();
    }

    PIXEndEvent(commandList);
}

void Menus::DrawSplitLabels()
{
    auto commandList = m_deviceResources->GetCommandList();

    auto output = m_deviceResources->GetOutputSize();
    float screenWidth = float(output.right - output.left);
    float screenHeight = float(output.bottom - output.top);

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Split Menu");

    // Labels
    {
        m_batch->Begin(commandList);
        {
            m_basicEffect->SetAlpha(1.f);
            m_basicEffect->SetDiffuseColor({ .2f, .2f, .2f, 1.f });
            m_basicEffect->Apply(commandList);
            m_primitiveBatch->Begin(commandList);
            {
                // SSAO.
                {
                    auto dim = m_smallFont->MeasureString(L"SSAO");
                    XMFLOAT2 off = XMFLOAT2(
                        (float(screenWidth - m_lineThickness) / 2.f
                            - XMVectorGetByIndex(dim, 0)) / 2.f
                        + float(m_lineThickness) / 2.f
                        - m_border,
                        float(screenHeight - XMVectorGetByIndex(dim, 1) - m_border)
                    );

                    m_primitiveBatch->DrawQuad(
                        { { off.x - m_border, off.y - m_border, 0, 1 } },
                        { { off.x + XMVectorGetByIndex(dim, 0) + m_border, off.y - m_border, 0, 1 } },
                        { { off.x + XMVectorGetByIndex(dim, 0) + m_border, off.y + XMVectorGetByIndex(dim, 1) + m_border, 0, 1 } },
                        { { off.x - m_border, off.y +XMVectorGetByIndex(dim, 1) + m_border, 0, 1 } }
                    );
                    m_smallFont->DrawString(
                        m_batch.get(),
                        L"SSAO",
                        off,
                        ATG::Colors::White
                    );
                }

                // AO.
                {
                    auto dim = m_smallFont->MeasureString(L"AO");
                    XMFLOAT2 off = XMFLOAT2(
                        (float(screenWidth - m_lineThickness) / 2.f
                            - XMVectorGetByIndex(dim, 0)) / 2.f +
                        float(screenWidth + m_lineThickness) / 2.f - m_border,
                        float(screenHeight -XMVectorGetByIndex(dim, 1) - m_border)
                    );

                    m_primitiveBatch->DrawQuad(
                        { { off.x - m_border, off.y - m_border, 0, 1 } },
                        { { off.x + XMVectorGetByIndex(dim, 0) + m_border, off.y - m_border, 0, 1 } },
                        { { off.x + XMVectorGetByIndex(dim, 0) + m_border, off.y + XMVectorGetByIndex(dim, 1) + m_border, 0, 1 } },
                        { { off.x - m_border, off.y +XMVectorGetByIndex(dim, 1) + m_border, 0, 1 } }
                    );
                    m_smallFont->DrawString(
                        m_batch.get(),
                        L"AO",
                        off,
                        ATG::Colors::White
                    );
                }
            }
            m_primitiveBatch->End();
            m_batch->End();
        }
    }

    PIXEndEvent(commandList);
}

void Menus::DrawLabel()
{
    auto commandList = m_deviceResources->GetCommandList();

    auto output = m_deviceResources->GetOutputSize();
    float screenWidth = float(output.right - output.left);
    float screenHeight = float(output.bottom - output.top);

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Label Menu");

    m_batch->Begin(commandList);
    {
        m_basicEffect->SetAlpha(1.f);
        m_basicEffect->SetDiffuseColor({ .2f, .2f, .2f, 1.f });
        m_basicEffect->Apply(commandList);
        m_primitiveBatch->Begin(commandList);
        {
            std::wstring label;

            if (m_lightingModel >= MenuLightingModel::LightingModelCount)
            {
                throw std::exception("DrawLabel");
            }

            if (m_lightingModel == MenuLightingModel::AO)
                label = L"AO";
            else if (m_lightingModel == MenuLightingModel::SSAO)
                label = L"SSAO";

            auto dim = m_smallFont->MeasureString(label.c_str());
            XMFLOAT2 off = XMFLOAT2(
                (screenWidth - XMVectorGetByIndex(dim, 0)) / 2.f,
                (screenHeight -XMVectorGetByIndex(dim, 1) - m_border)
            );

            m_primitiveBatch->DrawQuad(
                { { off.x - m_border, off.y - m_border, 0, 1 } },
                { { off.x + XMVectorGetByIndex(dim, 0) + m_border, off.y - m_border, 0, 1 } },
                { { off.x + XMVectorGetByIndex(dim, 0) + m_border, off.y +XMVectorGetByIndex(dim, 1) + m_border, 0, 1 } },
                { { off.x - m_border, off.y +XMVectorGetByIndex(dim, 1) + m_border, 0, 1 } }
            );
            m_smallFont->DrawString(
                m_batch.get(),
                label.c_str(),
                off,
                ATG::Colors::White
            );
        }
        m_primitiveBatch->End();
        m_batch->End();
    }

    PIXEndEvent(commandList);
}

void Menus::DrawFrameRate(uint32_t fps)
{
    if (!m_showFPS)
        return;

    auto commandList = m_deviceResources->GetCommandList();

    auto output = m_deviceResources->GetOutputSize();
    float screenWidth = float(output.right - output.left);

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Label Menu");

    m_batch->Begin(commandList);
    {
        m_basicEffect->SetAlpha(1.f);
        m_basicEffect->SetDiffuseColor({ .2f, .2f, .2f, 1.f });
        m_basicEffect->Apply(commandList);
        m_primitiveBatch->Begin(commandList);
        {
            auto dim = m_smallFont->MeasureString(L"FPS: 0000");
            XMFLOAT2 off = XMFLOAT2(
                screenWidth - XMVectorGetByIndex(dim, 0) - m_border,
                float(m_border)
            );

            std::wstringstream label;
            label << "FPS: " << fps;
            auto drawDim = m_smallFont->MeasureString(label.str().c_str());
            XMFLOAT2 drawOff = XMFLOAT2(
                off.x + float(XMVectorGetByIndex(dim, 0) - XMVectorGetByIndex(drawDim, 0)) / 2.f,
                off.y
            );

            m_primitiveBatch->DrawQuad(
                { { off.x - m_border, off.y - m_border, 0, 1 } },
                { { off.x + XMVectorGetByIndex(dim, 0) + m_border, off.y - m_border, 0, 1 } },
                { { off.x + XMVectorGetByIndex(dim, 0) + m_border, off.y +XMVectorGetByIndex(dim, 1) + m_border, 0, 1 } },
                { { off.x - m_border, off.y +XMVectorGetByIndex(dim, 1) + m_border, 0, 1 } }
            );
            m_smallFont->DrawString(
                m_batch.get(),
                label.str().c_str(),
                drawOff,
                ATG::Colors::Green
            );
        }
        m_primitiveBatch->End();
        m_batch->End();
    }

    PIXEndEvent(commandList);
}

void Menus::Draw(uint32_t fps, bool halfLine)
{
    auto commandList = m_deviceResources->GetCommandList();
    auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
    auto dsvDescriptor = m_deviceResources->GetDepthStencilView();

    ID3D12DescriptorHeap* ppHeaps[] = { m_csuDescriptors->Heap() };
    commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);

    // Half line
    if (halfLine)
    {
        DrawSplitLabels();
        DrawCenterLine();
        DrawFrameRate(fps);
        DrawMainMenu(true);
    }
    else
    {
        DrawLabel();
        DrawFrameRate(fps);
        DrawMainMenu(false);
    }
}

bool Menus::ProcessKeys(DirectX::Keyboard::KeyboardStateTracker& keyboard)
{
    bool update = false;

    if (keyboard.IsKeyPressed(Keyboard::F1))
    {
        m_showHelp = !m_showHelp;
    }
    else if (keyboard.IsKeyPressed(Keyboard::Tab))
    {
        m_lightingModel++;

        if (m_lightingModel == MenuLightingModel::LightingModelCount)
            m_lightingModel = 0;
    }
    else if (keyboard.IsKeyPressed(Keyboard::F))
    {
        m_showFPS = !m_showFPS;
    }

    // Do not capture.
    if (!m_showHelp)
        return update;

    if (keyboard.IsKeyPressed(Keyboard::Up))
    {
        selection--;

        if (selection == std::numeric_limits<unsigned int>::max())
            selection = _countof(m_optionList) - 1;
    }
    else if (keyboard.IsKeyPressed(Keyboard::Down))
    {
        selection++;

        if (_countof(m_optionList) <= selection)
            selection = 0;
    }
    else if (keyboard.IsKeyPressed(Keyboard::Left))
    {
        delta = -1.f;
    }
    else if (keyboard.IsKeyPressed(Keyboard::Right))
    {
        delta = 1.f;
    }
    else if (keyboard.IsKeyReleased(Keyboard::Left)
        || keyboard.IsKeyReleased(Keyboard::Right))
    {
        delta = .0f;
    }
    else if (keyboard.IsKeyPressed(Keyboard::R))
    {
        m_aoDistance.Reset();
        m_aoFalloff.Reset();
        m_aoNumSamples.Reset();
        m_aoSampleType.Reset();
        m_ssaoNoiseFilterTolerance.Reset();
        m_ssaoBlurTolerance.Reset();
        m_ssaoUpsampleTolerance.Reset();
        m_ssaoNormalMultiply.Reset();

        update = true;
    }

    if (0.f < delta)
    {
        update = m_optionList[selection]->Increment();
    }
    else if (delta < 0.f)
    {
        update = m_optionList[selection]->Decrement();
    }

    return update;
}