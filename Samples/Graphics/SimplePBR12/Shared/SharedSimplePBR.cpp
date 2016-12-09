//--------------------------------------------------------------------------------------
// SharedSimplePBR.h
//
// Shared sample class to demonstrate PBRModel and PBREffect in DirectX 12 on Xbox ERA
// and PC UWP.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SharedSimplePBR.h"

#include "ControllerFont.h"
#include "DDSTextureLoader.h"
#include "GeometricPrimitive.h"

#if defined(_XBOX_ONE) && defined(_TITLE)
#include "../Xbox/SimplePBRXbox12.h" // get sample definition
#else
#include "../UWP/SimplePBRUWP12.h" // get sample definition
#endif


using namespace DirectX;
using namespace DirectX::SimpleMath;
using Microsoft::WRL::ComPtr;

namespace
{
    // PBR Assest paths.
    const wchar_t* s_modelPaths[] =
    {
        L"Assets\\Models\\ToyRobot\\Floor\\Floor.sdkmesh",
        L"Assets\\Models\\ToyRobot\\ToyRobot\\ToyRobot.sdkmesh",
        L"Assets\\Models\\ToyRobot\\WoodBlocks\\WoodBlocks.sdkmesh"
    };

    // A simple test scene for material parameters
    struct TestScene
    {
        std::unique_ptr<DirectX::Model> m_model;
        std::unique_ptr<DirectX::GeometricPrimitive> m_sphere;
        std::unique_ptr<ATG::PBREffect> m_effect;

        void Init(ID3D12Device* device,
                    D3D12_GPU_DESCRIPTOR_HANDLE radianceTex, int numMips, 
                    D3D12_GPU_DESCRIPTOR_HANDLE irradianceTex, 
                    D3D12_GPU_DESCRIPTOR_HANDLE sampler)
        {
            RenderTargetState hdrBufferRts(Sample::GetHDRRenderFormat(), Sample::GetDepthFormat());

            m_sphere = DirectX::GeometricPrimitive::CreateSphere(1.5);

            // Create PBR Effect
            EffectPipelineStateDescription pbrEffectPipelineState(
                &ATG::VertexPositionNormalTextureTangent::InputLayout,
                CommonStates::Opaque,
                CommonStates::DepthDefault,
                CommonStates::CullClockwise,
                hdrBufferRts,
                D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
            m_effect = std::make_unique<ATG::PBREffect>(device, EffectFlags::None, pbrEffectPipelineState);

            // Lighting
            m_effect->SetIBLTextures(
                    radianceTex,
                    numMips,
                    irradianceTex,
                    sampler);
            
            // Model
            m_model = Model::CreateFromSDKMESH(L"Assets\\Models\\XboxOrb\\XboxOrb.sdkmesh");
        }

        void XM_CALLCONV Render(ID3D12GraphicsCommandList* commandList, FXMMATRIX camView, CXMMATRIX camProj)
        {
            const size_t numSpheres = 3;
            const float step = 15.f;

            Vector3 modelPos((-step * (numSpheres- 1)) / 2.f, 0, 0);
            
            m_effect->SetConstantAlbedo(Vector3(1, 1, 1));
            m_effect->SetConstantMetallic(1);
           
            for (size_t i = 0; i < numSpheres; i++)
            {
                m_effect->SetView(camView);
                m_effect->SetProjection(camProj);
                m_effect->SetWorld(Matrix::CreateTranslation(modelPos));

                modelPos += Vector3(step, 0, 0);

                m_effect->SetConstantRoughness(float(i) / float(numSpheres-1));

                m_effect->Apply(commandList);
                m_model->DrawOpaque(commandList);
            }

            modelPos = Vector3((-step * (numSpheres - 1)) / 2.f, 0, 0);
            modelPos += Vector3(0, step, 0);
            
            m_effect->SetConstantMetallic(0);

            for (size_t i = 0; i < numSpheres; i++)
            {
                m_effect->SetView(camView);
                m_effect->SetProjection(camProj);
                m_effect->SetWorld(Matrix::CreateTranslation(modelPos));

                modelPos += Vector3(step, 0, 0);

                m_effect->SetConstantRoughness(float(i) / float(numSpheres-1));

                m_effect->Apply(commandList);
                m_model->DrawOpaque(commandList);
            }
        }
    };

