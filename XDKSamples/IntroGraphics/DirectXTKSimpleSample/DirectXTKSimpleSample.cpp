//--------------------------------------------------------------------------------------
// DirectXTKSimpleSample.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "DirectXTKSimpleSample.h"

extern void ExitSample();

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

Sample::Sample() :
    m_frame(0)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_D32_FLOAT, 2,
        DX::DeviceResources::c_Enable4K_UHD);
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

    // Create DirectXTK for Audio objects
    AUDIO_ENGINE_FLAGS eflags = AudioEngine_UseMasteringLimiter;
#ifdef _DEBUG
    eflags = eflags | AudioEngine_Debug;
#endif

    m_audEngine = std::make_unique<AudioEngine>(eflags);

    m_audioEvent = 0;
    m_audioTimerAcc = 10.f;

    m_waveBank = std::make_unique<WaveBank>(m_audEngine.get(), L"xmadroid.xwb");
    m_soundEffect = std::make_unique<SoundEffect>(m_audEngine.get(), L"MusicMono_xma.wav");
    m_effect1 = m_soundEffect->CreateInstance();
    m_effect2 = m_waveBank->CreateInstance(10);

    m_effect1->Play(true);
    m_effect2->Play();
}

#pragma region Frame Update
// Executes basic render loop.
void Sample::Tick()
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Frame %I64u", m_frame);

    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    // Only update audio engine once per frame
    if (!m_audEngine->Update())
    {
        if (m_audEngine->IsCriticalError())
        {
            // This would only happen if we were rendering to a headset that was disconnected
            // This sample always renders to the 'default' system audio device, not to headsets
            assert(false);
        }
    }

    Render();

    PIXEndEvent();
    m_frame++;
}

// Updates the world.
void Sample::Update(DX::StepTimer const& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    Vector3 eye(0.0f, 0.7f, 1.5f);
    Vector3 at(0.0f, -0.1f, 0.0f);

    m_view = Matrix::CreateLookAt(eye, at, Vector3::UnitY);

    m_world = Matrix::CreateRotationY(float(timer.GetTotalSeconds() * XM_PIDIV4));

    m_batchEffect->SetView(m_view);
    m_batchEffect->SetWorld(Matrix::Identity);

    m_audioTimerAcc -= (float)timer.GetElapsedSeconds();
    if (m_audioTimerAcc < 0)
    {
        m_audioTimerAcc = 4.f;

        m_waveBank->Play(m_audioEvent++);

        if (m_audioEvent >= 11)
            m_audioEvent = 0;
    }

    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        if (pad.IsViewPressed())
        {
            ExitSample();
        }
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

    auto size = m_deviceResources->GetOutputSize();
    RECT safeRect = SimpleMath::Viewport::ComputeTitleSafeArea(size.right, size.bottom);

    // Prepare the render target to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Render");

    // Draw procedurally generated dynamic grid
    const XMVECTORF32 xaxis = { 20.f, 0.f, 0.f };
    const XMVECTORF32 yaxis = { 0.f, 0.f, 20.f };
    DrawGrid(xaxis, yaxis, g_XMZero, 20, 20, Colors::Gray);

    // Draw sprite
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Draw sprite");
    m_sprites->Begin();
    m_sprites->Draw(m_texture2.Get(), XMFLOAT2(float(safeRect.left), float(safeRect.top + 50)), nullptr, Colors::White);

    m_font->DrawString(m_sprites.get(), L"DirectXTK Simple Sample",
        XMFLOAT2(float(safeRect.left), float(safeRect.top)), Colors::Yellow);
    m_sprites->End();
    PIXEndEvent(context);

    // Draw 3D object
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Draw teapot");
    XMMATRIX local = m_world * Matrix::CreateTranslation(-2.f, -2.f, -4.f);
    m_shape->Draw(local, m_view, m_projection, Colors::White, m_texture1.Get());
    PIXEndEvent(context);

    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Draw model");
    const XMVECTORF32 scale = { 0.01f, 0.01f, 0.01f };
    const XMVECTORF32 translate = { 3.f, -2.f, -4.f };
    XMVECTOR rotate = Quaternion::CreateFromYawPitchRoll(XM_PI / 2.f, 0.f, -XM_PI / 2.f);
    local = m_world * XMMatrixTransformation(g_XMZero, Quaternion::Identity, scale, g_XMZero, rotate, translate);
    m_model->Draw(context, *m_states, local, m_view, m_projection);
    PIXEndEvent(context);

    PIXEndEvent(context);

    // Show the new frame.
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    m_graphicsMemory->Commit();
    PIXEndEvent(context);
}

