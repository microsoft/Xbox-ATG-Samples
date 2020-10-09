//--------------------------------------------------------------------------------------
// WheelTest.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"

#include "WheelTest.h"

#include "ATGColors.h"

extern void ExitSample() noexcept;

using namespace DirectX;

using Microsoft::WRL::ComPtr;

// Sample Globals

namespace
{
    //ID for button functions
    unsigned int ID_CREATEDESTROY_BASE = 0;
    unsigned int ID_STARTSTOP_BASE = 100;
    unsigned int ID_PAUSECONTINUE_BASE = 200;
    unsigned int ID_REQUEST_VENDOR_REPORT = 500;
    unsigned int ID_DUMP_VENDOR_REPORT = 501;

    Platform::String^ ToPlatform(const wstring& str)
    {
        return ref new Platform::String(str.c_str());
    }

    // ForceList is a helper class for loading and controlling force feedback.
    // It provides the following functionality:
    // -Load force feedback assembly from a txt file
    // -Store feedback states, angles, and assembly language
    // -Handle communication with the wheel for feedback control
    class ForceList
    {
    public:
        enum class State
        {
            eUnloaded,
            eDownloaded,
            eStarted,
            ePaused
        };

    public:
        static const uint8_t FL_NO_ERROR = 0x00;
        static const uint8_t FILE_OPEN_ERROR = 0xFF;
        static const uint8_t MISSING_COLON_ERROR = 0xFE;
        static const uint8_t ERROR_MASK = 0x80;

    public:
        ForceList()
        {
            angle_ = 0;
        }

        ~ForceList()
        {
        }

        uint8_t LoadFromFile(const wstring& fName)
        {
            bool inBracket = false;
            bool inAngleBracket = false;
            bool pastColon = false;
            wstring angleString;
            uint8_t currentIndex = 0;
            HANDLE hfile = ::CreateFile2(fName.c_str(), FILE_GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr);
            if ((hfile == INVALID_HANDLE_VALUE) && (hfile == nullptr))
            {
                return FILE_OPEN_ERROR;
            }

            for (;;)
            {
                wchar_t buffer[128] = {};
                unsigned int bytesRead = 0;
                unsigned int bufferPtr = 0;
                BOOL readResult = ReadFile(hfile, buffer, sizeof(buffer) / sizeof(wchar_t), reinterpret_cast<LPDWORD>(&bytesRead), nullptr);
                if ((readResult == false) || (bytesRead == 0))
                {
                    break;
                }

                while (bufferPtr < bytesRead)
                {
                    if (buffer[bufferPtr] == L'\0')
                    {
                        ++bufferPtr;
                    }
                    else if (!inBracket)
                    {
                        if (!inAngleBracket)
                        {
                            if (buffer[bufferPtr] == L'[')
                            {
                                inBracket = true;
                                pastColon = false;
                            }
                            else if (buffer[bufferPtr] == L'<')
                            {
                                inAngleBracket = true;
                            }
                        }
                        else if (buffer[bufferPtr] == L'>')
                        {
                            inAngleBracket = false;
                            angle_ = static_cast<uint16_t>(_wtoi(angleString.c_str()));
                        }
                        else
                        {
                            angleString += buffer[bufferPtr];
                        }
                        ++bufferPtr;
                    }
                    else if (!pastColon)
                    {
                        if (buffer[bufferPtr] == L':')
                        {
                            ++bufferPtr;
                            pastColon = true;
                        }
                        if (buffer[bufferPtr] == L']')
                        {
                            CloseHandle(hfile);
                            return MISSING_COLON_ERROR;
                        }
                        else
                        {
                            names_[currentIndex] += buffer[bufferPtr];
                            ++bufferPtr;
                        }
                    }
                    else // In assembler;
                    {
                        if (buffer[bufferPtr] == ']')
                        {
                            ++bufferPtr;
                            inBracket = false;
                            ++currentIndex;
                        }
                        else
                        {
                            assemblies_[currentIndex].append(1, wchar_t(buffer[bufferPtr]));
                            ++bufferPtr;
                        }
                    }

                }
            }

            ::CloseHandle(hfile);
            count_ = currentIndex;
            return FL_NO_ERROR;
        }

