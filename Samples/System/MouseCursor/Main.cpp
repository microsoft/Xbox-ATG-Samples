//--------------------------------------------------------------------------------------
// Main.cpp
//
// Entry point for Universal Windows Platform (UWP) app.
//
// Initialize the mouse cursor object and implements the event handling for
// necessary mouse cursor events to implement clip cursor or relative mouse mode.
// Keyboard handling is also implemented here. 
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "MouseCursor.h"

#include <ppltasks.h>

#include "Telemetry.h"

using namespace concurrency;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::UI::ViewManagement;
using namespace Windows::System;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;
using namespace DirectX;
using namespace DirectX::SimpleMath;


ref class ViewProvider sealed : public IFrameworkView
{
public:
    ViewProvider() :
        m_exit( false ),
        m_visible( true ),
        m_DPI( 96.f ),
        m_logicalWidth( 800.f ),
        m_logicalHeight( 600.f ),
        m_nativeOrientation( DisplayOrientations::None ),
        m_currentOrientation( DisplayOrientations::None )
    {
        m_outputWidthPixels = ConvertDipsToPixels(m_logicalWidth);
        m_outputHeightPixels = ConvertDipsToPixels(m_logicalHeight);
    }

    // IFrameworkView methods
    virtual void Initialize( CoreApplicationView^ applicationView )
    {
        applicationView->Activated +=
            ref new TypedEventHandler<CoreApplicationView^, IActivatedEventArgs^>( this, &ViewProvider::OnActivated );

        CoreApplication::Suspending +=
            ref new EventHandler<SuspendingEventArgs^>( this, &ViewProvider::OnSuspending );

        CoreApplication::Resuming +=
            ref new EventHandler<Platform::Object^>( this, &ViewProvider::OnResuming );
        
        m_sample = std::make_unique<Sample>();

        // Sample Usage Telemetry
        //
        // Disable or remove this code block to opt-out of sample usage telemetry
        //
        if (ATG::EventRegisterATGSampleTelemetry() == ERROR_SUCCESS)
        {
            wchar_t exeName[MAX_PATH + 1] = {};
            if (!GetModuleFileNameW(nullptr, exeName, MAX_PATH))
            {
                wcscpy_s(exeName, L"Unknown");
            }
            wchar_t fname[_MAX_FNAME] = {};
            wchar_t ext[_MAX_EXT] = {};
            (void)_wsplitpath_s(exeName, nullptr, 0, nullptr, 0, fname, _MAX_FNAME, ext, _MAX_EXT);
            (void)_wmakepath_s(exeName, nullptr, nullptr, fname, ext); // keep only the filename + extension

            ATG::EventWriteSampleLoaded(exeName);
        }
    }

    virtual void Uninitialize()
    {
        m_sample.reset();
    }

    virtual void SetWindow( CoreWindow^ window )
    {
        window->SizeChanged +=
            ref new TypedEventHandler<CoreWindow^, WindowSizeChangedEventArgs^>( this, &ViewProvider::OnWindowSizeChanged );

        window->VisibilityChanged +=
            ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>( this, &ViewProvider::OnVisibilityChanged );

        window->Closed +=
            ref new TypedEventHandler<CoreWindow^, CoreWindowEventArgs^>( this, &ViewProvider::OnWindowClosed );

        auto dispatcher = CoreWindow::GetForCurrentThread()->Dispatcher;

        dispatcher->AcceleratorKeyActivated +=
            ref new TypedEventHandler<CoreDispatcher^, AcceleratorKeyEventArgs^>(this, &ViewProvider::OnAcceleratorKeyActivated);
        
        auto currentDisplayInformation = DisplayInformation::GetForCurrentView();

        currentDisplayInformation->DpiChanged +=
            ref new TypedEventHandler<DisplayInformation^, Object^>( this, &ViewProvider::OnDpiChanged );

        currentDisplayInformation->OrientationChanged +=
            ref new TypedEventHandler<DisplayInformation^, Object^>( this, &ViewProvider::OnOrientationChanged );

        DisplayInformation::DisplayContentsInvalidated +=
            ref new TypedEventHandler<DisplayInformation^, Object^>( this, &ViewProvider::OnDisplayContentsInvalidated );

        // Mouse press and move handlers for the uncaptured mouse
        window->PointerPressed += 
            ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow ^, Windows::UI::Core::PointerEventArgs ^>( this, &ViewProvider::OnPointerPressed );
        
        window->PointerMoved += 
            ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow ^, Windows::UI::Core::PointerEventArgs ^>( this, &ViewProvider::OnPointerMoved );
        
        // Handler for mouse movement when the mouse is captured
        Windows::Devices::Input::MouseDevice::GetForCurrentView()->MouseMoved += 
            ref new Windows::Foundation::TypedEventHandler<Windows::Devices::Input::MouseDevice ^, Windows::Devices::Input::MouseEventArgs ^>( this, &ViewProvider::OnMouseMoved );
        
        // Handler for dealing with losing the capture mode 
        window->PointerCaptureLost +=
            ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow ^, Windows::UI::Core::PointerEventArgs ^>( this, &ViewProvider::OnPointerCaptureLost );
        
        window->KeyDown +=
            ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow ^, Windows::UI::Core::KeyEventArgs ^>( this, &ViewProvider::OnKeyDown );

        m_DPI = currentDisplayInformation->LogicalDpi;

        

        m_logicalWidth = window->Bounds.Width;
        m_logicalHeight = window->Bounds.Height;

        m_nativeOrientation = currentDisplayInformation->NativeOrientation;
        m_currentOrientation = currentDisplayInformation->CurrentOrientation;

        m_outputWidthPixels = ConvertDipsToPixels( m_logicalWidth );
        m_outputHeightPixels = ConvertDipsToPixels( m_logicalHeight );

        DXGI_MODE_ROTATION rotation = ComputeDisplayRotation();

        if ( rotation == DXGI_MODE_ROTATION_ROTATE90 || rotation == DXGI_MODE_ROTATION_ROTATE270 )
        {
            std::swap(m_outputWidthPixels, m_outputHeightPixels);
        }

        m_sample->Initialize( reinterpret_cast<IUnknown*>( window ),
                              m_outputWidthPixels, m_outputHeightPixels, rotation );
    }

    virtual void Load( Platform::String^ entryPoint )
    {
    }

    virtual void Run()
    {
        while ( !m_exit )
        {
            if ( m_visible )
            {
                m_sample->Tick();

                CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents( CoreProcessEventsOption::ProcessAllIfPresent );
            }
            else
            {
                CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents( CoreProcessEventsOption::ProcessOneAndAllPending );
            }
        }
    }

protected:
    // Event handlers
    void OnActivated( CoreApplicationView^ applicationView, IActivatedEventArgs^ args )
    {
        if (args->Kind == ActivationKind::Launch)
        {
            auto launchArgs = static_cast<LaunchActivatedEventArgs^>(args);

            if (launchArgs->PrelaunchActivated)
            {
                // Opt-out of Prelaunch
                CoreApplication::Exit();
                return;
            }
        }

        int w, h;
        m_sample->GetDefaultSize( w, h );

        m_DPI = DisplayInformation::GetForCurrentView()->LogicalDpi;

        ApplicationView::PreferredLaunchWindowingMode = ApplicationViewWindowingMode::PreferredLaunchViewSize;
        // Change to ApplicationViewWindowingMode::FullScreen to default to full screen

        auto desiredSize = Size( ConvertPixelsToDips( w ),
                                 ConvertPixelsToDips( h ) ); 

        ApplicationView::PreferredLaunchViewSize = desiredSize;

        auto view = ApplicationView::GetForCurrentView();

        auto minSize = Size( ConvertPixelsToDips( 320 ),
                             ConvertPixelsToDips( 200 ) );

        view->SetPreferredMinSize( minSize );

        CoreWindow::GetForCurrentThread()->Activate();

        view->FullScreenSystemOverlayMode = FullScreenSystemOverlayMode::Standard;

        CoreApplication::GetCurrentView()->TitleBar->ExtendViewIntoTitleBar = true;

        view->TryResizeView(desiredSize);
    }

    void OnSuspending( Platform::Object^ sender, SuspendingEventArgs^ args )
    {
        SuspendingDeferral^ deferral = args->SuspendingOperation->GetDeferral();

        create_task( [ this, deferral ] ()
        {
            m_sample->OnSuspending();

            deferral->Complete();
        } );
    }

    void OnResuming( Platform::Object^ sender, Platform::Object^ args )
    {
        m_sample->OnResuming();
    }

    void OnWindowSizeChanged( CoreWindow^ sender, WindowSizeChangedEventArgs^ args )
    {
        float prevWidth = (float) m_outputWidthPixels;
        float prevHeight = (float) m_outputHeightPixels;

        m_logicalWidth = sender->Bounds.Width;
        m_logicalHeight = sender->Bounds.Height;

        m_outputWidthPixels = ConvertDipsToPixels(m_logicalWidth);
        m_outputHeightPixels = ConvertDipsToPixels(m_logicalHeight);

        // Adjust drawn cursor location based on size change
        if (m_relative)
        {
            m_virtualCursorOnscreenPosition.X = m_outputWidthPixels / 2.0f;
            m_virtualCursorOnscreenPosition.Y = m_outputHeightPixels / 2.0f;
        }
        else if (m_clipCursor)
        {
            m_virtualCursorOnscreenPosition.X = m_virtualCursorOnscreenPosition.X * (m_outputWidthPixels / prevWidth);
            m_virtualCursorOnscreenPosition.Y = m_virtualCursorOnscreenPosition.Y * (m_outputHeightPixels / prevHeight);
            m_sample->UpdatePointer(m_virtualCursorOnscreenPosition);
        }
        HandleWindowSizeChanged();
    }

    void OnVisibilityChanged( CoreWindow^ sender, VisibilityChangedEventArgs^ args )
    {
        m_visible = args->Visible;
        if ( m_visible )
            m_sample->OnActivated();
        else
            m_sample->OnDeactivated();
    }

    void OnWindowClosed( CoreWindow^ sender, CoreWindowEventArgs^ args )
    {
        m_exit = true;
    }

    void OnAcceleratorKeyActivated(CoreDispatcher^, AcceleratorKeyEventArgs^ args)
    {
        if (args->EventType == CoreAcceleratorKeyEventType::SystemKeyDown
            && args->VirtualKey == VirtualKey::Enter
            && args->KeyStatus.IsMenuKeyDown
            && !args->KeyStatus.WasKeyDown)
        {
            // Implements the classic ALT+ENTER fullscreen toggle
            auto view = ApplicationView::GetForCurrentView();

            if (view->IsFullScreenMode)
                view->ExitFullScreenMode();
            else
                view->TryEnterFullScreenMode();

            args->Handled = true;
        }
    }


    void OnDpiChanged( DisplayInformation^ sender, Object^ args )
    {
        m_DPI = sender->LogicalDpi;

        HandleWindowSizeChanged();
    }

    void OnOrientationChanged( DisplayInformation^ sender, Object^ args )
    {
        m_currentOrientation = sender->CurrentOrientation;

        HandleWindowSizeChanged();
    }

    void OnDisplayContentsInvalidated( DisplayInformation^ sender, Object^ args )
    {
        m_sample->ValidateDevice();
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
            float newX = (float) ConvertDipsToPixels( args->CurrentPoint->Position.X );
            float newY = (float) ConvertDipsToPixels( args->CurrentPoint->Position.Y );

            Sample::MouseMode mode = m_sample->SetMode( Windows::Foundation::Point( newX, newY ) );

            // if the user selected relative mode, capture the mouse 
            if ( mode == Sample::RELATIVE_MOUSE )
            {
                m_absolute = false;
                m_relative = true;

                // Set the pointer cursor to null. This causes the windows mouse to disappear so the 
                // app or game's mouse can then be drawn. 
                sender->SetPointerCapture();
                sender->PointerCursor = nullptr;
                
                // Save the pointers position
                m_virtualCursorOnscreenPosition.X = newX;
                m_virtualCursorOnscreenPosition.Y = newY;

                m_virtualCursorOnscreenPosition.X = fmaxf( 0.f, fminf( m_virtualCursorOnscreenPosition.X, (float) m_outputWidthPixels ) );
                m_virtualCursorOnscreenPosition.Y = fmaxf( 0.f, fminf( m_virtualCursorOnscreenPosition.Y, (float) m_outputHeightPixels ) );

            }
            // If the user selected clip cursor mode, capture the mouse
            else if ( mode == Sample::CLIPCURSOR_MOUSE )
            {
                m_absolute = false;
                m_clipCursor = true;

                // Set the pointer cursor to null. This causes the windows mouse to disappear so the 
                // app or game's mouse can then be drawn.
                sender->SetPointerCapture();
                sender->PointerCursor = nullptr;								

                // Save the pointers position
                m_virtualCursorOnscreenPosition.X = newX;
                m_virtualCursorOnscreenPosition.Y = newY;

                m_virtualCursorOnscreenPosition.X = fmaxf( 0.f, fminf( m_virtualCursorOnscreenPosition.X, (float) m_outputWidthPixels ) );
                m_virtualCursorOnscreenPosition.Y = fmaxf( 0.f, fminf( m_virtualCursorOnscreenPosition.Y, (float) m_outputHeightPixels ) );
            }
        }
    }

    // When ESC is pressed, exit clip cursor or relative mode and return to absolute mode
    void OnKeyDown( Windows::UI::Core::CoreWindow ^sender, Windows::UI::Core::KeyEventArgs ^args )
    {
        if ( args->VirtualKey == Windows::System::VirtualKey::Escape && !m_absolute )
        {
            sender->ReleasePointerCapture();
            sender->PointerCursor = ref new CoreCursor( CoreCursorType::Arrow, 0 );
            
            // Use the window location and the virtual cursors location to reposition the windows mouse
            Windows::Foundation::Point newPoint;
            newPoint.X = sender->Bounds.X + ConvertPixelsToDips(int(m_virtualCursorOnscreenPosition.X));
            newPoint.Y = sender->Bounds.Y + ConvertPixelsToDips(int(m_virtualCursorOnscreenPosition.Y));
            sender->PointerPosition = newPoint;
            
            m_clipCursor = false;
            m_relative = false;
            m_absolute = true;
            Windows::Foundation::Point pt( 0,0 );
            m_sample->SetMode( pt );
        }
    }

    // When the pointer capture is lost exit clip cursor or relative mode and return to absolute mode
    void OnPointerCaptureLost( Windows::UI::Core::CoreWindow ^sender, Windows::UI::Core::PointerEventArgs ^args )
    {
        sender->ReleasePointerCapture();
        sender->PointerCursor = ref new CoreCursor( CoreCursorType::Arrow, 0 );

        // Use the window location and the virtual cursors location to reposition the windows mouse
        Windows::Foundation::Point newPoint;
        newPoint.X = sender->Bounds.X + ConvertPixelsToDips(int(m_virtualCursorOnscreenPosition.X));
        newPoint.Y = sender->Bounds.Y + ConvertPixelsToDips(int(m_virtualCursorOnscreenPosition.Y));
        sender->PointerPosition = newPoint;

        m_clipCursor = false;
        m_relative = false;
        m_absolute = true;
        Windows::Foundation::Point pt( 0, 0 );
        m_sample->SetMode( pt );
    }

    // When the mouse moves in absolute mode check to see if it is hovering over a selection box
    void OnPointerMoved( Windows::UI::Core::CoreWindow ^sender, Windows::UI::Core::PointerEventArgs ^args )
    {
        if ( m_absolute )
        {
            float newX = (float) ConvertDipsToPixels( args->CurrentPoint->Position.X );
            float newY = (float) ConvertDipsToPixels( args->CurrentPoint->Position.Y );

            m_sample->CheckLocation( Windows::Foundation::Point( newX, newY ) );

        }
    }

    // When the mouse moves in captured mode update the on screen position for clip cursor or update the camera/target for relative mode
    void OnMouseMoved( Windows::Devices::Input::MouseDevice ^sender, Windows::Devices::Input::MouseEventArgs ^args )
    {
        float newX = (float)ConvertDipsToPixels((float)args->MouseDelta.X);
        float newY = (float)ConvertDipsToPixels((float)args->MouseDelta.Y);

        if ( m_clipCursor )
        {
            m_virtualCursorOnscreenPosition.X = m_virtualCursorOnscreenPosition.X + newX;
            m_virtualCursorOnscreenPosition.Y = m_virtualCursorOnscreenPosition.Y + newY;

            m_virtualCursorOnscreenPosition.X = fmaxf( 0.f, fminf( m_virtualCursorOnscreenPosition.X, (float) m_outputWidthPixels ) );
            m_virtualCursorOnscreenPosition.Y = fmaxf( 0.f, fminf( m_virtualCursorOnscreenPosition.Y, (float) m_outputHeightPixels ) );
            
            
            m_sample->UpdatePointer( m_virtualCursorOnscreenPosition );
        }
        else if ( m_relative )
        {	
            m_sample->UpdateCamera( Vector3( newX, newY, 0.f ) );
        }
    }

