//--------------------------------------------------------------------------------------
// AdvancedESRAM12.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Bokeh12.h"

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

	static const int PRESET_SCENE_COUNT = 4;
    
    enum DescriptorHeapIndex
    {
        SRV_Font = 0,
        SRV_CtrlFont,
        SRV_SceneColor,
        SRV_SceneDepth,
        SRV_Count
    };

    enum RTVDescriptorHeapIndex
    {
        RTV_SceneColor = 0,
        RTV_Count
    };

    enum DSVDescriptorHeapIndex
    {
        DSV_SceneDepth = 0,
        DSV_Count
    };

    enum TimerIndex
    {
        TI_Frame,
        TI_Scene,
        TI_Bokeh,
        TI_Copy
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

    // Sample Constants
    const DXGI_FORMAT c_colorFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    const DXGI_FORMAT c_depthFormat = DXGI_FORMAT_D32_FLOAT;

    //-----------------------------------
    // Helper Functions

    void AddMod(int& value, int add, int mod)
    {
        int res = (value + add) % mod;
        value = res < 0 ? mod - 1 : res;
    }

    void IncrMod(int& value, int mod) { AddMod(value, 1, mod); }
    void DecrMod(int& value, int mod) { AddMod(value, -1, mod); }
}
#pragma endregion

#pragma region Construction
Sample::Sample() noexcept(false)
    : m_displayWidth(0)
    , m_displayHeight(0)
    , m_frame(0)
    , m_presetScene(0)
    , m_cameraAngle(0.0f)
    , m_cameraElevation(5.0f)
    , m_cameraDistance(5.0f)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, 2, DX::DeviceResources::c_Enable4K_UHD);

    // performance / quality tradeoff
    m_bokehParams.maxCoCSizeNear = 32;       // maximum allowed radius
    m_bokehParams.maxCoCSizeFar = 32;
    m_bokehParams.switchover1[0] = 16;       // near/far 1/2 -> 1/4 switchover threshold in pixels (radius)
    m_bokehParams.switchover1[1] = 16;       // make larger to have higher quality/lower speed
    m_bokehParams.switchover2[0] = 16;       // near 1/4 -> 1/8
    m_bokehParams.switchover2[1] = 16;       //

    // edges blend
    m_bokehParams.initialEnergyScale = 1.72f;
    m_bokehParams.useFastShader = true;

    SetPredefinedScene(0);
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