    std::unique_ptr<TestScene> s_testScene;
}

SharedSimplePBR::SharedSimplePBR(Sample* sample) :
    m_sample(sample),
    m_radTexDescIndex(ATG::DescriptorPile::INVALID_INDEX),
    m_HDRBufferDescIndex(ATG::DescriptorPile::INVALID_INDEX)
{
    m_gamePad = std::make_unique<GamePad>();
}

void SharedSimplePBR::Update(DX::StepTimer const& timer)
{
    const float elapsedSeconds = static_cast<float>(timer.GetElapsedSeconds());

    // Update camera via game pad
    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);

        if (pad.IsViewPressed())
        {
            Windows::ApplicationModel::Core::CoreApplication::Exit();
        }
    }
    else
    {
        m_gamePadButtons.Reset();
    }
    m_camera->Update(elapsedSeconds, pad);

#if !defined(_XBOX_ONE) || !defined(_TITLE)
    // KB/Mouse currently only PC
    m_camera->Update(elapsedSeconds, *(m_sample->m_mouse.get()), *(m_sample->m_keyboard.get()));
#endif

    // Update model effects
    for (auto& m : m_pbrModels)
    {
        auto effect = m->GetEffect();
        effect->SetView(m_camera->GetView());
        effect->SetProjection(m_camera->GetProjection());
        effect->SetWorld(Matrix::CreateRotationY(XM_PI));
    }

    // Update skybox
    m_skybox->Update(m_camera->GetView(), m_camera->GetProjection());
}

