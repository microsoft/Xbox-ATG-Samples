//--------------------------------------------------------------------------------------
// UserManagement.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "UserManagement.h"

#include "ATGColors.h"
#include "ControllerFont.h"

extern void ExitSample();

using namespace DirectX;
using namespace Windows::Foundation;
using namespace Windows::Xbox::Input; // contains Controller class
using namespace Windows::Xbox::System; // contains User class

using Microsoft::WRL::ComPtr;

namespace
{
    std::mutex g_consoleMutex;
    
    inline std::wstring FormatHResult(int hResult)
    {
        wchar_t hrBuf[16] = {};
        swprintf_s(hrBuf, L"0x%08X", hResult);
        return std::wstring(hrBuf);
    }

    inline std::wstring FormatControllerType(Platform::String^ controllerType)
    {
        if (!controllerType)
        {
            return std::wstring();
        }

        const wchar_t* classPrefix = wcsrchr(controllerType->Data(), L'.');
        if (classPrefix)
        {
            return std::wstring(classPrefix + 1);
        }
        else
        {
            return std::wstring(controllerType->Data());
        }
    }

    inline std::wstring GetGamertag(User^ user)
    {
        if (user)
        {
            return std::wstring(user->DisplayInfo->Gamertag->Data());
        }
        else
        {
            return std::wstring(L"<no user>");
        }
    }

    inline bool IsUserStale(User^ user)
    {
        // User only becomes stale when user signs out. It never transitions from IsSignedIn = false -> true.
        return !user->IsSignedIn;
    }

    User^ UpdateStaleUser(User^ staleUser)
    {
        auto users = User::Users;
        for (auto freshUser : users)
        {
            if (Platform::String::CompareOrdinal(staleUser->XboxUserId, freshUser->XboxUserId) == 0)
            {
                return freshUser;
            }
        }

        return nullptr;
    }
}

