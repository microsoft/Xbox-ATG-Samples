//--------------------------------------------------------------------------------------
// HLSLSymbols.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "HLSLSymbols.h"

#include "ATGColors.h"
#include "ReadData.h"

extern void ExitSample() noexcept;

using namespace DirectX;

using Microsoft::WRL::ComPtr;

Sample::Sample() noexcept(false) :
    m_frame(0)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN);
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(IUnknown* window)
{
    m_gamePad = std::make_unique<GamePad>();

    m_deviceResources->SetWindow(window);

    m_deviceResources->CreateDeviceResources();  
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();
}

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
    m_frame++;
}

// Updates the world.
void Sample::Update(DX::StepTimer const& /*timer*/)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);

        if (pad.IsViewPressed())
        {
            ExitSample();
        }
    }
    else
    {
        m_gamePadButtons.Reset();
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

    // Prepare the command list to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    auto size = m_deviceResources->GetOutputSize();
    RECT safeRect = SimpleMath::Viewport::ComputeTitleSafeArea(size.right, size.bottom);

    XMFLOAT2 positionTitle = { (float) safeRect.left, (float) safeRect.top, };
    m_spriteBatch->Begin(commandList);
    m_spriteBatch->SetViewport(m_deviceResources->GetScreenViewport());
    m_fontDescription->DrawString(m_spriteBatch.get(), L"HLSLSymbols sample", positionTitle);
    m_spriteBatch->End();

    auto yMargin = 20.0f;

    D3D12_VIEWPORT viewportExample;
    viewportExample.TopLeftX = positionTitle.x;
    viewportExample.TopLeftY = positionTitle.y + 4.0f * yMargin;
    viewportExample.Width = 100.0f;
    viewportExample.Height = 100.0f;
    viewportExample.MinDepth = 0.0f;
    viewportExample.MaxDepth = 1.0;

    auto DrawExample = [commandList, this, &viewportExample, yMargin] (const wchar_t* name, 
        ID3D12PipelineState* pso)-> void
    {
        static const VertexPositionColor c_vertexData[3] =
        {
            { 
                XMFLOAT3(0.0f, 1.0f, 1.0f),
                SimpleMath::Vector4(Colors::Red),
            },  // Top / Red
            { 
                XMFLOAT3(1.0f, -1.0f, 1.0f),
                SimpleMath::Vector4(Colors::Green),
            },  // Right / Green
            { 
                XMFLOAT3(-1.0f, -1.0f, 1.0f),
                SimpleMath::Vector4(Colors::Blue),
            }   // Left / Blue
        };

        PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, name);
        commandList->SetGraphicsRootSignature(m_rootSignature.Get());
        commandList->SetPipelineState(pso);
        commandList->RSSetViewports(1, &viewportExample);
        m_primitiveBatch->Begin(commandList);
        m_primitiveBatch->DrawTriangle(c_vertexData[0], c_vertexData[1], c_vertexData[2]);
        m_primitiveBatch->End();
        auto vp = m_deviceResources->GetScreenViewport();
        commandList->RSSetViewports(1, &vp);
        PIXEndEvent(commandList);
    };

    auto DrawDescription = [this, commandList, &viewportExample, yMargin] (const wchar_t* description)->void
    {
        XMFLOAT2 positionText = {
            viewportExample.TopLeftX + viewportExample.Width + 20.0f, 
            viewportExample.TopLeftY + viewportExample.Height / 2.0f,
        };

        m_spriteBatch->Begin(commandList);
        m_spriteBatch->SetViewport(m_deviceResources->GetScreenViewport());
        m_fontDescription->DrawString(m_spriteBatch.get(), description, positionText);
        m_spriteBatch->End();
    };

    // Draw triangle with each variant of the pixel shader.
    // See comments at the CreateGraphicsPipelineState calls later in this file
    DrawExample(L"EmbeddedPdb", m_pipelineStateEmbeddedPdb.Get());
    DrawDescription(L"Symbols are embedded in the shader binary.");
    viewportExample.TopLeftY += viewportExample.Height + yMargin;
    DrawExample(L"ManualPdb", m_pipelineStateManualPdb.Get());
    DrawDescription(L"Symbols were saved to a user-specified pdb file, and the full path to that file is embedded in the shader binary.");
    viewportExample.TopLeftY += viewportExample.Height + yMargin;
    DrawExample(L"AutoPdb", m_pipelineStateAutoPdb.Get());
    DrawDescription(L"Symbols were saved to a pdb file with an auto-generated filename, and the full path to that file is embedded in the shader binary.");
    viewportExample.TopLeftY += viewportExample.Height + yMargin;
    DrawExample(L"AutoPdbNoPath", m_pipelineStateAutoPdbNoPath.Get());
    DrawDescription(L"Symbols were saved to a pdb file with an auto-generated filename, and the user must manually set the pdb path in PIX.");
    viewportExample.TopLeftY += viewportExample.Height + yMargin;
    DrawExample(L"StrippedPdb", m_pipelineStateStrippedPdb.Get());
    DrawDescription(L"Symbols were stripped entirely and not saved.");
    viewportExample.TopLeftY += viewportExample.Height + yMargin;

    PIXEndEvent(commandList);

    // Show the new frame.
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    m_graphicsMemory->Commit(m_deviceResources->GetCommandQueue());
    PIXEndEvent();
}

