//--------------------------------------------------------------------------------------
// CoroutinesXDK.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "CoroutinesXDK.h"

#include "ATGColors.h"
#include "ControllerFont.h"

extern void ExitSample() noexcept;

using namespace DirectX;
using namespace Windows::Xbox::UI;

using Microsoft::WRL::ComPtr;

namespace
{
    // This is a simple awaitable type that allows you to query the status of an asynchronous operation and
    //  resume a coroutine once that operation is complete
    struct keyboard_await_adapter
    {
        Windows::Foundation::IAsyncOperation<Platform::String^>^ async;

        bool await_ready()
        {
            return async->Status == Windows::Foundation::AsyncStatus::Completed;
        }

        void await_suspend(std::experimental::coroutine_handle<>) const
        {
        }

        auto await_resume() const
        {
            return async->GetResults();
        }
    };

    // Global co_await operator allows you to use co_await with any type and transform it to another awaitable type
    keyboard_await_adapter operator co_await(Windows::Foundation::IAsyncOperation<Platform::String^> ^ async)
    {
        return { async };
    }

    // This awaitable type allows you to resume an IAsyncOperation<AccountPickerResults^> on a background thread
    //  once it has completed.
    struct accountpicker_await_adapter
    {
        Windows::Foundation::IAsyncOperation<AccountPickerResult^>^ async;

        bool await_ready() const
        {
            return async->Status == Windows::Foundation::AsyncStatus::Completed;
        }

        void await_suspend(std::experimental::coroutine_handle<> handle) const
        {
            winrt::com_ptr<IContextCallback> context;
            winrt::check_hresult(CoGetObjectContext(__uuidof(context), reinterpret_cast<void **>(winrt::put_abi(context))));

            async->Completed = ref new Windows::Foundation::AsyncOperationCompletedHandler<AccountPickerResult^>([handle, context = std::move(context)](const auto, Windows::Foundation::AsyncStatus)
            {
                ComCallData data = {};
                data.pUserDefined = handle.address();

                auto callback = [](ComCallData * data)
                {
                    std::experimental::coroutine_handle<>::from_address(data->pUserDefined)();
                    return S_OK;
                };

                winrt::check_hresult(context->ContextCallback(callback, &data, IID_ICallbackWithNoReentrancyToApplicationSTA, 5, nullptr));
            });
        }

        auto await_resume() const
        {
            return async->GetResults();
        }
    };

    // Global co_await operator allows you to use co_await with any type and transform it to another awaitable type
    accountpicker_await_adapter operator co_await(Windows::Foundation::IAsyncOperation<AccountPickerResult^> ^ async)
    {
        return { async };
    }
}

Sample::Sample() :
    m_frame(0)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
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

    m_stringAsync = nullptr;
    m_acquiringNewUser = false;
}

std::future<void> Sample::UpdateUsersOffThread()
{
    // Start on the same thread as the caller
    EmitThreadIdDebug();

    // This type will resume immediately on another thread
    co_await winrt::resume_background{};

    std::lock_guard<std::mutex> lock(m_usersMutex);

    // This is a long operation so do it on a thread that isn't rendering
    m_users = Windows::Xbox::System::User::Users;
    EmitThreadIdDebug();
}

std::future<AccountPickerResult^> Sample::GetNewUser()
{
    m_acquiringNewUser = true;
    auto op = SystemUI::ShowAccountPickerAsync(nullptr, AccountPickerOptions::None);

    // This requires a local variable for string formatting. Do it in a subroutine
    //  so it is not a part of the coroutine context allocation.
    EmitThreadIdDebug();

    // This coroutine will suspend here and return execution to the caller. Once the
    //  account picker operation is complete the coroutine will resume on a background
    //  thread because this is using the overloaded co_await operator that transforms
    //  an AccountPickerResult^ into our custom accountpicker_await_adapter object.
    //  Once the operation is complete, all locals and variable declared before this
    //  statement, such as the AccountPickerResult^ object itself, are accessible.
    co_await op;

    // This part of the coroutine is running on a different thread.
    EmitThreadIdDebug();

    AccountPickerResult^ results = nullptr;
    try
    {
        results = op->GetResults();

        // lock guard is required since m_currentUser is accessed across multiple threads
        std::lock_guard<std::mutex> lock(m_userMutex);
        m_currentUser = results->User;
    }
    catch (...)
    {
        // GetResults can throw an exception if the TCUI failed for some reason. There isn't much
        //  for this sample to do. It should be handled in a way that's appropriate to game code.
    }

    m_acquiringNewUser = false;
    co_return results;
}