void SharedSimplePBR::Render()
{
    // Resources and dimensions for this render
    DX::DeviceResources* deviceResources = m_sample->m_deviceResources.get();    
    auto commandList = deviceResources->GetCommandList();
    auto size = deviceResources->GetOutputSize();
    auto safe = SimpleMath::Viewport::ComputeTitleSafeArea(size.right, size.bottom);

    // Set descriptor heaps
    ID3D12DescriptorHeap* heaps[] = { m_srvPile->Heap(), m_commonStates->Heap() };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    // Draw to HDR buffer
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render HDR");

    auto depthStencilDescriptor = deviceResources->GetDepthStencilView();
    auto toneMapRTVDescriptor = m_rtvHeap->GetFirstCpuHandle();
    commandList->OMSetRenderTargets(1, &toneMapRTVDescriptor, FALSE, &depthStencilDescriptor);

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Model Draw");
#ifndef TEST_SCENE
    for (auto& m : m_pbrModels)
    {
        m->GetEffect()->Apply(commandList);
        m->GetModel()->DrawOpaque(commandList);
    }
#else
   s_testScene->Render(commandList, m_camera->GetView(), m_camera->GetProjection());
#endif
    PIXEndEvent(commandList); // Model Draw

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Sky box");
    {  
        // Render test skybox
        m_skybox->Render(commandList);
    }

    PIXEndEvent(commandList);

    PIXEndEvent(commandList); // Render HDR

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Tonemap HDR to SDR backbuffer");
    {
        auto rtv = static_cast<D3D12_CPU_DESCRIPTOR_HANDLE>(deviceResources->GetRenderTargetView());
        commandList->OMSetRenderTargets(1, &rtv, FALSE, NULL);
        
        D3D12_RESOURCE_BARRIER b1 = CD3DX12_RESOURCE_BARRIER::Transition(m_rtvHDRBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commandList->ResourceBarrier(1, &b1);

        // Set up tonemap effect
        m_toneMapEffect->SetTexture(m_srvPile->GetGpuHandle(m_HDRBufferDescIndex), m_commonStates->LinearClamp());
        m_toneMapEffect->Apply(commandList);

        // Draw quad
        m_toneMapBatch->Begin(commandList);
        m_toneMapBatch->DrawQuad(ToneMapVert(Vector3(-1, 1, 0), Vector2(0, 0)),
            ToneMapVert(Vector3(1, 1, 0), Vector2(1, 0)),
            ToneMapVert(Vector3(1, -1, 0), Vector2(1, 1)),
            ToneMapVert(Vector3(-1, -1, 0), Vector2(0, 1)));
        m_toneMapBatch->End();

        D3D12_RESOURCE_BARRIER b2 = CD3DX12_RESOURCE_BARRIER::Transition(m_rtvHDRBuffer.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        commandList->ResourceBarrier(1, &b2);
    }
    PIXEndEvent(commandList); // Tonemap
    
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render HUD");
    {
        m_hudBatch->Begin(commandList);

        m_smallFont->DrawString(m_hudBatch.get(), L"SimplePBR Sample",
            XMFLOAT2(float(safe.left), float(safe.top)), ATG::Colors::LightGrey);

        DX::DrawControllerString(m_hudBatch.get(),
            m_smallFont.get(), m_ctrlFont.get(),
            L"[RThumb] [LThumb] Mouse, W,A,S,D : Move Camera [View] Exit ",
            XMFLOAT2(float(safe.left),
                float(safe.bottom) - m_smallFont->GetLineSpacing()),
            ATG::Colors::LightGrey);

        m_hudBatch->End();
    }
    PIXEndEvent(commandList); // HUD

    PIXEndEvent(commandList); // Render
}

void SharedSimplePBR::CreateDeviceDependentResources()
{
    auto device = m_sample->m_deviceResources->GetD3DDevice();

    // State objects
    m_commonStates = std::make_unique<DirectX::CommonStates>(device);

    // create heaps
    m_srvPile = std::make_unique<ATG::DescriptorPile>(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 128);
    m_rtvHeap = std::make_unique<DescriptorHeap>(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 1);
    
    // Begin uploading texture resources
    ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin();

    // Radiance (specular environment) texture
    DX::ThrowIfFailed(
        DirectX::CreateDDSTextureFromFile(
            device,
            resourceUpload,
            L"Stonewall_Ref_radiance.dds",
            m_radianceTexture.ReleaseAndGetAddressOf(),
            false
            ));

    m_radTexDescIndex = m_srvPile->Allocate();
    DirectX::CreateShaderResourceView(device, m_radianceTexture.Get(), m_srvPile->GetCpuHandle(m_radTexDescIndex), true);

    // Irradiance (diffuse environment) texture
    DX::ThrowIfFailed(
        DirectX::CreateDDSTextureFromFile(
            device,
            resourceUpload,
            L"Stonewall_Ref_irradiance.dds",
            m_irradianceTexture.ReleaseAndGetAddressOf(),
            false
            ));

    m_irrTexDescIndex = m_srvPile->Allocate();
    DirectX::CreateShaderResourceView(device, m_irradianceTexture.Get(), m_srvPile->GetCpuHandle(m_irrTexDescIndex), true);

    // Pipeline state - for rendering direct to back buffer
    {
        RenderTargetState backBufferRts(Sample::GetBackBufferFormat(), Sample::GetDepthFormat());

        // HUD
        DirectX::SpriteBatchPipelineStateDescription hudpd(
            backBufferRts,
            &CommonStates::AlphaBlend);

        m_hudBatch = std::make_unique<SpriteBatch>(device, resourceUpload, hudpd);

        auto segoeDescIndex = m_srvPile->Allocate();
        m_smallFont = std::make_unique<SpriteFont>(device, resourceUpload,
            L"SegoeUI_18.spritefont",
            m_srvPile->GetCpuHandle(segoeDescIndex),
            m_srvPile->GetGpuHandle(segoeDescIndex));

        auto legendDescInex = m_srvPile->Allocate();
        m_ctrlFont = std::make_unique<SpriteFont>(device, resourceUpload,
            L"XboxOneControllerLegendSmall.spritefont",
            m_srvPile->GetCpuHandle(legendDescInex),
            m_srvPile->GetGpuHandle(legendDescInex));

        // Create tone mapping effect
        m_toneMapEffect = std::make_unique<ATG::ToneMapEffect>(device, Sample::GetBackBufferFormat());

        // Tone map batch
        m_toneMapBatch = std::make_unique<DirectX::PrimitiveBatch<ToneMapVert>>(device);
   }

    // Pipeline state - for rendering to HDR buffer
    {
        RenderTargetState hdrBufferRts(Sample::GetHDRRenderFormat(), Sample::GetDepthFormat());

        // Sky rendering batch
        m_spriteBatch = std::make_unique<DirectX::SpriteBatch>(device, resourceUpload, SpriteBatchPipelineStateDescription(hdrBufferRts, &CommonStates::Opaque));

        // PBR Model
        const auto numModels = _countof(s_modelPaths);
        m_pbrModels.resize(numModels);
        
        for (auto i = 0; i < numModels; i++)
        {
            m_pbrModels[i] = std::make_unique<ATG::PBRModel>(s_modelPaths[i]);
            m_pbrModels[i]->Create(device, hdrBufferRts, m_commonStates.get(), resourceUpload, m_srvPile.get());
        }

        // Skybox
        m_skybox = std::make_unique<ATG::Skybox>(device, m_srvPile->GetGpuHandle(m_radTexDescIndex), hdrBufferRts, *m_commonStates);
    }
 
    // The current map has too much detail removed at last mips, scale back down to
    // match reference.
    const int numMips = m_radianceTexture->GetDesc().MipLevels - 3;

    // Set lighting textures for each model
    for (auto& m : m_pbrModels)
    {
        m->GetEffect()->SetIBLTextures(
            m_srvPile->GetGpuHandle(m_radTexDescIndex),
            numMips,
            m_srvPile->GetGpuHandle(m_irrTexDescIndex),
            m_commonStates->LinearWrap());
    }

    s_testScene = std::make_unique<TestScene>();
    s_testScene->Init(device, m_srvPile->GetGpuHandle(m_radTexDescIndex),
        numMips,
        m_srvPile->GetGpuHandle(m_irrTexDescIndex),
        m_commonStates->LinearWrap());

    auto finished = resourceUpload.End(m_sample->m_deviceResources->GetCommandQueue());
    finished.wait();
}

void SharedSimplePBR::CreateWindowSizeDependentResources()
{
    auto device = m_sample->m_deviceResources->GetD3DDevice();
    const auto size = m_sample->m_deviceResources->GetOutputSize();

    // Set hud sprite viewport
    m_hudBatch->SetViewport(m_sample->m_deviceResources->GetScreenViewport());
     
    // set camera
    {
        const float fovAngleY = 70.0f * XM_PI / 180.0f;

        m_camera = std::make_unique<DX::OrbitCamera>();
        m_camera->SetWindow(size.right, size.bottom);
        m_camera->SetProjectionParameters(fovAngleY, 0.1f, 1000.f, false);
        m_camera->SetRadius(25.f);
        m_camera->SetRadiusRate(5.f);
        m_camera->SetFocus(Vector3(0, 4, -5));
        // Rotate to face front
        m_camera->SetRotation(Vector3(0, XM_PI, XM_PI / 10));
    }

    // HDR render target resource
    {
        D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(
            Sample::GetHDRRenderFormat(),
            size.right,
            size.bottom,
            1,
            1  // Use a single mipmap level.
        );
        desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        // create resource
        CD3DX12_HEAP_PROPERTIES rtvHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
        DX::ThrowIfFailed(device->CreateCommittedResource(
            &rtvHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            nullptr, // &depthOptimizedClearValue,
            IID_GRAPHICS_PPV_ARGS(m_rtvHDRBuffer.ReleaseAndGetAddressOf())
        ));
        m_rtvHDRBuffer->SetName(L"HDR buffer");
    }

    // HDR render target view
    {  
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = Sample::GetHDRRenderFormat();
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        device->CreateRenderTargetView(m_rtvHDRBuffer.Get(), &rtvDesc, m_rtvHeap->GetFirstCpuHandle());

        // create SR view and put in heap
        if (m_HDRBufferDescIndex == ATG::DescriptorPile::INVALID_INDEX) m_HDRBufferDescIndex = m_srvPile->Allocate();

        DirectX::CreateShaderResourceView(device, m_rtvHDRBuffer.Get(), m_srvPile->GetCpuHandle(m_HDRBufferDescIndex));
    }
}

// For UWP only
void SharedSimplePBR::OnDeviceLost()
{
    m_hudBatch.reset();
    m_smallFont.reset();
    m_ctrlFont.reset();
    
    m_gamePad.reset();
    m_camera.reset();
    m_commonStates.reset();

    m_srvPile.reset();

    m_spriteBatch.reset();
    m_toneMapEffect.reset();
    m_toneMapBatch.reset();

    m_rtvHeap.reset();

    for (auto& m : m_pbrModels)
    {
        m.reset();
    }
}