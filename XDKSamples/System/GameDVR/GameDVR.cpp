//--------------------------------------------------------------------------------------
// GameDVR.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "ATGColors.h"
#include "GameDVR.h"

using namespace Windows::Xbox::Input;
using namespace Windows::Foundation::Collections;
using namespace Windows::Xbox::Media::Capture;
using namespace Windows::Foundation;
using namespace Windows::Storage::Streams;

extern void ExitSample();

using namespace DirectX;

using Microsoft::WRL::ComPtr;

Sample::Sample() :
    m_frame(0)
    , m_curRotationAngleRad(0.0f)
    , m_totalCaptureTime(0.0f)
    , m_appState(APP_INIT)
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

    // Initialize the world matrices
    m_mWorld = XMMatrixIdentity();
    
    // Initialize the view matrix
    static const XMVECTORF32 s_eye = { { { 0.0f, 4.0f, -10.0f, 0.0f } } };
    static const XMVECTORF32 s_at = { { { 0.0f, 1.0f, 0.0f, 0.0f } } };
    static const XMVECTORF32 s_up = { { { 0.0f, 1.0f, 0.0f, 0.0f } } };
    m_mView = XMMatrixLookAtLH(s_eye, s_at, s_up);
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
void Sample::Update(DX::StepTimer const& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    float elapsedTime = float(timer.GetElapsedSeconds());

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

    // SAMPLE LOGIC
    UpdateCube(elapsedTime);

    switch (m_appState)
    {
    case APP_INIT:
        // Note: The GameDVR service automatically detects user permissions 
        // (e.g. can record/upload clips) and will enable/disable recording based 
        // upon those permissions.

        // Retrieve the currently signed-in user
        if (m_user == nullptr)
        {
            m_user = Windows::Xbox::ApplicationModel::Core::CoreApplicationContext::CurrentUser;
        }

        // Reset necessary variables for capture
        m_totalCaptureTime = 0;
        m_messageQueue.clear();

        if (m_user == nullptr)
        {   
            m_appState = APP_AWAITING_LOGIN;

            concurrency::create_task(
                Windows::Xbox::UI::SystemUI::ShowAccountPickerAsync(
                    nullptr,
                    Windows::Xbox::UI::AccountPickerOptions::None
                )
            ).then([this](concurrency::task<Windows::Xbox::UI::AccountPickerResult^> t)
            {
                try
                {
                    Windows::Xbox::UI::AccountPickerResult^ result = t.get();
                    
                    if (result->User == nullptr)
                    {
                        // User canceled the dialog
                        return;
                    }
                    else
                    {
                        m_user = (Windows::Xbox::System::User^)result->User;
                        m_appState = APP_INIT;
                    }
                }
                catch (...)
                {

                }
            });

            break;
        }

        DisplayMessage(L"Press A to start recording for 30 seconds.");

        // If A was pressed, begin recording
        if (pad.IsAPressed())
        {
            m_appState = APP_START_RECORDING;
            DisplayMessage(L"Starting clip capture...");
        }

        break;

    case APP_AWAITING_LOGIN:
    {
        break;
    }

    case APP_START_RECORDING:
    {
        // Set up the clip to record past 30 seconds
        DateTime start, end;

        ::GetSystemTimeAsFileTime(reinterpret_cast< FILETIME* >(&end));
        start.UniversalTime = end.UniversalTime - 30LL * 10000000LL;          // Subtract 30 seconds from the end time

        m_appClipInfo = ref new ApplicationClipInfo(L"Your GreatestMoment Id here");
        m_appClipInfo->LocalizedClipName = ref new Platform::String(L"This Shows up in Toast");

        // Create the ApplicationClipCapture instance
        m_appCapture = ref new ApplicationClipCapture();
        auto asyncOp = m_appCapture->RecordTimespanAsync(m_user, m_appClipInfo, start, end);
        DisplayMessage(L"Executing RecordTimespanAsync()...");

        // Lambda used to handle asynchronous execution
        // This will be executed upon recording completion
        auto currentUser = m_user;
        asyncOp->Completed = ref new AsyncOperationWithProgressCompletedHandler< ApplicationClip^, ApplicationClip^ >(
            [this, currentUser](IAsyncOperationWithProgress< ApplicationClip^, ApplicationClip^ >^ operation,
                Windows::Foundation::AsyncStatus status)
        {
            try
            {
                wchar_t resultMessageBuffer[128] = {};

                if (status == Windows::Foundation::AsyncStatus::Completed && operation->ErrorCode.Value == 0)
                {
                    DisplayMessage(L"Successfully captured clip.");

                    ApplicationClip^ clip = operation->GetResults();
                    swprintf_s(resultMessageBuffer, L"-> Clip local ID: %ls", clip->LocalId->Data());
                    DisplayMessage(resultMessageBuffer);

                    DisplayMessage(L"Press A to query available clips.");
                    m_appState = APP_DONE_RECORDING;
                }
                else
                {
                    swprintf_s(resultMessageBuffer, L"-> Failed to capture clip. Error code: %d", operation->ErrorCode.Value);
                    DisplayMessage(resultMessageBuffer);
                    m_appState = APP_DONE_ERROR;
                }
            }
            catch (Platform::Exception^ ex)
            {
                wchar_t exceptionMessageBuffer[128] = {};
                swprintf_s(exceptionMessageBuffer, L"Exception thrown. Error code: %d", ex->HResult);
                DisplayMessage(exceptionMessageBuffer);
                m_appState = APP_DONE_ERROR;
            }
        });

        m_appState = APP_RECORDING;
        break;
    }

    case APP_RECORDING:
        m_totalCaptureTime += elapsedTime;
        break;

    case APP_DONE_RECORDING:
        if (pad.IsAPressed())
        {
            m_appState = APP_START_QUERY;
        }

        break;

    case APP_START_QUERY:
    {
        DisplayMessage(L"Executing GetClipsAsync()...");

        m_appClipQuery = ref new ApplicationClipQuery();
        auto queryOp = m_appClipQuery->GetClipsAsync(m_user);

        // Lambda used to handle asynchronous execution
        // This will be executed upon querying completion
        queryOp->Completed = ref new AsyncOperationCompletedHandler< IVectorView< ApplicationClip^ >^ >(
            [this](IAsyncOperation< IVectorView< ApplicationClip^ >^ >^ operation,
                Windows::Foundation::AsyncStatus status)
        {
            try
            {
                wchar_t resultMessageBuffer[128] = {};

                if (status == Windows::Foundation::AsyncStatus::Completed && operation->ErrorCode.Value == 0)
                {
                    IVectorView< ApplicationClip^ >^ clips = operation->GetResults();

                    swprintf_s(resultMessageBuffer, L"-> Query successful. Found %u", clips->Size);
                    DisplayMessage(resultMessageBuffer);

                    ApplicationClip^ clip = clips->GetAt(clips->Size - 1);
                    IVectorView< ApplicationClipLocation^ >^ locations = clip->Locations;
                    auto uri = locations->GetAt(locations->Size - 1)->Uri;

                    wchar_t uriBuffer[128] = {};
                    swprintf_s(uriBuffer, L"-> Latest clip URI: : %ls", uri->DisplayUri->Data());
                    DisplayMessage(uriBuffer);

                    DisplayMessage(L"Press LT+RT+RB to quit sample.");

                    m_appState = APP_DONE;
                }
                else
                {
                    swprintf_s(resultMessageBuffer, L"Failed to query clips. Error code: %d", operation->ErrorCode.Value);
                    DisplayMessage(resultMessageBuffer);
                    m_appState = APP_DONE_ERROR;
                }
            }
            catch (Platform::Exception^ ex)
            {
                wchar_t exceptionMessageBuffer[128] = {};
                swprintf_s(exceptionMessageBuffer, L"Exception thrown. Error code: %d", ex->HResult);
                DisplayMessage(exceptionMessageBuffer);
                m_appState = APP_DONE_ERROR;
            }
        });

        m_appState = APP_QUERY;
        break;
    }

    case APP_QUERY:
        break;

    case APP_DONE:
        break;

    case APP_DONE_ERROR:
        break;
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

    RenderText(context);
    RenderCube(context);

    m_spriteBatch->End();

    PIXEndEvent(context);

    // Show the new frame.
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    m_graphicsMemory->Commit();
    PIXEndEvent(context);
}

//--------------------------------------------------------------------------------------
// Called once per frame to the rotating cube
//--------------------------------------------------------------------------------------
void Sample::UpdateCube(float elapsedTime)
{
    m_curRotationAngleRad += elapsedTime / 3;
    if (m_curRotationAngleRad >= XM_2PI)
    {
        m_curRotationAngleRad -= XM_2PI;
    }


    // Rotate cube around the origin
    m_mWorld = XMMatrixRotationY(m_curRotationAngleRad);

    // Setup our lighting parameters
    XMFLOAT4 lightDirs[2] =
    {
        XMFLOAT4(-0.577f, 0.577f, -0.577f, 1.0f),
        XMFLOAT4(0.0f, 0.0f, -1.0f, 1.0f),
    };
    XMFLOAT4 lightColors[2] =
    {
        XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f),
        XMFLOAT4(0.5f, 0.0f, 0.0f, 1.0f)
    };
}