        uint16_t GetAngle()
        {
            return angle_;
        }

        const wstring& GetName(uint8_t index)
        {
            return names_[index];
        }

        const wstring& GetAssembly(uint8_t index)
        {
            return assemblies_[index];
        }

        uint8_t GetCount() const { return count_; }

        void Download(uint8_t index, IWheel^ wheel)
        {
            if (states_[index] == State::eUnloaded)
            {
                (void)wheel->CreateEquationOnDevice(index, ToPlatform(assemblies_[index]), EquationFlags::Normal);
                states_[index] = State::eDownloaded;
            }
        }

        void Start(uint8_t index, IWheel^ wheel)
        {
            if (states_[index] != State::eUnloaded)
            {
                wheel->StartEquation(index);
                wheel->UpdateEquationsToDevice();
                states_[index] = State::eStarted;
            }
        }

        void Stop(uint8_t index, IWheel^ wheel)
        {
            if (states_[index] != State::eUnloaded)
            {
                wheel->StopEquation(index);
                wheel->UpdateEquationsToDevice();
                states_[index] = State::eDownloaded;
            }
        }

        void Pause(uint8_t index, IWheel^ wheel)
        {
            if (states_[index] != State::eUnloaded)
            {
                wheel->PauseEquation(index);
                wheel->UpdateEquationsToDevice();
                states_[index] = State::ePaused;
            }
        }

        void Continue(uint8_t index, IWheel^ wheel)
        {
            if (states_[index] != State::eUnloaded)
            {
                wheel->ContinueEquation(index);
                wheel->UpdateEquationsToDevice();
                states_[index] = State::eStarted;
            }
        }

        void Destroy(uint8_t index, IWheel^ wheel)
        {
            wheel->ClearEquation(index);
            wheel->UpdateEquationsToDevice();
            states_[index] = State::eUnloaded;
        }

        ForceList::State GetState(uint8_t index) const
        {
            return states_[index];
        }

    private:
        uint8_t count_;
        std::array<wstring, 16> names_;
        std::array<wstring, 16> assemblies_;
        std::array<State, 16> states_;

        uint16_t angle_;
    };//ForceList

    ForceList forceList_;
    uint8_t ForceListError_ = ForceList::FL_NO_ERROR;

    // Wheel list management
    IWheel^ wheel_;
    INavigationController^ navController_;

    void OnApplicationMemoryDump(IWheel^ /*pIWheel*/, unsigned short /*addr*/, const Platform::Array<unsigned int>^ /*vals*/)
    {
        //Ignore the data
    }

    void OnControllerAdded_(IController^ pIController)
    {
        if (pIController->Type == "Microsoft.Xbox.Input.Wheel")
        {
            //Setup wheel
            IWheel^ pIWheel = dynamic_cast<IWheel^>(pIController);
            wheel_ = pIWheel;
            navController_ = (INavigationController^)wheel_;
            wheel_->ApplicationMemoryGet += ref new ApplicationMemoryGetEvent(&OnApplicationMemoryDump);

            // Some wheels start up with a force in place already. This will effectively stop that force. 
            wheel_->CreateEquationOnDevice(0, "NOP", EquationFlags::Normal);
            wheel_->ResetDevice();
            if (forceList_.GetAngle() != 0)
            {
                wheel_->SetAngle(forceList_.GetAngle());
            }
        }
    }

    void OnControllerAdded(Platform::Object^ /*sender*/, ControllerAddedEventArgs^ args)
    {
        OnControllerAdded_(args->Controller);
    }

    void OnControllerRemoved(Platform::Object^ /*sender*/, ControllerRemovedEventArgs^ args)
    {
        IController^ pIController = args->Controller;
        if (pIController->Type == "Microsoft.Xbox.Input.Wheel")
        {
            IWheel^ pIWheel = (IWheel^)(pIController);
            if (pIWheel == wheel_)
            {
                wheel_ = nullptr;
                navController_ = nullptr;
            }
        }
    }
}