awaitable_future<Platform::String^> Sample::GetNewDisplayString()
{
    m_displayString = nullptr;

    auto async = SystemUI::ShowVirtualKeyboardAsync(L"String", L"Virtual Keyboard", L"Provide a string", VirtualKeyboardInputScope::Default);
    m_stringAsync = async;
    auto newString = co_await async;

    m_displayString = newString;
    co_return newString;
}

// This is a normal subroutine which means local variables will be put on the stack
//  instead of part of an allocation on the heap.
void Sample::EmitThreadIdDebug()
{
    wchar_t buffer[100];
    swprintf_s(buffer, L"Current thread ID: %i\n", GetCurrentThreadId());
    OutputDebugString(buffer);
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
void Sample::Update(DX::StepTimer const&)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);

        if (m_gamePadButtons.a)
        {
            if(!m_acquiringNewUser)
                GetNewUser();
        }

        if (m_gamePadButtons.x)
        {
            UpdateUsersOffThread();
        }

        if (m_gamePadButtons.y)
        {
            m_displayString = nullptr;
            m_future = GetNewDisplayString();
        }

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

    // Prepare the render target to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Render");

    m_spriteBatch->Begin();

    float posx = 60.f;
    float posy = 20.f;
    
    wchar_t buffer[2000] = {};
    {
        // lock guard is required since m_currentUser is accessed across multiple threads
        std::lock_guard<std::mutex> lock(m_userMutex);
        swprintf_s(buffer, L"Press [A] to change current user\nPress [X] to query users\nPress [Y] to open virtual keyboard\nCurrent user: %ls",
            (m_currentUser == nullptr ? L"<None>" : m_currentUser->DisplayInfo->Gamertag->Begin()));
    }

    DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(), buffer, DirectX::XMFLOAT2{ posx, posy }, DirectX::Colors::White);

    posy += 200.f;

    if (m_users != nullptr)
    {
        m_font->DrawString(m_spriteBatch.get(), L"Users:", DirectX::XMFLOAT2{ posx, posy }, DirectX::Colors::White);
        for (uint32 i = 0; i < m_users->Size; ++i)
        {
            posy += 35.f;
            Platform::String^ display = m_users->GetAt(i)->DisplayInfo->Gamertag;
            m_font->DrawString(m_spriteBatch.get(), display->Begin(), DirectX::XMFLOAT2{ posx, posy }, DirectX::Colors::White);
        }
    }

    // Poll for completion of virtual keyboard event in the string coroutine. Once it is completed
    //  the coroutine can be resumed to finish the process.
    if (m_displayString == nullptr)
    {
        if (m_stringAsync != nullptr)
        {
            if (m_stringAsync->Status == Windows::Foundation::AsyncStatus::Completed)
            {
                m_future.resume();
                m_future.release();
                m_stringAsync = nullptr;
            }
        }
    }
    else
    {
        m_font->DrawString(m_spriteBatch.get(), m_displayString->Begin(), DirectX::XMFLOAT2{ 640.f, 20.f }, DirectX::Colors::White);
    }

    m_spriteBatch->End();

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

    // Clear the views.
    auto renderTarget = m_deviceResources->GetRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    context->ClearRenderTargetView(renderTarget, ATG::Colors::Background);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

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

    m_font = std::make_unique<SpriteFont>(device, L"SegoeUI_24.spritefont");
    m_ctrlFont = std::make_unique<SpriteFont>(device, L"XboxOneControllerLegendSmall.spritefont");
    m_spriteBatch = std::make_unique<SpriteBatch>(context);
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    auto viewport = m_deviceResources->GetScreenViewport();
    m_spriteBatch->SetViewport(viewport);
}
#pragma endregion