//--------------------------------------------------------------------------------------
// Called once per frame to render cube
//--------------------------------------------------------------------------------------
void Sample::RenderCube(_In_ ID3D11DeviceContextX* const /* pCtx */)
{
    m_GeoCube->Draw(m_mWorld, m_mView, m_mProjection, ATG::Colors::Green);
}

//--------------------------------------------------------------------------------------
// Called once per frame to render text
//--------------------------------------------------------------------------------------
void Sample::RenderText(_In_ ID3D11DeviceContextX* const /* pCtx */)
{
    // We divide the screen into a course grid of 16x10 cells to make placing UI
    // elements easier.
    D3D11_RECT screenRect = m_deviceResources->GetOutputSize();
    FLOAT fWidth = static_cast<FLOAT>(screenRect.right - screenRect.left) - 1.0f;
    FLOAT fHeight = static_cast<FLOAT>(screenRect.bottom - screenRect.top) - 1.0f;
    FLOAT fGridX = fWidth / 16.0f;
    FLOAT fGridY = fHeight / 10.0f;
    FLOAT fTextHeight = m_font->GetLineSpacing() + 2.0f;  // linespacing for display data

    // Draw the title.
    m_fontLarge->DrawString(m_spriteBatch.get(), L"GameDVR Sample", XMFLOAT2(fGridX, fGridY), ATG::Colors::White);
    
    // Draw the readouts
    for (int i = 0; i < m_messageQueue.size(); ++i)
    {
        m_font->DrawString(m_spriteBatch.get(), m_messageQueue[i].c_str(), XMFLOAT2(fGridX, 4 * fGridY + fTextHeight * i), ATG::Colors::White);
    }

    // When recording, display the current capture time and a "REC" graphic
    if (m_appState == APP_RECORDING)
    {
        wchar_t captureTimeBuffer[128] = {};
        swprintf_s(captureTimeBuffer, L"Time:  %.2f seconds", m_totalCaptureTime);
        m_font->DrawString(m_spriteBatch.get(), captureTimeBuffer, XMFLOAT2(12 * fGridX, 8 * fGridY), ATG::Colors::White);

        m_fontLarge->DrawString(m_spriteBatch.get(), L"REC", XMFLOAT2(12 * fGridX, 2 * fGridY), ATG::Colors::Green);
    }
}

