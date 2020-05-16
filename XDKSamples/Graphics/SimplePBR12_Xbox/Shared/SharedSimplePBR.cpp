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

//#define TEST_SCENE

#if defined(_XBOX_ONE) && defined(_TITLE)
#include "../Xbox/SimplePBRXbox12.h" // get sample definition
#else
#include "../UWP/SimplePBRUWP12.h" // get sample definition
#endif

extern void ExitSample() noexcept;

using namespace DirectX;
using namespace DirectX::SimpleMath;
using Microsoft::WRL::ComPtr;

namespace
{
    // PBR Assest paths.
    const wchar_t* s_modelPaths[] =
    {
        L"Floor.sdkmesh",
        L"ToyRobot.sdkmesh",
        L"WoodBlocks.sdkmesh"
    };

    // A simple test scene for material parameters
    struct TestScene
    {
        std::unique_ptr<DirectX::Model> m_model;
        std::unique_ptr<DirectX::GeometricPrimitive> m_sphere;
        std::unique_ptr<ATG::PBREffect> m_effect;

        void Init(ID3D12Device* device,
                  ResourceUploadBatch& upload,
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
            m_model = Model::CreateFromSDKMESH(device, L"XboxOrb.sdkmesh");

            // Optimize model for rendering
            m_model->LoadStaticBuffers(device, upload);
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
    m_gamepadConnected(false)
{
    m_gamePad = std::make_unique<GamePad>();

    m_hdrScene = std::make_unique<DX::RenderTexture>(Sample::GetHDRRenderFormat());
}

void SharedSimplePBR::Update(DX::StepTimer const& timer)
{
    const float elapsedSeconds = static_cast<float>(timer.GetElapsedSeconds());

    // Update camera via game pad
    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_gamepadConnected = true;

        m_gamePadButtons.Update(pad);

        if (pad.IsViewPressed())
        {
            ExitSample();
        }
    }
    else
    {
        m_gamepadConnected = false;

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
    m_hdrScene->BeginScene(commandList);

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

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render HUD");
    {
        m_hudBatch->Begin(commandList);

        m_smallFont->DrawString(m_hudBatch.get(), L"SimplePBR Sample",
            XMFLOAT2(float(safe.left), float(safe.top)), ATG::ColorsHDR::LightGrey);

        const wchar_t* legendStr = (m_gamepadConnected) ?
            L"[RThumb] [LThumb]: Move Camera   [View] Exit "
            : L"Mouse, W,A,S,D: Move Camera   Esc: Exit ";

        DX::DrawControllerString(m_hudBatch.get(),
            m_smallFont.get(), m_ctrlFont.get(),
            legendStr,
            XMFLOAT2(float(safe.left),
                float(safe.bottom) - m_smallFont->GetLineSpacing()),
            ATG::ColorsHDR::LightGrey);

        m_hudBatch->End();
    }
    PIXEndEvent(commandList); // HUD

    m_hdrScene->EndScene(commandList);

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Tonemap");

#if defined(_XBOX_ONE) && defined(_TITLE)

    // Generate both HDR10 and tonemapped SDR signal
    D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptors[2] = { deviceResources->GetRenderTargetView(), deviceResources->GetGameDVRRenderTargetView() };
    commandList->OMSetRenderTargets(2, rtvDescriptors, FALSE, nullptr);

    m_HDR10->SetHDRSourceTexture(m_srvPile->GetGpuHandle(StaticDescriptors::SceneTex));
    m_HDR10->Process(commandList);

#else

    {
        auto rtv = static_cast<D3D12_CPU_DESCRIPTOR_HANDLE>(deviceResources->GetRenderTargetView());
        commandList->OMSetRenderTargets(1, &rtv, FALSE, NULL);

        if (deviceResources->GetColorSpace() == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
        {
            // HDR10 signal
            m_HDR10->SetHDRSourceTexture(m_srvPile->GetGpuHandle(StaticDescriptors::SceneTex));
            m_HDR10->Process(commandList);
        }
        else
        {
            // Tonemap for SDR signal
            m_toneMap->SetHDRSourceTexture(m_srvPile->GetGpuHandle(StaticDescriptors::SceneTex));
            m_toneMap->Process(commandList);
        }
    }

#endif

    PIXEndEvent(commandList); // Tonemap

    PIXEndEvent(commandList); // Render
}

void SharedSimplePBR::CreateDeviceDependentResources()
{
    auto device = m_sample->m_deviceResources->GetD3DDevice();

    // State objects
    m_commonStates = std::make_unique<DirectX::CommonStates>(device);

    // create heaps
    m_srvPile = std::make_unique<DescriptorPile>(device,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        128, // Maximum descriptors for both static and dynamic
        StaticDescriptors::Reserve);
    m_rtvHeap = std::make_unique<DescriptorHeap>(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 1);

    // Set up HDR render target.
    m_hdrScene->SetDevice(device, m_srvPile->GetCpuHandle(StaticDescriptors::SceneTex), m_rtvHeap->GetFirstCpuHandle());
    
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

    DirectX::CreateShaderResourceView(device, m_radianceTexture.Get(), m_srvPile->GetCpuHandle(StaticDescriptors::RadianceTex), true);

    // Irradiance (diffuse environment) texture
    DX::ThrowIfFailed(
        DirectX::CreateDDSTextureFromFile(
            device,
            resourceUpload,
            L"Stonewall_Ref_irradiance.dds",
            m_irradianceTexture.ReleaseAndGetAddressOf(),
            false
            ));

    DirectX::CreateShaderResourceView(device, m_irradianceTexture.Get(), m_srvPile->GetCpuHandle(StaticDescriptors::IrradianceTex), true);

    // Pipeline state - for rendering direct to back buffer
    {
        RenderTargetState backBufferRts(Sample::GetBackBufferFormat(), Sample::GetDepthFormat());

        // Create HDR10 color space effect
#if defined(_XBOX_ONE) && defined(_TITLE)
        backBufferRts.numRenderTargets = 2;
        backBufferRts.rtvFormats[1] = m_sample->m_deviceResources->GetGameDVRFormat();

        m_HDR10 = std::make_unique<ToneMapPostProcess>(device, backBufferRts,
            ToneMapPostProcess::ACESFilmic, ToneMapPostProcess::SRGB, true);
#else
        m_HDR10 = std::make_unique<ToneMapPostProcess>(device, backBufferRts,
            ToneMapPostProcess::None, ToneMapPostProcess::ST2084);

        // Create tone mapping effect
        m_toneMap = std::make_unique<ToneMapPostProcess>(device, backBufferRts,
            ToneMapPostProcess::ACESFilmic, ToneMapPostProcess::SRGB);
#endif

    }

    // Pipeline state - for rendering to HDR buffer
    {
        RenderTargetState hdrBufferRts(Sample::GetHDRRenderFormat(), Sample::GetDepthFormat());

        // HUD
        DirectX::SpriteBatchPipelineStateDescription hudpd(
            hdrBufferRts,
            &CommonStates::AlphaBlend);

        m_hudBatch = std::make_unique<SpriteBatch>(device, resourceUpload, hudpd);

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
        m_skybox = std::make_unique<DX::Skybox>(device, m_srvPile->GetGpuHandle(StaticDescriptors::RadianceTex), hdrBufferRts, *m_commonStates);
    }
 
    // The current map has too much detail removed at last mips, scale back down to
    // match reference.
    const int numMips = m_radianceTexture->GetDesc().MipLevels - 3;

    // Set lighting textures for each model
    for (auto& m : m_pbrModels)
    {
        m->GetEffect()->SetIBLTextures(
            m_srvPile->GetGpuHandle(StaticDescriptors::RadianceTex),
            numMips,
            m_srvPile->GetGpuHandle(StaticDescriptors::IrradianceTex),
            m_commonStates->LinearWrap());
    }

    s_testScene = std::make_unique<TestScene>();
    s_testScene->Init(device,
        resourceUpload,
        m_srvPile->GetGpuHandle(StaticDescriptors::RadianceTex),
        numMips,
        m_srvPile->GetGpuHandle(StaticDescriptors::IrradianceTex),
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
    m_hdrScene->SetWindow(size);

    // Begin uploading texture resources
    ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin();

    m_smallFont = std::make_unique<SpriteFont>(device, resourceUpload,
        (size.bottom > 1200) ? L"SegoeUI_36.spritefont" : L"SegoeUI_18.spritefont",
        m_srvPile->GetCpuHandle(StaticDescriptors::Font),
        m_srvPile->GetGpuHandle(StaticDescriptors::Font));

    m_ctrlFont = std::make_unique<SpriteFont>(device, resourceUpload,
        (size.bottom > 1200) ? L"XboxOneControllerLegend.spritefont" : L"XboxOneControllerLegendSmall.spritefont",
        m_srvPile->GetCpuHandle(StaticDescriptors::CtrlFont),
        m_srvPile->GetGpuHandle(StaticDescriptors::CtrlFont));

    auto finished = resourceUpload.End(m_sample->m_deviceResources->GetCommandQueue());
    finished.wait();
}

// For UWP only
void SharedSimplePBR::OnDeviceLost()
{
    m_hudBatch.reset();
    m_smallFont.reset();
    m_ctrlFont.reset();
    
    m_camera.reset();
    m_commonStates.reset();

    m_srvPile.reset();

    m_spriteBatch.reset();
    m_toneMap.reset();
    m_HDR10.reset();

    m_hdrScene->ReleaseDevice();
    m_rtvHeap.reset();

    for (auto& m : m_pbrModels)
    {
        m.reset();
    }
}