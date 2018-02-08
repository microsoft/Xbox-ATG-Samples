//--------------------------------------------------------------------------------------
// CustomEventProvider.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "CustomEventProvider.h"
#include "EtwScopedEvent.h"

#include "ATGColors.h"
#include "ReadData.h"

extern void ExitSample();

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

//  Need to define the sequence number used to generate unique event IDs.
UINT32  EtwScopedEvent::g_Sequence = 0;

Sample::Sample() :
    m_frame(0)
{
    // Renders only 2D, so no need for a depth buffer.
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN);
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(IUnknown* window)
{
    //  Register the CEP_Main event provider
    EventRegisterCEP_Main();

    m_gamePad = std::make_unique<GamePad>();

    m_deviceResources->SetWindow(window);

    m_deviceResources->CreateDeviceResources();  
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    // Sample code
    m_stressThread = nullptr;

    m_threadCount = 0;
    m_quitStress = FALSE;

    //  Lock us to Core 0
    SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR(1 << 0));

    //  Create a work thread and lock to Core 1
    m_stressThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)Sample::StressThreadEntry, (LPVOID)this, 0, nullptr);
    DX::ThrowIfFailed((m_stressThread == nullptr) ? HRESULT_FROM_WIN32(GetLastError()) : S_OK);
    SetThreadAffinityMask(m_stressThread, DWORD_PTR(1 << 1));
    CloseHandle(m_stressThread);

    //  Spin until stress thread activates
    while (m_threadCount < 1)
    {
        Sleep(10);
    }
}

//--------------------------------------------------------------------------------------
// Name: StressThread
// Desc: Code we can run on another core to simulate doing some real work.
//--------------------------------------------------------------------------------------

DWORD WINAPI Sample::StressThreadEntry( LPVOID lpParam )
{
    assert( lpParam != nullptr );
    ( (Sample*)lpParam )->StressThread();
    return 0;
}

void Sample::StressThread()
{
    m_threadCount.fetch_add(1);

    while( !m_quitStress )
    {
        {
            ETWScopedEvent( L"StressSleep" );
            Sleep( 1 );
        }

        Load1();
    }

    m_threadCount.fetch_sub(1);
}

void Sample::Load1()
{
    ETWScopedEvent( L"Load1" );

    volatile UINT64    sum = 0;

    for ( UINT64 i = 0; i < 1000000; ++i )
    {
        sum = sum + 1;
    }

    Load2();
}

void Sample::Load2()
{
    ETWScopedEvent( L"Load2" );

    volatile UINT64    sum = 0;

    for ( UINT64 i = 0; i < 1000000; ++i )
    {
        sum = sum + 1;
    }
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

    Render();

    PIXEndEvent();
    m_frame++;
}

// Updates the world.
void Sample::Update(DX::StepTimer const& /*timer*/)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    ETWScopedEvent(L"Update");
    EventWriteMark(L"Sample::Update");

    Child1();

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

//--------------------------------------------------------------------------------------
// Name: Child1()
// Desc: Simulated CPU load.
//--------------------------------------------------------------------------------------

void Sample::Child1()
{
    ETWScopedEvent(L"Child1");

    Sleep(1);

    Child2();

    Sleep(2);
}


//--------------------------------------------------------------------------------------
// Name: Child2()
// Desc: Simulated nested CPU load.
//--------------------------------------------------------------------------------------

void Sample::Child2()
{
    ETWScopedEvent(L"Child2");

    Sleep(3);
}

#pragma region Frame Render
// Draws the scene.
void Sample::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    ETWScopedEvent(L"Render");
    EventWriteMark(L"Sample::Render");

    Child1();

    // Prepare the command list to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Render");

    context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_states->DepthNone(), 0);
    context->RSSetState(m_states->CullNone());

    m_effect->Apply(context);

    context->IASetInputLayout(m_inputLayout.Get());

    m_batch->Begin();

    VertexPositionColor v1(Vector3(0.f, 0.5f, 0.5f), Colors::Yellow);
    VertexPositionColor v2(Vector3(0.5f, -0.5f, 0.5f), Colors::Yellow);
    VertexPositionColor v3(Vector3(-0.5f, -0.5f, 0.5f), Colors::Yellow);

    m_batch->DrawTriangle(v1, v2, v3);

    m_batch->End();

    PIXEndEvent(context);

    //  Synthesize some data
    UINT32  syntheticData = rand();
    EventWriteBlockCulled(syntheticData);

    // Show the new frame.
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    m_graphicsMemory->Commit();
    PIXEndEvent();
}

// Helper method to clear the back buffers.
void Sample::Clear()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.
    auto renderTarget = m_deviceResources->GetRenderTargetView();

    context->ClearRenderTargetView(renderTarget, ATG::Colors::Background);

    context->OMSetRenderTargets(1, &renderTarget, nullptr);

    // Set the viewport.
    auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    PIXEndEvent(context);
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

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Sample::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto context = m_deviceResources->GetD3DDeviceContext();

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device, m_deviceResources->GetBackBufferCount());

    m_states = std::make_unique<CommonStates>(device);

    m_effect = std::make_unique<BasicEffect>(device);
    m_effect->SetVertexColorEnabled(true);

    void const* shaderByteCode;
    size_t byteCodeLength;

    m_effect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);

    DX::ThrowIfFailed(
        device->CreateInputLayout(VertexPositionColor::InputElements,
            VertexPositionColor::InputElementCount,
            shaderByteCode, byteCodeLength,
            m_inputLayout.ReleaseAndGetAddressOf()));

    m_batch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(context);
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    
}
#pragma endregion