//--------------------------------------------------------------------------------------
// Add some text to render to the screen
//--------------------------------------------------------------------------------------
void Sample::DisplayMessage(const std::wstring& message)
{
    const int MAX_MESSAGE_SIZE = 10;

    m_messageQueue.push_back(message);

    // Message queue shouldn't exceed MAX_MESSAGE_SIZE
    int difference = (int)m_messageQueue.size() - MAX_MESSAGE_SIZE;
    while (difference > 0)
    {
        m_messageQueue.pop_front();
        --difference;
    }
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

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device, m_deviceResources->GetBackBufferCount());

    m_spriteBatch = std::make_unique<SpriteBatch>(m_deviceResources->GetD3DDeviceContext());
    m_font = std::make_unique<SpriteFont>(device, L"SegoeUI_24.spritefont");
    m_fontLarge = std::make_unique<SpriteFont>(device, L"SegoeUI_36.spritefont");
    m_fontExtraLarge = std::make_unique<SpriteFont>(device, L"SegoeUI_48.spritefont");

    m_GeoCube = GeometricPrimitive::CreateCube(m_deviceResources->GetD3DDeviceContext(), 1.5f, false);
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    // Initialize the projection matrix
    auto viewport = m_deviceResources->GetScreenViewport();
    m_mProjection = XMMatrixPerspectiveFovLH(XM_PIDIV4, viewport.Width / viewport.Height, 0.01f, 100.0f);
}
#pragma endregion
