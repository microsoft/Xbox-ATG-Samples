//--------------------------------------------------------------------------------------
// Main.cpp
//
// Entry point for Xbox One exclusive title.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "GamepadCppWinRT.h"

#include "Telemetry.h"

using namespace winrt::Windows::ApplicationModel;
using namespace winrt::Windows::ApplicationModel::Core;
using namespace winrt::Windows::ApplicationModel::Activation;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::Foundation;
using namespace DirectX;

class ViewProvider : public winrt::implements<ViewProvider, IFrameworkView>
{
public:
    ViewProvider() :
        m_exit(false)
    {
    }

    // IFrameworkView methods
    void Initialize(CoreApplicationView const & applicationView)
    {
        applicationView.Activated({ this, &ViewProvider::OnActivated });

        CoreApplication::Suspending({ this, &ViewProvider::OnSuspending });

        CoreApplication::Resuming({ this, &ViewProvider::OnResuming });

        m_sample = std::make_unique<Sample>();

        // Sample Usage Telemetry
        //
        // Disable or remove this code block to opt-out of sample usage telemetry
        //
        if (EventRegisterATGSampleTelemetry() == ERROR_SUCCESS)
        {
            wchar_t exePath[MAX_PATH + 1] = {};
            if (!GetModuleFileNameW(nullptr, exePath, MAX_PATH))
            {
                wcscpy_s(exePath, L"Unknown");
            }
            EventWriteSampleLoaded(exePath);
        }
    }

    void Uninitialize()
    {
        m_sample.reset();
    }

    void SetWindow(CoreWindow const & window)
    {
        window.Closed({ this, &ViewProvider::OnWindowClosed });

        // Default window thread to CPU 0
        SetThreadAffinityMask(GetCurrentThread(), 0x1);

        auto windowPtr = static_cast<::IUnknown*>(winrt::get_abi(window));
        m_sample->Initialize(windowPtr);
    }

    void Load(winrt::hstring const &)
    {
    }

    void Run()
    {
        while (!m_exit)
        {
            m_sample->Tick();

            CoreWindow::GetForCurrentThread().Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
        }
    }

protected:
    // Event handlers
    void OnActivated(CoreApplicationView const & /*applicationView*/, IActivatedEventArgs const & /*args*/)
    {
        CoreWindow::GetForCurrentThread().Activate();
    }

    void OnSuspending(IInspectable const & /*sender*/, SuspendingEventArgs const & args)
    {
        auto deferral = args.SuspendingOperation().GetDeferral();

        auto f = std::async(std::launch::async, [this, deferral]()
        {
            m_sample->OnSuspending();

            deferral.Complete();
        });
    }

    void OnResuming(IInspectable const & /*sender*/, IInspectable const & /*args*/)
    {
        m_sample->OnResuming();
    }

    void OnWindowClosed(CoreWindow const & /*sender*/, CoreWindowEventArgs const & /*args*/)
    {
        m_exit = true;
    }

private:
    bool                    m_exit;
    std::unique_ptr<Sample> m_sample;
};

class ViewProviderFactory : public winrt::implements<ViewProviderFactory, IFrameworkViewSource>
{
public:
    IFrameworkView CreateView()
    {
        return winrt::make<ViewProvider>();
    }
};

// Entry point
int WINAPIV WinMain()
{
    winrt::init_apartment();

    // Default main thread to CPU 0
    SetThreadAffinityMask(GetCurrentThread(), 0x1);

    auto viewProviderFactory = winrt::make<ViewProviderFactory>();
    CoreApplication::Run(viewProviderFactory);

    winrt::uninit_apartment();
    return 0;
}

// Exit helper
void ExitSample()
{
    winrt::Windows::ApplicationModel::Core::CoreApplication::Exit();
}