void Sample::Update(const DX::StepTimer&)
{
    using ButtonState = DirectX::GamePad::ButtonStateTracker::ButtonState;

    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    auto pad = m_gamePad.GetState(0);
    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);

        if (pad.IsViewPressed())
        {
            ExitSample();
        }

        m_cameraAngle -= pad.thumbSticks.leftX * 0.2f;
        m_cameraElevation = std::max(-10.f, std::min(10.f, m_cameraElevation - pad.thumbSticks.leftY * 0.2f));
        m_cameraDistance = std::max(1.f, std::min(10.f, m_cameraDistance - pad.thumbSticks.rightY * 0.2f));

        // Focus Length
        if (pad.IsDPadLeftPressed())
        {
            // longest super telephoto Canon F/11 lens is 1.2 meter
            // so let's limit it to 200mm because even that can produce blurs of radius of 100+ in the near plane
            m_bokehParams.focusLength = std::min(0.2f, m_bokehParams.focusLength + 0.01f);
        }
        if (pad.IsDPadRightPressed())
        {
            m_bokehParams.focusLength = std::max(0.025f, m_bokehParams.focusLength - 0.01f);
        }

        // FNumber
        if (pad.IsAPressed())
        {
            m_bokehParams.FNumber = std::min(64.0f, m_bokehParams.FNumber + 0.1f);
        }
        if (pad.IsBPressed())
        {
            m_bokehParams.FNumber = std::max(1.0f, m_bokehParams.FNumber - 0.1f);
        }

        // Focal Plane
        if (pad.IsYPressed())
        {
            m_bokehParams.focalPlane = std::min(10.0f, m_bokehParams.focalPlane + 0.01f);
        }
        if (pad.IsXPressed())
        {
            m_bokehParams.focalPlane = std::max(0.5f, m_bokehParams.focalPlane - 0.01f);
        }

        // Max Near CoC Size
        if (pad.IsLeftShoulderPressed())
        {
            m_bokehParams.maxCoCSizeNear = std::max(1.0f, m_bokehParams.maxCoCSizeNear - 1.f);
        }
        if (pad.IsLeftTriggerPressed())
        {
            m_bokehParams.maxCoCSizeNear = std::min(128.0f, m_bokehParams.maxCoCSizeNear + 1.f);
        }

        // Max Far CoC Size
        if (pad.IsRightShoulderPressed())
        {
            m_bokehParams.maxCoCSizeFar = std::max(1.0f, m_bokehParams.maxCoCSizeFar - 1.f);
        }
        if (pad.IsRightTriggerPressed())
        {
            m_bokehParams.maxCoCSizeFar = std::min(128.0f, m_bokehParams.maxCoCSizeFar + 1.f);
        }
        
        // Toggle fast bokeh shader
        if (m_gamePadButtons.dpadUp == ButtonState::PRESSED)
        {
            m_bokehParams.useFastShader = !m_bokehParams.useFastShader;
        }

        // Iterate through preset parameters & cam position
        if (m_gamePadButtons.dpadDown == ButtonState::PRESSED)
        {
            IncrMod(m_presetScene, PRESET_SCENE_COUNT);
            SetPredefinedScene(m_presetScene);
        }
    }
    else
    {
        m_gamePadButtons.Reset();
    }

    // Update the scene.
    CalculateCameraMatrix();

    for (int i = 0; i < m_scene.size(); ++i)
    {
        Model::UpdateEffectMatrices(m_scene[i].effects, m_scene[i].world, m_matView, m_matProj);
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

    m_profiler->BeginFrame(commandList);
    m_profiler->Start(commandList, TI_Frame);

    D3D12_CPU_DESCRIPTOR_HANDLE hRTVScene = m_rtvPile->GetCpuHandle(RTV_SceneColor);
    D3D12_CPU_DESCRIPTOR_HANDLE hDSV = m_dsvPile->GetCpuHandle(DSV_SceneDepth);

    TransitionResource(commandList, m_sceneColor.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
    TransitionResource(commandList, m_sceneDepth.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);

    commandList->ClearRenderTargetView(hRTVScene, DirectX::Colors::Transparent, 0, nullptr);
    commandList->ClearDepthStencilView(hDSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0x0, 0, nullptr);
    commandList->OMSetRenderTargets(1, &hRTVScene, false, &hDSV);

    D3D12_VIEWPORT viewport = m_deviceResources->GetScreenViewport();
    commandList->RSSetViewports(1, &viewport);

    D3D12_RECT scissor = m_deviceResources->GetScissorRect();
    commandList->RSSetScissorRects(1, &scissor);


    //------------------------------
    // Scene

    {
        ScopedPixEvent Scene(commandList, PIX_COLOR_DEFAULT, L"Scene");

        m_profiler->Start(commandList, TI_Scene);

        // render the city and microscopes
        for (int i = 0; i < m_scene.size(); ++i)
        {
            m_scene[i].model->DrawOpaque(commandList, m_scene[i].effects.begin());
        }

        m_profiler->Stop(commandList, TI_Scene);
    }


    //------------------------------
    // Bokeh

    {
        ScopedPixEvent Bokeh(commandList, PIX_COLOR_DEFAULT, L"Bokeh");
        m_profiler->Start(commandList, TI_Bokeh);

        TransitionResource(commandList, m_sceneDepth.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        m_bokehDOF->Render(commandList, m_sceneColor.Get(), m_cpuPile->GetCpuHandle(SRV_SceneColor), m_cpuPile->GetCpuHandle(SRV_SceneDepth), hRTVScene, m_matInvProj, m_bokehParams, false);
        TransitionResource(commandList, m_sceneDepth.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);

        m_profiler->Stop(commandList, TI_Bokeh);
    }


    //------------------------------
    // Copy

    {
        ScopedPixEvent Copy(commandList, PIX_COLOR_DEFAULT, L"Copy");

        m_profiler->Start(commandList, TI_Copy);
        TransitionResource(commandList, m_sceneColor.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        D3D12_CPU_DESCRIPTOR_HANDLE hRTV = m_deviceResources->GetRenderTargetView();
        commandList->OMSetRenderTargets(1, &hRTV, true, nullptr);

        D3D12_GPU_DESCRIPTOR_HANDLE hSRV = m_srvPile->GetGpuHandle(SRV_SceneColor);
        m_copyShader->SetSourceTexture(hSRV, m_sceneColor.Get());
        m_copyShader->Process(commandList);

        TransitionResource(commandList, m_sceneColor.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
        m_profiler->Stop(commandList, TI_Copy);
    }


    //------------------------------
    // HUD

    {
        ScopedPixEvent HUD(commandList, PIX_COLOR_DEFAULT, L"HUD");

        // performance printout
        const float frameTime = m_profiler->GetAverageMS(TI_Frame);
        const float sceneTime = m_profiler->GetAverageMS(TI_Scene);
        const float bokehTime = m_profiler->GetAverageMS(TI_Bokeh);
        const float copyTime = m_profiler->GetAverageMS(TI_Copy);

        auto safe = SimpleMath::Viewport::ComputeTitleSafeArea(m_displayWidth, m_displayHeight);

        wchar_t textBuffer[256] = {};
        XMFLOAT2 textPos = XMFLOAT2(float(safe.left), float(safe.top));
        XMVECTOR textColor = ATG::Colors::Green;

        m_hudBatch->Begin(commandList);
        m_smallFont->DrawString(m_hudBatch.get(), L"Bokeh Sample", textPos, textColor);
        textPos.y += m_smallFont->GetLineSpacing();

        _snwprintf_s(textBuffer, _TRUNCATE,
            L"Frame CPU: %.2f ms \nFrame GPU: %.2f ms \nScene: %.2f ms \nBokeh: %.2f ms \nFinal copy: %.2f ms",
            1000 * m_timer.GetElapsedSeconds(),
            frameTime,
            sceneTime,
            bokehTime,
            copyTime);
        m_smallFont->DrawString(m_hudBatch.get(), textBuffer, textPos, textColor);


        _snwprintf_s(textBuffer, _TRUNCATE,
            L"[DPad] Up/Down   Fast Bokeh: %s \n\
                [DPad] Left/Right   Lens: %.2fmm \n\
                [A][B] F/%.1f \n\
                [X][Y] Focal Plane: %.2fm \n\
                [LB][LT] CoC Near: %.1f \n\
                [RB][RT] CoC Far: %.1f \n\
                [View] Exit",
            m_bokehParams.useFastShader ? L"true" : L"false",
            m_bokehParams.focusLength * 1000.f,
            m_bokehParams.FNumber,
            m_bokehParams.focalPlane,
            m_bokehParams.maxCoCSizeNear,
            m_bokehParams.maxCoCSizeFar);

        textPos.y = float(safe.bottom - m_smallFont->GetLineSpacing() * 7);
        DX::DrawControllerString(m_hudBatch.get(), m_smallFont.get(), m_ctrlFont.get(), textBuffer, textPos, textColor);
        m_hudBatch->End();

        m_profiler->Stop(commandList, TI_Frame);
    }

    m_profiler->EndFrame(commandList);

    // Show the new frame.
    {
        m_deviceResources->Present();
        m_graphicsMemory->Commit(m_deviceResources->GetCommandQueue());
    }
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

    // State objects
    m_commonStates = std::make_unique<DirectX::CommonStates>(device);

    // Create heap
    m_cpuPile = std::make_unique<DescriptorPile>(device,
        128,
        DescriptorHeapIndex::SRV_Count);

    m_srvPile = std::make_unique<DescriptorPile>(device,
        128,
        DescriptorHeapIndex::SRV_Count);

    m_rtvPile = std::make_unique<DescriptorPile>(device,
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        128,
        RTVDescriptorHeapIndex::RTV_Count);

    m_dsvPile = std::make_unique<DescriptorPile>(device,
        D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        128,
        DSVDescriptorHeapIndex::DSV_Count);

    // Load models from disk.
    m_models.resize(_countof(s_modelPaths));
    for (int i = 0; i < m_models.size(); ++i)
    {
        m_models[i] = Model::CreateFromSDKMESH(device, s_modelPaths[i]);
    }

    // Upload textures to GPU.
    ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin();

    m_bokehDOF = std::make_unique<BokehEffect>(device, c_colorFormat, m_graphicsMemory.get(), resourceUpload);
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

    auto postRtState = RenderTargetState(m_deviceResources->GetBackBufferFormat(), DXGI_FORMAT_UNKNOWN);
    m_copyShader = std::make_unique<BasicPostProcess>(device, postRtState, BasicPostProcess::Copy);
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();
    const auto size = m_deviceResources->GetOutputSize();

    // Calculate display dimensions.
    m_displayWidth = size.right - size.left;
    m_displayHeight = size.bottom - size.top;

    m_bokehDOF->ResizeResources(device, m_displayWidth, m_displayHeight);

    // create scene render target
    D3D12_CLEAR_VALUE colorClearValue = CD3DX12_CLEAR_VALUE(c_colorFormat, DirectX::Colors::Transparent);
    CreateColorTextureAndViews(device, m_displayWidth, m_displayHeight, c_colorFormat, m_sceneColor.ReleaseAndGetAddressOf(), m_rtvPile->GetCpuHandle(RTV_SceneColor), m_cpuPile->GetCpuHandle(SRV_SceneColor), &colorClearValue);
    device->CopyDescriptorsSimple(1, m_srvPile->GetCpuHandle(SRV_SceneColor), m_cpuPile->GetCpuHandle(SRV_SceneColor), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CLEAR_VALUE depthClearValue = CD3DX12_CLEAR_VALUE(c_depthFormat, 1.0f, 0x0);
    CreateDepthTextureAndViews(device, m_displayWidth, m_displayHeight, c_depthFormat, m_sceneDepth.ReleaseAndGetAddressOf(), m_dsvPile->GetCpuHandle(DSV_SceneDepth), m_cpuPile->GetCpuHandle(SRV_SceneDepth), &depthClearValue);
    device->CopyDescriptorsSimple(1, m_srvPile->GetCpuHandle(SRV_SceneDepth), m_cpuPile->GetCpuHandle(SRV_SceneDepth), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Set hud sprite viewport
    m_hudBatch->SetViewport(m_deviceResources->GetScreenViewport());

    // Begin uploading texture resources
    {
        ResourceUploadBatch resourceUpload(device);
        resourceUpload.Begin();

        m_smallFont = std::make_unique<SpriteFont>(device, resourceUpload,
            (size.bottom > 1080) ? L"SegoeUI_36.spritefont" : L"SegoeUI_18.spritefont",
            m_srvPile->GetCpuHandle(DescriptorHeapIndex::SRV_Font),
            m_srvPile->GetGpuHandle(DescriptorHeapIndex::SRV_Font));

        m_ctrlFont = std::make_unique<SpriteFont>(device, resourceUpload,
            (size.bottom > 1080) ? L"XboxOneControllerLegend.spritefont" : L"XboxOneControllerLegendSmall.spritefont",
            m_srvPile->GetCpuHandle(DescriptorHeapIndex::SRV_CtrlFont),
            m_srvPile->GetGpuHandle(DescriptorHeapIndex::SRV_CtrlFont));

        auto finished = resourceUpload.End(m_deviceResources->GetCommandQueue());
        finished.wait();
    }
}

void Sample::SetPredefinedScene(int index)
{
    switch (index)
    {
    default:

        // macro scene
    case 0:
        m_bokehParams.focusLength = 0.075f;    // 75 mm lens
        m_bokehParams.FNumber = 2.8f;          // F/2.8 aperture
        m_bokehParams.focalPlane = 0.5f;       // focus distance
        m_cameraAngle = -0.8f;                   // radians
        m_cameraElevation = 0.8f;
        m_cameraDistance = 1.1f;
        break;

    case 1:
        // default scene
        m_bokehParams.focusLength = 0.075f;    // 75 mm lens
        m_bokehParams.FNumber = 2.8f;          // F/2.8 aperture
        m_bokehParams.focalPlane = 2.5f;       // focus distance
        m_cameraAngle = -0.8f;                   // radians
        m_cameraElevation = 0.8f;
        m_cameraDistance = 2.3f;
        break;

        // defocused background
    case 2:
        m_bokehParams.focusLength = 0.075f;    // 75 mm lens
        m_bokehParams.FNumber = 2.8f;          // F/2.8 aperture
        m_bokehParams.focalPlane = 1.f;        // focus distance
        m_cameraAngle = -2.4f;                   // radians
        m_cameraElevation = 0.8f;
        m_cameraDistance = 2.5f;
        break;

        // doll house
    case 3:
        m_bokehParams.focusLength = 0.175f;    // 175 mm lens
        m_bokehParams.FNumber = 2.8f;          // F/2.8 aperture
        m_bokehParams.focalPlane = 2.5f;       // focus distance
        m_cameraAngle = -1.28f;                  // radians
        m_cameraElevation = 0.8f;
        m_cameraDistance = 3.1f;
        break;
    }
}

void Sample::CalculateCameraMatrix()
{
    XMVECTOR eye = XMVectorSet(sinf(m_cameraAngle) * m_cameraDistance, m_cameraElevation, cosf(m_cameraAngle) * m_cameraDistance, 0);

    m_matView = XMMatrixLookAtLH(eye, XMVectorSet(0, 0, 0, 0), XMVectorSet(0, 1, 0, 0));
    m_matProj = XMMatrixPerspectiveFovLH(XM_PI / 4.f, float(m_displayWidth) / float(m_displayHeight), 0.05f, 100);
    m_matInvProj = XMMatrixInverse(nullptr, m_matProj);
}
#pragma endregion