// Helper method to clear the back buffers.
void Sample::Clear()
{
    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.
    auto rtvDescriptor = m_deviceResources->GetRenderTargetView();

    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, nullptr);
    commandList->ClearRenderTargetView(rtvDescriptor, ATG::Colors::Background, 0, nullptr);

    // Set the viewport and scissor rect.
    auto viewport = m_deviceResources->GetScreenViewport();
    auto scissorRect = m_deviceResources->GetScissorRect();
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);

    PIXEndEvent(commandList);
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
    auto device = m_deviceResources->GetD3DDevice();

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device);

    // Create root signature.
    auto vertexShaderBlob = DX::ReadData(L"VertexShader.cso");

    // Xbox One best practice is to use HLSL-based root signatures to support shader precompilation.

    DX::ThrowIfFailed(
        device->CreateRootSignature(0, vertexShaderBlob.data(), vertexShaderBlob.size(),
            IID_GRAPHICS_PPV_ARGS(m_rootSignature.ReleaseAndGetAddressOf())));

    // Create the pipeline state, which includes loading shaders.
    // Describe and create the graphics pipeline state object (PSO).
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = VertexPositionColor::InputLayout;
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = { vertexShaderBlob.data(), vertexShaderBlob.size() };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.DSVFormat = m_deviceResources->GetDepthBufferFormat();
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = m_deviceResources->GetBackBufferFormat();
    psoDesc.SampleDesc.Count = 1;

    // The standard pixel shader, compiled with fxc /Zi.
    {
        auto pixelShaderBlob = DX::ReadData(L"PixelShader_Fxc_EmbeddedPdb.cso");
        psoDesc.PS = { pixelShaderBlob.data(), pixelShaderBlob.size() };

        DX::ThrowIfFailed(
            device->CreateGraphicsPipelineState(&psoDesc,
                IID_GRAPHICS_PPV_ARGS(m_pipelineStateEmbeddedPdb.ReleaseAndGetAddressOf())));
    }

    // The pixel shader compiled with D3DCompile and D3DCOMPILE_DEBUG, and with the symbols stripped
    // to a manually specified pdb filename.
    {
        auto pixelShaderBlob = DX::ReadData(L"PixelShader_D3DCompile_ManualPdb.cso");
        psoDesc.PS = { pixelShaderBlob.data(), pixelShaderBlob.size() };

        DX::ThrowIfFailed(
            device->CreateGraphicsPipelineState(&psoDesc,
                IID_GRAPHICS_PPV_ARGS(m_pipelineStateManualPdb.ReleaseAndGetAddressOf())));
    }

    // The pixel shader compiled with D3DCompile and D3DCOMPILE_DEBUG, and with the symbols stripped
    // to an automatically chosen pdb filename (based on the semantic hash).
    {
        auto pixelShaderBlob = DX::ReadData(L"PixelShader_D3DCompile_AutoPdb.cso");
        psoDesc.PS = { pixelShaderBlob.data(), pixelShaderBlob.size() };

        DX::ThrowIfFailed(
            device->CreateGraphicsPipelineState(&psoDesc,
                IID_GRAPHICS_PPV_ARGS(m_pipelineStateAutoPdb.ReleaseAndGetAddressOf())));
    }

    // The pixel shader compiled with D3DCompile and D3DCOMPILE_DEBUG, and with the symbols stripped
    // to an automatically chosen pdb filename (based on the semantic hash).
    // The pdb pathname is stripped from the binary, so that it must be manually selected in PIX.
    {
        auto pixelShaderBlob = DX::ReadData(L"PixelShader_D3DCompile_AutoPdb_NoPath.cso");
        psoDesc.PS = { pixelShaderBlob.data(), pixelShaderBlob.size() };

        DX::ThrowIfFailed(
            device->CreateGraphicsPipelineState(&psoDesc,
                IID_GRAPHICS_PPV_ARGS(m_pipelineStateAutoPdbNoPath.ReleaseAndGetAddressOf())));
    }

    // The pixel shader compiled with D3DCompile and D3DCOMPILE_DEBUG, but with the symbols stripped
    // entirely away. This version will not display source in PIX, and therefore cannot be edited
    // or debugged at HLSL level.
    {
        auto pixelShaderBlob = DX::ReadData(L"PixelShader_D3DCompile_StrippedPdb.cso");
        psoDesc.PS = { pixelShaderBlob.data(), pixelShaderBlob.size() };

        DX::ThrowIfFailed(
            device->CreateGraphicsPipelineState(&psoDesc,
                IID_GRAPHICS_PPV_ARGS(m_pipelineStateStrippedPdb.ReleaseAndGetAddressOf())));
    }

    {
        m_primitiveBatch = std::make_unique<DirectX::PrimitiveBatch<VertexPositionColor>>(device);

        m_resourceDescriptorHeap = std::make_unique<DescriptorHeap>(device, ResourceDescriptors::Count);

        ResourceUploadBatch resourceUpload(device);
        resourceUpload.Begin();

        RenderTargetState renderTargetState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());
        SpriteBatchPipelineStateDescription pipelineDescription(renderTargetState);
        m_spriteBatch = std::make_unique<SpriteBatch>(device, resourceUpload, pipelineDescription);

        m_fontDescription = std::make_unique<SpriteFont>(device,
            resourceUpload,
            L"SegoeUI_18.spritefont",
            m_resourceDescriptorHeap->GetCpuHandle(ResourceDescriptors::FontDescription),
            m_resourceDescriptorHeap->GetGpuHandle(ResourceDescriptors::FontDescription));

        auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());
        uploadResourcesFinished.wait();     // Wait for resources to upload
    }

    // Wait until assets have been uploaded to the GPU.
    m_deviceResources->WaitForGpu();
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
}
#pragma endregion