// Helper method to clear the back buffers.
void Sample::Clear()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views
    auto renderTarget = m_deviceResources->GetRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    context->ClearRenderTargetView(renderTarget, Colors::CornflowerBlue);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

    // Set the viewport.
    auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    PIXEndEvent(context);
}

void XM_CALLCONV Sample::DrawGrid(FXMVECTOR xAxis, FXMVECTOR yAxis, FXMVECTOR origin, size_t xdivs, size_t ydivs, GXMVECTOR color)
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Draw grid");

    context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_states->DepthNone(), 0);
    context->RSSetState(m_states->CullCounterClockwise());

    m_batchEffect->Apply(context);

    context->IASetInputLayout(m_batchInputLayout.Get());

    m_batch->Begin();

    xdivs = std::max<size_t>(1, xdivs);
    ydivs = std::max<size_t>(1, ydivs);

    for (size_t i = 0; i <= xdivs; ++i)
    {
        float fPercent = float(i) / float(xdivs);
        fPercent = (fPercent * 2.0f) - 1.0f;
        XMVECTOR vScale = XMVectorScale(xAxis, fPercent);
        vScale = XMVectorAdd(vScale, origin);

        VertexPositionColor v1(XMVectorSubtract(vScale, yAxis), color);
        VertexPositionColor v2(XMVectorAdd(vScale, yAxis), color);
        m_batch->DrawLine(v1, v2);
    }

    for (size_t i = 0; i <= ydivs; i++)
    {
        float fPercent = float(i) / float(ydivs);
        fPercent = (fPercent * 2.0f) - 1.0f;
        XMVECTOR vScale = XMVectorScale(yAxis, fPercent);
        vScale = XMVectorAdd(vScale, origin);

        VertexPositionColor v1(XMVectorSubtract(vScale, xAxis), color);
        VertexPositionColor v2(XMVectorAdd(vScale, xAxis), color);
        m_batch->DrawLine(v1, v2);
    }

    m_batch->End();

    PIXEndEvent(context);
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Sample::OnSuspending()
{
    m_audEngine->Suspend();

    auto context = m_deviceResources->GetD3DDeviceContext();
    context->Suspend(0);
}

void Sample::OnResuming()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    context->Resume();
    m_timer.ResetElapsedTime();

    m_audEngine->Resume();
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Sample::CreateDeviceDependentResources()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto device = m_deviceResources->GetD3DDevice();

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device, m_deviceResources->GetBackBufferCount());

    m_states = std::make_unique<CommonStates>(device);

    m_fxFactory = std::make_unique<EffectFactory>(device);

    m_sprites = std::make_unique<SpriteBatch>(context);

    m_batch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(context);

    m_batchEffect = std::make_unique<BasicEffect>(device);
    m_batchEffect->SetVertexColorEnabled(true);

    {
        void const* shaderByteCode;
        size_t byteCodeLength;

        m_batchEffect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);

        DX::ThrowIfFailed(
            device->CreateInputLayout(VertexPositionColor::InputElements,
                VertexPositionColor::InputElementCount,
                shaderByteCode, byteCodeLength,
                m_batchInputLayout.ReleaseAndGetAddressOf())
            );
    }

    m_font = std::make_unique<SpriteFont>(device, L"SegoeUI_18.spritefont");

    m_shape = GeometricPrimitive::CreateTeapot(context, 4.f, 8);

    // SDKMESH has to use clockwise winding with right-handed coordinates, so textures are flipped in U
    m_model = Model::CreateFromSDKMESH(device, L"tiny.sdkmesh", *m_fxFactory);

    // Load textures
    void *grfxMemory = nullptr; // we just leak the graphics memory...

    DX::ThrowIfFailed(
        Xbox::CreateDDSTextureFromFile(device, L"assets\\seafloor.dds", nullptr, m_texture1.ReleaseAndGetAddressOf(), &grfxMemory)
        );

    DX::ThrowIfFailed(
        Xbox::CreateDDSTextureFromFile(device, L"assets\\windowslogo.dds", nullptr, m_texture2.ReleaseAndGetAddressOf(), &grfxMemory)
        );
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    auto size = m_deviceResources->GetOutputSize();
    float aspectRatio = float(size.right) / float(size.bottom);
    float fovAngleY = 70.0f * XM_PI / 180.0f;

    // This is a simple example of change that can be made when the app is in
    // portrait or snapped view.
    if (aspectRatio < 1.0f)
    {
        fovAngleY *= 2.0f;
    }

    // This sample makes use of a right-handed coordinate system using row-major matrices.
    m_projection = Matrix::CreatePerspectiveFieldOfView(
        fovAngleY,
        aspectRatio,
        0.01f,
        100.0f
        );

    m_batchEffect->SetProjection(m_projection);
}
#pragma endregion