Sample::Sample() :
    m_frame(0),
    m_pButtons(nullptr),
    m_vendorDebugReport(nullptr)
{
    // Renders only 2D, so no need for a depth buffer.
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN);
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(IUnknown* window)
{
    wheel_ = nullptr;
    navController_ = nullptr;
    m_reading = nullptr;

    m_gamePad = std::make_unique<GamePad>();

    m_deviceResources->SetWindow(window);

    m_deviceResources->CreateDeviceResources();  
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    // Load forces from text file
    ForceListError_ = forceList_.LoadFromFile(L"Forces.txt");
    if (ForceListError_ != ForceList::FL_NO_ERROR)
    {
        // Bail out, error will be shown on screen in render loop
        return;
    }

    // Wheel List and management
    Controller::ControllerAdded += ref new Windows::Foundation::EventHandler<ControllerAddedEventArgs^>(OnControllerAdded);
    Controller::ControllerRemoved += ref new Windows::Foundation::EventHandler<ControllerRemovedEventArgs^>(OnControllerRemoved);

    //Cycle through all connected controllers and add any connected wheels
    IVectorView<IController^>^ controllers = Controller::Controllers;
    for (uint8_t i = 0; i < controllers->Size; ++i)
    {
        OnControllerAdded_(controllers->GetAt(i));
    }
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
void Sample::Update(DX::StepTimer const& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    float elapsedTime = float(timer.GetElapsedSeconds());
    elapsedTime;

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

    if (navController_ != nullptr)
    {
        m_reading = navController_->GetNavigationReading();

        DoWork();
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

    wstring errorTxt;

    //Check for errors with the force list
    if (forceList_.GetCount() == 0)
    {
        if (ForceListError_ == ForceList::FL_NO_ERROR)
        {
            errorTxt = L"No Forces Found in forces.txt file";
        }
        else if (ForceListError_ == ForceList::FILE_OPEN_ERROR)
        {
            errorTxt = L"Unable to open forces.txt file";
        }
        else if (ForceListError_ == ForceList::MISSING_COLON_ERROR)
        {
            errorTxt = L"Format of forces.txt file was invalid";
        }
        else
        {
            errorTxt = L"Unknown error parsing forces.txt file";
        }
    }

    //Ensure we have a valid wheel connected
    if (wheel_ == nullptr)
    {
        if (errorTxt.length() != 0)
        {
            errorTxt += L'\n';
        }
        errorTxt += L"No wheel Connnected";
    }
    else if (navController_ == nullptr)
    {
        if (errorTxt.length() != 0)
        {
            errorTxt += L'\n';
        }
        errorTxt += L"Wheel could not be cast to navigation controller";
    }

    //Display any error
    if (errorTxt.length() != 0)
    {
        RECT rc = m_deviceResources->GetOutputSize();
        m_font->DrawString(m_spriteBatch.get(), errorTxt.c_str(), XMFLOAT2(rc.right / 2.f, rc.bottom / 2.f), ATG::Colors::White);
    }
    else
    {
        //We have a valid wheel connected, so update UI
        DrawVersion();
        m_pButtons->DrawButtons();
        DrawReadings();
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

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device, m_deviceResources->GetBackBufferCount());

    m_spriteBatch = std::make_unique<SpriteBatch>(m_deviceResources->GetD3DDeviceContext());

    m_font = std::make_unique<SpriteFont>(device, L"SegoeUI_24.spritefont");
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    auto vp = m_deviceResources->GetScreenViewport();
    m_spriteBatch->SetViewport(vp);
}
#pragma endregion

void Sample::OnButton(ButtonData * pActiveButton)
{
    // Ignore if we don't have a wheel
    if (wheel_ == nullptr)
    {
        return;
    }

    if (!pActiveButton || !pActiveButton->fValid)
    {
        return;
    }

    //Send a request to get the vendor report
    if (pActiveButton->id == ID_REQUEST_VENDOR_REPORT)
    {
        auto req = ref new Platform::Array<uint8_t>(60);
        req[0] = 0;
        req[1] = 0;
        req[2] = 0;
        wheel_->SendVendorDebugRequest(req);
        return;
    }

    //Dump the vendor report
    if (pActiveButton->id == ID_DUMP_VENDOR_REPORT)
    {
        wheel_->GetVendorDebugReport(0, m_vendorDebugReport);
        return;
    }

    //Send or remove feedback code to the wheel
    uint8_t id = static_cast<uint8_t>(pActiveButton->id);
    if (id < ID_STARTSTOP_BASE)
    {
        auto force_id = static_cast<uint8_t>(id - ID_CREATEDESTROY_BASE);
        wstring buttonText = forceList_.GetName(force_id);
        if (forceList_.GetState(force_id) == ForceList::State::eUnloaded)
        {
            forceList_.Download(force_id, wheel_);
            buttonText += L" Destroy";
        }
        else
        {
            forceList_.Destroy(force_id, wheel_);
            buttonText += L" Create";
        }
        m_pButtons->ChangeButtonText(id, buttonText);
    }
    //Start or stop feedback code
    else if (id < ID_PAUSECONTINUE_BASE)
    {
        auto force_id = static_cast<uint8_t>(id - ID_STARTSTOP_BASE);
        wstring buttonText = forceList_.GetName(force_id);
        if (forceList_.GetState(force_id) == ForceList::State::eStarted)
        {
            forceList_.Stop(force_id, wheel_);
            buttonText += L" Start";
        }
        else
        {
            forceList_.Start(force_id, wheel_);
            buttonText += L" Stop";
        }
        m_pButtons->ChangeButtonText(id, buttonText);
    }
    else
    {
        auto force_id = static_cast<uint8_t>(id - ID_PAUSECONTINUE_BASE);
        wstring buttonText = forceList_.GetName(force_id);
        if (forceList_.GetState(force_id) == ForceList::State::ePaused)
        {
            forceList_.Continue(force_id, wheel_);
            buttonText += L" Pause";
        }
        else
        {
            forceList_.Pause(force_id, wheel_);
            buttonText += L" Continue";
        }
        m_pButtons->ChangeButtonText(id, buttonText);
    }
}

//--------------------------------------------------------------------------------------
// Name: DoWork()
// Desc: Renders the page for displaying the current input
//--------------------------------------------------------------------------------------
void Sample::DoWork()
{
    // Make sure the buttons are active
    if (!m_pButtons->m_fIsInitialized)
    {
        auto f = [this](ButtonData* b) { Sample::OnButton(b); };
        m_pButtons = new ButtonGrid(12, 6, f, m_deviceResources->GetD3DDevice(), m_deviceResources->GetD3DDeviceContext(), m_deviceResources->GetOutputSize());

        m_vendorDebugReport = ref new Platform::Array<uint8_t>(60);
        m_pButtons->SetButton(ID_REQUEST_VENDOR_REPORT, 1, 0, L"Request Debug Report           ");
        m_pButtons->SetButton(ID_DUMP_VENDOR_REPORT, 1, 1, L"Update Debug Report");

        uint8_t row = 2;
        uint8_t col = 0;
        uint8_t id = 0;
        for (uint8_t i = 0; i < forceList_.GetCount(); ++i)
        {
            std::wstring btnText = forceList_.GetName(i);
            btnText += L" Create";
            m_pButtons->SetButton(id + ID_CREATEDESTROY_BASE, row, col++, btnText);
            if (col == 6)
            {
                col = 0;
                ++row;
                if (row >= 5)
                {
                    break;
                }
            }
            btnText = forceList_.GetName(i);
            btnText += L" Start";
            m_pButtons->SetButton(id + ID_STARTSTOP_BASE, row, col++, btnText);
            if (col == 6)
            {
                col = 0;
                ++row;
                if (row >= 5)
                {
                    break;
                }
            }
            btnText = forceList_.GetName(i);
            btnText += L" Pause";
            m_pButtons->SetButton(id + ID_PAUSECONTINUE_BASE, row, col++, btnText);
            if (col == 6)
            {
                col = 0;
                ++row;
                if (row >= 5)
                {
                    break;
                }
            }

            ++id;

        }
    }

    //Now check for input updates on the wheel
    m_pButtons->OnWheelData(m_reading);
}

//Setup all wheel inputs
struct
{
    Platform::String^ name;
    function<float(IWheelReading^)> getValue;
} wheelReadings[] =
{
    { L"Throttle", [](IWheelReading^ r) { return r->Throttle; } },
    { L"Brake", [](IWheelReading^ r) { return r->Brake; } },
    { L"Clutch", [](IWheelReading^ r) { return r->Clutch; } },
    { L"Handbrake", [](IWheelReading^ r) { return r->Handbrake; } },
    { L"RotationAngle", [](IWheelReading^ r) { return r->RotationAngle; } },
};

//Setup all navigation inputs
struct
{
    Platform::String^ name;
    function<bool(Windows::Xbox::Input::INavigationReading^)> getValue;
} navigationReadings[] =
{
    { L"IsAcceptPressed", [](Windows::Xbox::Input::INavigationReading^ r) { return r->IsAcceptPressed; } },
    { L"IsCancelPressed", [](Windows::Xbox::Input::INavigationReading^ r) { return r->IsCancelPressed; } },
    { L"IsDownPressed", [](Windows::Xbox::Input::INavigationReading^ r) { return r->IsDownPressed; } },
    { L"IsLeftPressed", [](Windows::Xbox::Input::INavigationReading^ r) { return r->IsLeftPressed; } },
    { L"IsMenuPressed", [](Windows::Xbox::Input::INavigationReading^ r) { return r->IsMenuPressed; } },
    { L"IsNextPagePressed", [](Windows::Xbox::Input::INavigationReading^ r) { return r->IsNextPagePressed; } },
    { L"IsPreviousPagePressed", [](Windows::Xbox::Input::INavigationReading^ r) { return r->IsPreviousPagePressed; } },
    { L"IsRightPressed", [](Windows::Xbox::Input::INavigationReading^ r) { return r->IsRightPressed; } },
    { L"IsUpPressed", [](Windows::Xbox::Input::INavigationReading^ r) { return r->IsUpPressed; } },
    { L"IsViewPressed", [](Windows::Xbox::Input::INavigationReading^ r) { return r->IsViewPressed; } },
    { L"IsXPressed", [](Windows::Xbox::Input::INavigationReading^ r) { return r->IsXPressed; } },
    { L"IsYPressed", [](Windows::Xbox::Input::INavigationReading^ r) { return r->IsYPressed; } },
};


//--------------------------------------------------------------------------------------
// Name: DrawText()
// Desc: Standard drawtext with preset alignment and color
//--------------------------------------------------------------------------------------
void Sample::DrawText(float sx, float sy, const wchar_t* value)
{
    m_font->DrawString(m_spriteBatch.get(), value, XMFLOAT2(sx, sy), ATG::Colors::White);
}

//--------------------------------------------------------------------------------------
// Name: DrawVersion()
// Desc: Draws wheel DLL and HW version to the screen
//--------------------------------------------------------------------------------------
void Sample::DrawVersion()
{
    if (wheel_ == nullptr)
    {
        return;
    }

    wchar_t buffer[512];

    // Draw Version
    if (wheel_->DllVersionIsDebug)
    {
        swprintf_s(buffer, L"DLL Version: %d.%d:%d (Debug)",
            wheel_->DllVersionMajor, wheel_->DllVersionMinor,
            wheel_->DllVersionBuild);
    }
    else
    {
        swprintf_s(buffer, L"DLL Version: %d.%d:%d (Release)\r\n",
            wheel_->DllVersionMajor,
            wheel_->DllVersionMinor, wheel_->DllVersionBuild);
    }
    DrawText(0, 0, buffer);

    swprintf_s(buffer, L"Hardware: VID_0x%04X PID_0x%04X MAJ_0x%02X MIN_0x%02X",
        static_cast<uint16_t>(wheel_->DeviceVidPid >> 16),
        static_cast<uint16_t>(wheel_->DeviceVidPid & 0xFFFF),
        wheel_->HardwareVersionMajor,
        wheel_->HardwareVersionMinor);
    DrawText(0, (25.0 * 0.8), buffer);
}


//--------------------------------------------------------------------------------------
// Name: DrawReadings()
// Desc: Draws all wheel readings to the screen
//--------------------------------------------------------------------------------------
void Sample::DrawReadings()
{
    if (wheel_ == nullptr)
    {
        return;
    }

    IWheelReading^ wr = wheel_->GetCurrentReading();
    if (!wr)
    {
        return;
    }

    float sx = 0;
    float const lineHeight = 25.0f;
    float const indent = 25.0f;
    float const itemSize = 225.0f;
    unsigned int const lines =
        /* header */ 1 + 10
        + /* spacing */ 1
        + /* header */ 1 + _countof(navigationReadings)
        + /* bottom margin */ 1;
    float sy = m_deviceResources->GetOutputSize().bottom - lines * lineHeight;

    wchar_t buffer[256] = {};

    DrawText(sx, sy, L"WHEEL SPECIFIC VALUES");
    sy += lineHeight;

    DrawText(sx + indent, sy, L"Angle");
    swprintf_s(buffer, L"%f MAX(%d)", wr->RotationAngle, wr->RotationAngleMaximum);
    DrawText(sx + indent + itemSize, sy, buffer);
    sy += lineHeight;

    DrawText(sx + indent, sy, L"Throttle");
    if (wr->ThrottleConnected) {
        swprintf_s(buffer, L"%f", wr->Throttle);
    }
    else {
        swprintf_s(buffer, L"Disconnected");
    }
    DrawText(sx + indent + itemSize, sy, buffer);
    sy += lineHeight;

    DrawText(sx + indent, sy, L"Brake");
    if (wr->BrakeConnected) {
        swprintf_s(buffer, L"%f", wr->Brake);
    }
    else {
        swprintf_s(buffer, L"Disconnected");
    }
    DrawText(sx + indent + itemSize, sy, buffer);
    sy += lineHeight;

    DrawText(sx + indent, sy, L"Clutch");
    if (wr->ClutchConnected) {
        swprintf_s(buffer, L"%f", wr->Clutch);
    }
    else {
        swprintf_s(buffer, L"Disconnected");
    }
    DrawText(sx + indent + itemSize, sy, buffer);
    sy += lineHeight;

    DrawText(sx + indent, sy, L"Handbrake");
    if (wr->HandbrakeConnected) {
        swprintf_s(buffer, L"%f", wr->Handbrake);
    }
    else {
        swprintf_s(buffer, L"Disconnected");
    }
    DrawText(sx + indent + itemSize, sy, buffer);
    sy += lineHeight;

    DrawText(sx + indent, sy, L"Powerlevel");
    swprintf_s(buffer, L"%f", wr->PowerLevel);
    DrawText(sx + indent + itemSize, sy, buffer);
    sy += lineHeight;

    DrawText(sx + indent, sy, L"IsPowered");
    if (wr->IsPowered) {
        DrawText(sx + indent + itemSize, sy, L"YES");
    }
    else {
        DrawText(sx + indent + itemSize, sy, L"NO");
    }
    sy += 2 * lineHeight;

    DrawText(sx, sy, L"NAVIGATION VALUES");
    sy += lineHeight;

    auto nc = (Windows::Xbox::Input::INavigationController^)wheel_;
    Windows::Xbox::Input::INavigationReading^ nr = nc->GetNavigationReading();
    if (nr)
    {
        for (size_t i = 0; i < _countof(navigationReadings); i++)
        {
            swprintf_s(buffer, L"%s = %d", navigationReadings[i].name->Data(), navigationReadings[i].getValue(nr));
            DrawText(sx + indent, sy, buffer);
            sy += lineHeight;
        }
    }
    else
    {
        DrawText(sx + indent, sy, L"No INavigationReading");
        sy += lineHeight;
    }
}
