//--------------------------------------------------------------------------------------
// Main.cpp
//
// Entry point for Xbox One exclusive title.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimplePLM.h"

#include <ppltasks.h>

#include "Telemetry.h"

using namespace concurrency;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::UI::Core;
using namespace Windows::Foundation;
using namespace DirectX;

ref class ViewProvider sealed : public IFrameworkView
{
public:
    ViewProvider() :
        m_exit(false)
    {
    }

    // IFrameworkView methods
    virtual void Initialize(CoreApplicationView^ applicationView)
    {
        applicationView->Activated += ref new
            TypedEventHandler<CoreApplicationView^, IActivatedEventArgs^>(this, &ViewProvider::OnActivated);

        CoreApplication::Suspending +=
            ref new EventHandler<SuspendingEventArgs^>(this, &ViewProvider::OnSuspending);

        CoreApplication::Resuming +=
            ref new EventHandler<Platform::Object^>(this, &ViewProvider::OnResuming);

        CoreApplication::ResourceAvailabilityChanged +=
            ref new EventHandler<Platform::Object^>(this, &ViewProvider::OnResourceAvailabilityChanged);

        m_sample = std::make_unique<Sample>();
        m_sample->ShowInstructions();
        m_sample->LogPLMEvent(L"Initialize()");

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

    virtual void Uninitialize()
    {
        m_sample.reset();
    }

    virtual void SetWindow(CoreWindow^ window)
    {
        window->Closed +=
            ref new TypedEventHandler<CoreWindow^, CoreWindowEventArgs^>(this, &ViewProvider::OnWindowClosed);

        window->VisibilityChanged +=
            ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>(this, &ViewProvider::OnVisibilityChanged);

        window->Activated +=
            ref new TypedEventHandler<CoreWindow^, WindowActivatedEventArgs^>(this, &ViewProvider::OnWindowActivated);

        // Default window thread to CPU 0
        SetThreadAffinityMask(GetCurrentThread(), 0x1);

        m_sample->Initialize(reinterpret_cast<IUnknown*>(window));
        m_sample->LogPLMEvent(L"SetWindow()");
    }

    virtual void Load(Platform::String^ entryPoint)
    {
        m_sample->LogPLMEvent(L"Load()", entryPoint->Data());
    }

    virtual void Run()
    {
        m_sample->LogPLMEvent(L"Run()");
        while (!m_exit)
        {
            m_sample->Tick();

            CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
        }
    }

protected:
    // Event handlers
    void OnActivated(CoreApplicationView^ applicationView, IActivatedEventArgs^ args)
    {
        if (args->Kind == ActivationKind::Launch)
        {
            auto launchArgs = static_cast<LaunchActivatedEventArgs^>(args);
            Platform::String^ argsData = args->Kind.ToString() + " : " + launchArgs->Arguments;
            m_sample->LogPLMEvent(L"OnActivated()", argsData->Data());
        }
        CoreWindow::GetForCurrentThread()->Activate();
    }

    void OnSuspending(Platform::Object^ sender, SuspendingEventArgs^ args)
    {
        if (m_sample->GetUseDeferral())
        {
            //GetDeferral must be called on the Core Window thread. The deferral can then be passed to other threads.
            auto deferral = args->SuspendingOperation->GetDeferral();

            //The deferral can be used to complete suspending on other threads after the OnSuspending handler has returned. 
            concurrency::create_task([this, deferral]()
            {
                m_sample->LogPLMEvent(L"OnSuspending()", L"using a deferral");
                m_sample->OnSuspending();
                deferral->Complete();
            });
        }
        else
        {
            //Without a deferral, the application will complete suspending as soon as it returns from the OnSuspending handler.
            m_sample->LogPLMEvent(L"OnSuspending()", L"not using a deferral");
            m_sample->OnSuspending();
        }
    }

    void OnResuming(Platform::Object^ sender, Platform::Object^ args)
    {
        m_sample->LogPLMEvent(L"OnResuming()");
        m_sample->OnResuming();
    }

    void OnResourceAvailabilityChanged(Platform::Object^ sender, Platform::Object^ args)
    {
        switch (CoreApplication::ResourceAvailability)
        {
        case ResourceAvailability::FullWithExtendedSystemReserve :
            m_sample->LogPLMEvent(L"OnResourceAvailabilityChanged()", L"FullWithExtendedSystemReserve"); break;
        case ResourceAvailability::Full:
            m_sample->LogPLMEvent(L"OnResourceAvailabilityChanged()", L"Full"); break;
        case ResourceAvailability::Constrained:
            m_sample->LogPLMEvent(L"OnResourceAvailabilityChanged()", L"Constrained"); break;
        default:
            break;
        }
    }

    void OnVisibilityChanged(CoreWindow^ sender, VisibilityChangedEventArgs^ args)
    {
        if (args->Visible)
        {
            m_sample->LogPLMEvent(L"OnVisibilityChanged()", L"Visible");
        }
        else
        {
            m_sample->LogPLMEvent(L"OnVisibilityChanged()", L"Not Visible");
        }
    }

    void OnWindowActivated(CoreWindow^ sender, WindowActivatedEventArgs^ args)
    {
        switch (args->WindowActivationState)
        {
        case CoreWindowActivationState::CodeActivated :
            m_sample->LogPLMEvent(L"OnWindowActivated()", L"CodeActivated"); break;
        case CoreWindowActivationState::PointerActivated :
            m_sample->LogPLMEvent(L"OnWindowActivated()", L"PointerActivated"); break;
        case CoreWindowActivationState::Deactivated :
            m_sample->LogPLMEvent(L"OnWindowActivated()", L"Deactivated"); break;
        default:
            break;
        }
    }

    void OnWindowClosed(CoreWindow^ sender, CoreWindowEventArgs^ args)
    {
        m_exit = true;
    }

private:
    bool                    m_exit;
    std::unique_ptr<Sample> m_sample;
};

ref class ViewProviderFactory : IFrameworkViewSource
{
public:
    virtual IFrameworkView^ CreateView()
    {
        return ref new ViewProvider();
    }
};


// Entry point
[Platform::MTAThread]
int __cdecl main(Platform::Array<Platform::String^>^ /*argv*/)
{
    // Default main thread to CPU 0
    SetThreadAffinityMask(GetCurrentThread(), 0x1);

    auto viewProviderFactory = ref new ViewProviderFactory();
    CoreApplication::Run(viewProviderFactory);
    return 0;
}


// Exit helper
void ExitSample() noexcept
{
    Windows::ApplicationModel::Core::CoreApplication::Exit();
}