private:
    bool                    m_exit;
    bool                    m_visible;
    float                   m_DPI;
    float                   m_logicalWidth;
    float                   m_logicalHeight;
    int                     m_outputHeightPixels;
    int                     m_outputWidthPixels;
    std::unique_ptr<Sample> m_sample;

    // Mode
    bool m_clipCursor = false;
    bool m_relative = false;
    bool m_absolute = true;

    Windows::Foundation::Point m_virtualCursorOnscreenPosition;
    Windows::Graphics::Display::DisplayOrientations	m_nativeOrientation;
    Windows::Graphics::Display::DisplayOrientations	m_currentOrientation;

    inline int ConvertDipsToPixels( float dips ) const
    {
        return int( dips * m_DPI / 96.f + 0.5f );
    }

    inline float ConvertPixelsToDips(int pixels) const
    {
        return (float(pixels) * 96.f / m_DPI);
    }

    DXGI_MODE_ROTATION ComputeDisplayRotation() const
    {
        DXGI_MODE_ROTATION rotation = DXGI_MODE_ROTATION_UNSPECIFIED;

        switch ( m_nativeOrientation )
        {
        case DisplayOrientations::Landscape:
            switch ( m_currentOrientation )
            {
            case DisplayOrientations::Landscape:
                rotation = DXGI_MODE_ROTATION_IDENTITY;
                break;

            case DisplayOrientations::Portrait:
                rotation = DXGI_MODE_ROTATION_ROTATE270;
                break;

            case DisplayOrientations::LandscapeFlipped:
                rotation = DXGI_MODE_ROTATION_ROTATE180;
                break;

            case DisplayOrientations::PortraitFlipped:
                rotation = DXGI_MODE_ROTATION_ROTATE90;
                break;
            }
            break;

        case DisplayOrientations::Portrait:
            switch ( m_currentOrientation )
            {
            case DisplayOrientations::Landscape:
                rotation = DXGI_MODE_ROTATION_ROTATE90;
                break;

            case DisplayOrientations::Portrait:
                rotation = DXGI_MODE_ROTATION_IDENTITY;
                break;

            case DisplayOrientations::LandscapeFlipped:
                rotation = DXGI_MODE_ROTATION_ROTATE270;
                break;

            case DisplayOrientations::PortraitFlipped:
                rotation = DXGI_MODE_ROTATION_ROTATE180;
                break;
            }
            break;
        }

        return rotation;
    }

    void HandleWindowSizeChanged()
    {
        m_outputWidthPixels = ConvertDipsToPixels( m_logicalWidth );
        m_outputHeightPixels = ConvertDipsToPixels( m_logicalHeight );


        DXGI_MODE_ROTATION rotation = ComputeDisplayRotation();

        if ( rotation == DXGI_MODE_ROTATION_ROTATE90 || rotation == DXGI_MODE_ROTATION_ROTATE270 )
        {
            std::swap(m_outputWidthPixels, m_outputHeightPixels);
        }

        m_sample->OnWindowSizeChanged(m_outputWidthPixels, m_outputHeightPixels, rotation );
    }

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
[ Platform::MTAThread ]
int main( Platform::Array<Platform::String^>^ argv )
{
    UNREFERENCED_PARAMETER( argv );

    auto viewProviderFactory = ref new ViewProviderFactory();
    CoreApplication::Run( viewProviderFactory );
    return 0;
}