Sample::Sample() :
    m_frame(0),
    m_signOutDeferralTimeInSeconds(5)
{
    // Renders only 2D, so no need for a depth buffer.
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

    // Sign up for controller events.
    Controller::ControllerAdded += ref new EventHandler<ControllerAddedEventArgs^>([=](Platform::Object^, ControllerAddedEventArgs^ args)
    {
        std::lock_guard<std::mutex> lock(g_consoleMutex); // lock to keep console output together

        auto controllerId = args->Controller->Id;
        auto controllerType = args->Controller->Type;
        auto controllerUser = args->Controller->User;

        m_console->Format(Colors::Aquamarine, L"EVENT: Controller Added, %ls ID: %lld, Paired User: %ls\n",
            FormatControllerType(controllerType).c_str(), 
            controllerId,
            GetGamertag(controllerUser).c_str());
    });

    Controller::ControllerPairingChanged += ref new EventHandler<ControllerPairingChangedEventArgs^>([=](Platform::Object^, ControllerPairingChangedEventArgs^ args)
    {
        std::lock_guard<std::mutex> lock(g_consoleMutex); // lock to keep console output together

        auto controllerId = args->Controller->Id;
        auto controllerType = args->Controller->Type;
        auto controllerPrevUser = args->PreviousUser;
        auto controllerUser = args->Controller->User;

        m_console->Format(Colors::Aquamarine, L"EVENT: Controller Pairing Changed, %ls ID: %lld, Previous User: %ls, New User: %ls\n",
            FormatControllerType(controllerType).c_str(), 
            controllerId,
            GetGamertag(controllerPrevUser).c_str(),
            GetGamertag(controllerUser).c_str());
    });

    Controller::ControllerRemoved += ref new EventHandler<ControllerRemovedEventArgs^>([=](Platform::Object^, ControllerRemovedEventArgs^ args)
    {
        std::lock_guard<std::mutex> lock(g_consoleMutex); // lock to keep console output together

        auto controllerId = args->Controller->Id;
        auto controllerType = args->Controller->Type;
        auto controllerUser = args->Controller->User;

        m_console->Format(Colors::Aquamarine, L"EVENT: Controller Removed, %ls ID: %lld, Paired User: %ls\n",
            FormatControllerType(controllerType).c_str(),
            controllerId,
            GetGamertag(controllerUser).c_str());
    });

    // Sign up for user events.
    Windows::Xbox::ApplicationModel::Core::CoreApplicationContext::CurrentUserChanged += ref new EventHandler<Platform::Object ^>([=](Platform::Object^, Platform::Object^ args)
    {
        auto prevCurrentUser = m_currentUser;
        m_currentUser = Windows::Xbox::ApplicationModel::Core::CoreApplicationContext::CurrentUser;

        std::lock_guard<std::mutex> lock(g_consoleMutex); // lock to keep console output together

        m_console->Format(Colors::Lime, L"EVENT: Current User Changed, from %ls to %ls\n",
            GetGamertag(prevCurrentUser).c_str(),
            GetGamertag(m_currentUser).c_str());
    });

    User::UserAdded += ref new EventHandler<UserAddedEventArgs^>([=](Platform::Object^, UserAddedEventArgs^ args)
    {
        std::lock_guard<std::mutex> lock(g_consoleMutex); // lock to keep console output together
        
        auto user = args->User;

        m_console->Format(Colors::Lime, L"EVENT: User Added, Gamertag: %ls\n", GetGamertag(user).c_str());
        WriteUser(user);
    });

    User::UserRemoved += ref new EventHandler<UserRemovedEventArgs^>([=](Platform::Object^, UserRemovedEventArgs^ args)
    {
        std::lock_guard<std::mutex> lock(g_consoleMutex); // lock to keep console output together
        
        auto user = args->User;

        m_console->Format(Colors::Lime, L"EVENT: User Removed, Gamertag: %ls\n", GetGamertag(user).c_str());
        WriteUser(user);
    });

    User::SignInCompleted += ref new EventHandler<SignInCompletedEventArgs^>([=](Platform::Object^, SignInCompletedEventArgs^ args)
    {
        std::lock_guard<std::mutex> lock(g_consoleMutex); // lock to keep console output together
        
        auto user = args->User;

        m_console->Format(Colors::Lime, L"EVENT: Sign In Completed, Gamertag: %ls\n", GetGamertag(user).c_str());
        WriteUser(user);
    });

    User::SignOutStarted += ref new EventHandler<SignOutStartedEventArgs^>([=](Platform::Object^, SignOutStartedEventArgs^ args)
    {
        std::lock_guard<std::mutex> lock(g_consoleMutex); // lock to keep console output together

        auto user = args->User;

        m_console->Format(Colors::Lime, L"EVENT: Sign Out Started, Gamertag: %ls...\n", GetGamertag(user).c_str());

        if (m_signOutDeferralTimeInSeconds > 0)
        {
            auto signOutDeferral = args->GetDeferral();
            m_console->Format(L"  User Sign Out Deferral Started at %0.2f seconds\n", m_timer.GetTotalSeconds());

            Concurrency::create_task([this, signOutDeferral]()
            {
                Concurrency::wait(m_signOutDeferralTimeInSeconds * 1000);
                signOutDeferral->Complete();

                std::lock_guard<std::mutex> lock(g_consoleMutex); // lock to keep console output together
                
                m_console->Format(L"  User Sign Out Deferral Completed at %0.2f seconds\n", m_timer.GetTotalSeconds());
            });
        }
    });

    User::SignOutCompleted += ref new EventHandler<SignOutCompletedEventArgs^>([=](Platform::Object^, SignOutCompletedEventArgs^ args)
    {
        std::lock_guard<std::mutex> lock(g_consoleMutex); // lock to keep console output together

        auto user = args->User;

        if (m_activeUser == user)
        {
            m_console->Format(Colors::Lime, L"User %ls signed out.. resetting active user\n", GetGamertag(user).c_str());
            m_activeUser = nullptr;
        }

        m_console->Format(Colors::Lime, L"EVENT: Sign Out Completed, Gamertag: %ls\n", GetGamertag(user).c_str());
    });

    // Display current Controller / User configuration on startup.
    std::lock_guard<std::mutex> lock(g_consoleMutex); // lock to keep console output together
    m_console->WriteLine(L"==== Startup Configuration ====");
    WriteControllers();
    WriteUsers();
    m_console->WriteLine(L"===============================");
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
void Sample::Update(DX::StepTimer const&)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    for (int i = 0; i < DirectX::GamePad::MAX_PLAYER_COUNT; ++i)
    {
        auto pad = m_gamePad->GetState(i);
        if (pad.IsConnected())
        {
            m_gamePadButtons[i].Update(pad);

            if (pad.IsViewPressed())
            {
                ExitSample();
            }

            if (m_gamePadButtons[i].menu == GamePad::ButtonStateTracker::PRESSED)
            {
                // Show account picker.
                auto gamepad = GetGamepad(i);
                PickUserAsync(gamepad, Windows::Xbox::UI::AccountPickerOptions::AllowGuests);
            }
            else if (m_gamePadButtons[i].x == GamePad::ButtonStateTracker::PRESSED)
            {
                // Write controller info to console window.
                if (pad.IsLeftTriggerPressed())
                {
                    // Write info on only this controller.
                    m_console->WriteLine(L"-- This Controller --");
                    auto gamepad = GetGamepad(i);
                    WriteController(gamepad);
                }
                else
                {
                    // Write info on all controllers.
                    WriteControllers();
                }
            }
            else if (m_gamePadButtons[i].y == GamePad::ButtonStateTracker::PRESSED)
            {
                // Write info on all users to console window.
                WriteUsers();
            }
            else if (m_gamePadButtons[i].a == GamePad::ButtonStateTracker::PRESSED)
            {
                // Set active user if not already set.
                if (!m_activeUser)
                {
                    auto gamepad = GetGamepad(i);
                    auto pairedUser = gamepad->User;

                    if (pairedUser)
                    {
                        // Set active user to the engaging controller's paired user.
                        m_activeUser = pairedUser;

                        m_console->Format(Colors::Lime, L"Active User set to %ls\n", GetGamertag(pairedUser).c_str());
                    }
                    else
                    {
                        // Use account picker to try to pair a user to the engaging controller.
                        PickUserAsync(gamepad, Windows::Xbox::UI::AccountPickerOptions::None).then([=](User^ userPicked)
                        {
                            m_activeUser = userPicked;
                            if (m_activeUser)
                            {
                                m_console->Format(Colors::Lime, L"Active User set to %ls\n", GetGamertag(userPicked).c_str());
                            }
                            else
                            {
                                m_console->WriteLine(ATG::Colors::Orange, L"No user chosen.. active user not set");
                            }
                        });
                    }
                }
            }
            else if (m_gamePadButtons[i].b == GamePad::ButtonStateTracker::PRESSED)
            {
                // Reset active user.
                m_activeUser = nullptr;

                m_console->WriteLine(Colors::Lime, L"Active User reset");
            }
            else if (m_gamePadButtons[i].dpadLeft == GamePad::ButtonStateTracker::PRESSED)
            {
                // Decrease sign out deferral time.
                if (m_signOutDeferralTimeInSeconds > 0)
                {
                    m_signOutDeferralTimeInSeconds--;
                }
            }
            else if (m_gamePadButtons[i].dpadRight == GamePad::ButtonStateTracker::PRESSED)
            {
                // Increase sign out deferral time.
                if (m_signOutDeferralTimeInSeconds < 60)
                {
                    m_signOutDeferralTimeInSeconds++;
                }
            }
        }
        else
        {
            m_gamePadButtons[i].Reset();
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

    // Prepare the render target to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    // Calculate the title-safe region.
    auto fullScreen = m_deviceResources->GetOutputSize();
    auto safe = SimpleMath::Viewport::ComputeTitleSafeArea(fullScreen.right, fullScreen.bottom);

    // Start rendering...
    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Render");

    m_spriteBatch->Begin();

    // Prompt to acquire user if needed
    if (!m_activeUser)
    {
        static const wchar_t setActiveUserPrompt[] = L"Press [A] to Set Active User";
        auto promptWidth = m_smallFont->MeasureString(setActiveUserPrompt);
        DX::DrawControllerString(m_spriteBatch.get(), m_smallFont.get(), m_controllerLegendFont.get(),
            setActiveUserPrompt,
            XMFLOAT2(float((safe.right - safe.left) / 2) - (XMVectorGetX(promptWidth) / 2.f), float(safe.bottom - safe.top) / 2.f),
            Colors::Lime,
            1.5f);
    }

    // Render console window area.
    m_console->Render();

    // Render status line (top).
    wchar_t statusStr[256] = {};
    auto activeUser = m_activeUser;
    auto currentUser = m_currentUser;
    swprintf_s(statusStr, L"Active User: %ls   Current User Property: %ls   Sign Out Deferral: %d seconds", 
        GetGamertag(activeUser).c_str(),
        GetGamertag(currentUser).c_str(),
        m_signOutDeferralTimeInSeconds);
    m_smallFont->DrawString(m_spriteBatch.get(), statusStr, XMFLOAT2(float(safe.left), float(safe.top)), Colors::Lime);

    // Render controller legend (bottom).
    DX::DrawControllerString(m_spriteBatch.get(), m_smallFont.get(), m_controllerLegendFont.get(),
        L"[View] Exit   [Menu] Show Account Picker   [X] List Controllers   [LT] +[X] List This Controller   [Y] List Users   [B] Reset Active User   [DPad] Adjust Sign Out Deferral",
        XMFLOAT2(float(safe.left), float(safe.bottom) - m_smallFont->GetLineSpacing()),
        ATG::Colors::LightGrey);

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

    m_console->WriteLine(Colors::Yellow, L"EVENT: Suspending");
    
    m_gamePad->Suspend();
}

void Sample::OnResuming()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    context->Resume();

    m_console->WriteLine(Colors::Yellow, L"EVENT: Resuming");

    m_gamePad->Resume();
    for (int i = 0; i < DirectX::GamePad::MAX_PLAYER_COUNT; ++i)
    {
        m_gamePadButtons[i].Reset();
    }

    // Check for a stale user object on resuming from suspend, refreshing as needed.
    if (m_activeUser && IsUserStale(m_activeUser))
    {
        m_console->WriteLine(Colors::Lime, L"Found stale active user");
        auto staleGamertag = GetGamertag(m_activeUser);

        m_activeUser = UpdateStaleUser(m_activeUser);

        if (m_activeUser)
        {
            m_console->Format(Colors::Lime, L"Freshened active user %ls\n", GetGamertag(m_activeUser).c_str());
        }
        else
        {
            m_console->Format(Colors::Lime, L"User %ls no longer signed in.. resetting active user\n", staleGamertag.c_str());
        }
    }

    m_timer.ResetElapsedTime();
}

void Sample::OnVisibilityChanged(Windows::UI::Core::VisibilityChangedEventArgs^ args)
{
    m_console->Format(Colors::Yellow, L"EVENT: OnVisibilityChanged, Visible: %ls\n", args->Visible.ToString()->Data());
}

#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Sample::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto context = m_deviceResources->GetD3DDeviceContext();

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device, m_deviceResources->GetBackBufferCount());

    m_spriteBatch = std::make_unique<SpriteBatch>(context);
    m_smallFont = std::make_unique<SpriteFont>(device, L"SegoeUI_18.spritefont");
    m_controllerLegendFont = std::make_unique<SpriteFont>(device, L"XboxOneControllerLegendSmall.spritefont");

    m_console = std::make_unique<DX::TextConsole>(context, L"Courier_16.spritefont");
    m_console->SetForegroundColor(ATG::Colors::LightGrey);
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    RECT fullScreen = m_deviceResources->GetOutputSize();

    // Define the sample console window region.
    auto consoleWindow = SimpleMath::Viewport::ComputeTitleSafeArea(fullScreen.right, fullScreen.bottom);
    consoleWindow.top += long(2.f * m_smallFont->GetLineSpacing());
    consoleWindow.bottom -= long(m_smallFont->GetLineSpacing());
    m_console->SetWindow(consoleWindow);
}

