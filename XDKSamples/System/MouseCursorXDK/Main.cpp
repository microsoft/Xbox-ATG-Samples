//--------------------------------------------------------------------------------------
// Main.cpp
//
// Entry point for Xbox One exclusive title.
//
// This version has been customized with mouse handling support for this sample.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "MouseCursor.h"
#include "Keyboard.h"

#include "Telemetry.h"

#include <ppltasks.h>

using namespace concurrency;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::UI::ViewManagement;
using namespace Windows::System;
using namespace Windows::Foundation;
using namespace DirectX;
using namespace DirectX::SimpleMath;

bool g_HDRMode = false;

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
        applicationView->Activated +=
            ref new TypedEventHandler<CoreApplicationView^, IActivatedEventArgs^>(this, &ViewProvider::OnActivated);

        CoreApplication::Suspending +=
            ref new EventHandler<SuspendingEventArgs^>(this, &ViewProvider::OnSuspending);

        CoreApplication::Resuming +=
            ref new EventHandler<Platform::Object^>(this, &ViewProvider::OnResuming);

        CoreApplication::DisableKinectGpuReservation = true;

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

    virtual void Uninitialize()
    {
        m_sample.reset();
    }

    virtual void SetWindow(CoreWindow^ window)
    {
        window->Closed +=
            ref new TypedEventHandler<CoreWindow^, CoreWindowEventArgs^>(this, &ViewProvider::OnWindowClosed);

        // Mouse press and move handlers for the uncaptured mouse
        window->PointerPressed += 
            ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow ^, Windows::UI::Core::PointerEventArgs ^>( this, &ViewProvider::OnPointerPressed );
        
        window->PointerMoved += 
            ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow ^, Windows::UI::Core::PointerEventArgs ^>( this, &ViewProvider::OnPointerMoved );
        
        // Handler for mouse movement when the mouse is captured
        Windows::Devices::Input::MouseDevice::GetForCurrentView()->MouseMoved += 
            ref new Windows::Foundation::TypedEventHandler<Windows::Devices::Input::MouseDevice ^, Windows::Devices::Input::MouseEventArgs ^>( this, &ViewProvider::OnMouseMoved );
        
		window->KeyDown +=
			ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow ^, Windows::UI::Core::KeyEventArgs ^>(this, &ViewProvider::OnKeyDown);
		
		// Default window thread to CPU 0
        SetThreadAffinityMask(GetCurrentThread(), 0x1);

        m_sample->Initialize(reinterpret_cast<IUnknown*>(window));
    }

    virtual void Load(Platform::String^ entryPoint)
    {
    }

    virtual void Run()
    {
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
        CoreWindow::GetForCurrentThread()->Activate();
    }

    void OnSuspending(Platform::Object^ sender, SuspendingEventArgs^ args)
    {
        auto deferral = args->SuspendingOperation->GetDeferral();

        create_task([this, deferral]()
        {
            m_sample->OnSuspending();

            deferral->Complete();
        });
    }

    void OnResuming(Platform::Object^ sender, Platform::Object^ args)
    {
        m_sample->OnResuming();
    }

    void OnWindowClosed(CoreWindow^ sender, CoreWindowEventArgs^ args)
    {
        m_exit = true;
    }


    // This handler is for the uncaptured windows mouse. When the mouse is still uncaptured ( in absolute mode )
    // pointer pressed events are handled to check if the user selected either UI option.
    // If a UI tile is selected, the mouse is captured and either relative or clip cursor mode is entered.
    // 
    void OnPointerPressed( Windows::UI::Core::CoreWindow ^sender, Windows::UI::Core::PointerEventArgs ^args )
    {
		if ( m_absolute )
        {		
            // Check which mode the user selected
            float dpi = (m_sample->IsRunning4k()) ? 192.f : 96.f;

            float newX = args->CurrentPoint->Position.X * dpi / 96.f + .5f;
            float newY = args->CurrentPoint->Position.Y * dpi / 96.f + .5f;

            Sample::MouseMode mode = m_sample->SetMode( Windows::Foundation::Point( newX, newY ) );

            // if the user selected relative mode, capture the mouse 
            if ( mode == Sample::RELATIVE_MOUSE )
            {
                m_absolute = false;
                m_relative = true;

                // Set the pointer cursor to null. This causes the windows mouse to disappear so the 
                // app or game's mouse can then be drawn. 
                sender->PointerCursor = nullptr;
                
                // Save the pointers position
                m_virtualCursorOnscreenPosition.X = newX;
                m_virtualCursorOnscreenPosition.Y = newY;

                m_virtualCursorOnscreenPosition.X = fmaxf( 0.f, m_virtualCursorOnscreenPosition.X );
                m_virtualCursorOnscreenPosition.Y = fmaxf( 0.f, m_virtualCursorOnscreenPosition.Y );

            }
            // If the user selected clip cursor mode, capture the mouse
            else if ( mode == Sample::CLIPCURSOR_MOUSE )
            {
                m_absolute = false;
                m_clipCursor = true;

                // Set the pointer cursor to null. This causes the windows mouse to disappear so the 
                // app or game's mouse can then be drawn.
				sender->PointerCursor = nullptr;

                // Save the pointers position
                m_virtualCursorOnscreenPosition.X = newX;
                m_virtualCursorOnscreenPosition.Y = newY;

                m_virtualCursorOnscreenPosition.X = fmaxf( 0.f, m_virtualCursorOnscreenPosition.X );
                m_virtualCursorOnscreenPosition.Y = fmaxf( 0.f, m_virtualCursorOnscreenPosition.Y );
            }
        }
		else if (args->CurrentPoint->Properties->IsRightButtonPressed)
		{
			sender->PointerCursor = ref new CoreCursor(CoreCursorType::Arrow, 0);

			m_clipCursor = false;
			m_relative = false;
			m_absolute = true;
			Windows::Foundation::Point pt(0, 0);
			m_sample->SetMode(pt);
		}
    }

    // When the mouse moves in absolute mode check to see if it is hovering over a selection box
    void OnPointerMoved( Windows::UI::Core::CoreWindow ^sender, Windows::UI::Core::PointerEventArgs ^args )
    {
        if ( m_absolute )
        {
			if (sender->PointerCursor == nullptr)
			{
				sender->PointerCursor = ref new CoreCursor(CoreCursorType::Arrow, 0);
			}
			
			float newX = args->CurrentPoint->Position.X + .5f;
            float newY = args->CurrentPoint->Position.Y + .5f;

            m_sample->CheckLocation( Windows::Foundation::Point( newX, newY ) );
        }
    }

    // When the mouse moves in captured mode update the on screen position for clip cursor or update the camera/target for relative mode
    void OnMouseMoved( Windows::Devices::Input::MouseDevice ^sender, Windows::Devices::Input::MouseEventArgs ^args )
    {
        float newX = (float)args->MouseDelta.X + .5f;
        float newY = (float)args->MouseDelta.Y + .5f;

        if ( m_clipCursor )
        {
            m_virtualCursorOnscreenPosition.X = m_virtualCursorOnscreenPosition.X + newX;
            m_virtualCursorOnscreenPosition.Y = m_virtualCursorOnscreenPosition.Y + newY;

            m_virtualCursorOnscreenPosition.X = fmaxf( 0.f, m_virtualCursorOnscreenPosition.X );
            m_virtualCursorOnscreenPosition.Y = fmaxf( 0.f, m_virtualCursorOnscreenPosition.Y );
            
            m_sample->UpdatePointer( m_virtualCursorOnscreenPosition );
        }
        else if ( m_relative )
        {	
            m_sample->UpdateCamera( Vector3( newX, newY, 0.f ) );
        }
    }

	// When ESC is pressed, exit clip cursor or relative mode and return to absolute mode
	void OnKeyDown(Windows::UI::Core::CoreWindow ^sender, Windows::UI::Core::KeyEventArgs ^args)
	{
		if (args->VirtualKey == Windows::System::VirtualKey::Escape || args->VirtualKey == Windows::System::VirtualKey::Q)
		{
			Windows::ApplicationModel::Core::CoreApplication::Exit();
		}
	}

private:
    bool                    m_exit;
    std::unique_ptr<Sample> m_sample;

    // Mode
    bool m_clipCursor = false;
    bool m_relative = false;
    bool m_absolute = true;

	Windows::Foundation::Point m_virtualCursorOnscreenPosition;
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
void ExitSample()
{
    Windows::ApplicationModel::Core::CoreApplication::Exit();
}