// Get platform gamepad from DirectXTK player index.
Windows::Xbox::Input::IGamepad ^ Sample::GetGamepad(int index)
{
    auto gamepadId = m_gamePad->GetCapabilities(index).id;

    if (gamepadId == 0)
    {
        return nullptr;
    }

    auto gamepads = Windows::Xbox::Input::Gamepad::Gamepads;
    for (auto gamepad : gamepads)
    {
        if (gamepad->Id == gamepadId)
        {
            return gamepad;
        }
    }

    return nullptr;
}

Concurrency::task<User^> Sample::PickUserAsync(IController^ controller, Windows::Xbox::UI::AccountPickerOptions options)
{
    return Concurrency::create_task(Windows::Xbox::UI::SystemUI::ShowAccountPickerAsync(controller, options)).then
    ([this](Concurrency::task<Windows::Xbox::UI::AccountPickerResult^> t) -> Windows::Xbox::System::User^
    {
        try
        {
            auto result = t.get();

            if (result->User)
            {
                m_console->Format(Colors::Lime, L"Picked User: %ls\n", GetGamertag((User^)result->User).c_str());
            }
            else
            {
                m_console->WriteLine(ATG::Colors::Orange, L"User canceled account picker");
            }
            return (User^)result->User;
        }
        catch (Platform::Exception^ ex)
        {
            m_console->Format(ATG::Colors::Orange, L"ShowAccountPickerAsync threw error: %ls\n", FormatHResult(ex->HResult).c_str());
        }
        catch (concurrency::task_canceled&)
        {
            m_console->WriteLine(ATG::Colors::Orange, L"ShowAccountPickerAsync canceled"); // system canceled picker, e.g. user may have pressed Xbox button to go Home
        }
        return nullptr;
    });
}

// Write a list of all active controllers to the sample console window.
void Sample::WriteControllers()
{
    m_console->WriteLine(L"-- Controllers --");

    auto controllers = Windows::Xbox::Input::Controller::Controllers;
    if (controllers->Size == 0)
    {
        m_console->WriteLine(L"   <none>");
        return;
    }

    for (auto&& controller : controllers)
    {
        WriteController(controller);
    }
}

// Write information about the controller passed in to the sample console window.
void Sample::WriteController(IController^ controller)
{
    auto controllerId = controller->Id;
    auto controllerType = controller->Type;
    auto controllerUser = controller->User;

    m_console->Format(L"   Controller ID: %lld, Type: %ls, Paired User: %ls\n", 
        controllerId, 
        controllerType->Data(),
        GetGamertag(controllerUser).c_str());
}

// Write information about all signed in users to the sample console window.
void Sample::WriteUsers()
{
    m_console->WriteLine(L"-- Users --");

    auto users = User::Users;
    if (users->Size == 0)
    {
        m_console->WriteLine(L"   <none>");
        return;
    }

    for (auto&& user : users)
    {
        WriteUser(user);
    }
}

// Write information about the user passed in, their paired controllers, and their audio devices to the sample console window.
void Sample::WriteUser(User^ user)
{
    if (!user)
    {
        return;
    }

    if (user->IsGuest)
    {
        if (user->IsSignedIn)
        {
            m_console->Format(L"   Guest User: %ls (XUID: %ls) sponsored by user %ls\n", GetGamertag(user).c_str(), user->XboxUserId->Data(), GetGamertag(user->Sponsor).c_str());
        }
        else
        {
            m_console->Format(L"   Guest User: %ls (XUID: %ls) is signed out\n", GetGamertag(user).c_str(), user->XboxUserId->Data());
        }
    }
    else
    {
        if (user->IsSignedIn)
        {
            m_console->Format(L"   User: %ls (XUID: %ls) is signed in\n", GetGamertag(user).c_str(), user->XboxUserId->Data());
        }
        else
        {
            m_console->Format(L"   User: %ls (XUID: %ls) is signed out\n", GetGamertag(user).c_str(), user->XboxUserId->Data());
        }
    }

    // Write information about all paired controllers.
    auto controllers = user->Controllers;
    if (controllers->Size == 0)
    {
        m_console->WriteLine(L"     Has no paired controller");
    }
    else
    {
        for (auto controller : controllers)
        {
            m_console->Format(L"     Controller ID: %lld, Type: %ls\n", controller->Id, controller->Type->Data());
        }
    }

    // Write information about user's audio devices.
    auto audioDevices = user->AudioDevices;
    for (auto audioDevice : audioDevices)
    {
        wchar_t idInfo[256] = { L'\0' };
        const wchar_t *preContext, *postContext;

        // Strip the 128 bit GUID's from the render target ID's to get some more human readable information from them.
        // Id's take the form of "{GUID}!DeviceInfo{GUID}\MoreDeviceInfo".

        // First copy the text between the two GUID's.
        preContext = wcschr(audioDevice->Id->Data(), L'!');
        postContext = wcschr(preContext, L'#');
        wcsncpy_s(idInfo, preContext + 1, wcslen(preContext + 1) - wcslen(postContext));

        // Now concatenate the text after the second GUID.
        preContext = wcsrchr(audioDevice->Id->Data(), L'}');
        wcscat_s(idInfo, preContext + 1);

        m_console->Format(L"     Audio Device Category/Type: %ls/%ls (%ls)\n", 
            audioDevice->DeviceCategory.ToString()->Data(), 
            audioDevice->DeviceType.ToString()->Data(), 
            idInfo);
    }
}

#pragma endregion
