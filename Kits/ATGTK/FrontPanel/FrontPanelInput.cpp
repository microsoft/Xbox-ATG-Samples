//--------------------------------------------------------------------------------------
// FrontPanelInput.cpp
//
// Microsoft Xbox One XDK
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "pch.h"

#include "FrontPanelInput.h"

#include <XboxFrontPanel.h>

using namespace ATG;

using Microsoft::WRL::ComPtr;

// --------------------------------------------------------------------------------
// FrontPanelInput::Impl definition
// --------------------------------------------------------------------------------
#pragma region FrontPanelInput::Impl definition
class FrontPanelInput::Impl
{
public:
    Impl(FrontPanelInput *owner, _In_ IXboxFrontPanelControl *frontPanelControl)
        : mOwner(owner)
        , mFrontPanelControl(frontPanelControl)
    {
        if (s_frontPanelInput)
        {
            throw std::exception("FrontPanelInput is a singleton");
        }

        s_frontPanelInput = this;
    }

    ComPtr<IXboxFrontPanelControl> mFrontPanelControl;
    FrontPanelInput *mOwner;
    static FrontPanelInput::Impl *s_frontPanelInput;

    void GetState(State &state)
    {
        XBOX_FRONT_PANEL_BUTTONS buttonReading = {};
        if (IsAvailable())
        {
            DX::ThrowIfFailed(mFrontPanelControl->GetButtonStates(&buttonReading));
        }
        state.buttons.rawButtons = buttonReading;

        XBOX_FRONT_PANEL_LIGHTS lightReading = {};
        if (IsAvailable())
        {
            DX::ThrowIfFailed(mFrontPanelControl->GetLightStates(&lightReading));
        }
        state.lights.rawLights = lightReading;

        state.buttons.button1      = (buttonReading & XBOX_FRONT_PANEL_BUTTONS_BUTTON1) == XBOX_FRONT_PANEL_BUTTONS_BUTTON1;
        state.buttons.button2      = (buttonReading & XBOX_FRONT_PANEL_BUTTONS_BUTTON2) == XBOX_FRONT_PANEL_BUTTONS_BUTTON2;
        state.buttons.button3      = (buttonReading & XBOX_FRONT_PANEL_BUTTONS_BUTTON3) == XBOX_FRONT_PANEL_BUTTONS_BUTTON3;
        state.buttons.button4      = (buttonReading & XBOX_FRONT_PANEL_BUTTONS_BUTTON4) == XBOX_FRONT_PANEL_BUTTONS_BUTTON4;
        state.buttons.button5      = (buttonReading & XBOX_FRONT_PANEL_BUTTONS_BUTTON5) == XBOX_FRONT_PANEL_BUTTONS_BUTTON5;
        state.buttons.dpadLeft     = (buttonReading & XBOX_FRONT_PANEL_BUTTONS_LEFT)    == XBOX_FRONT_PANEL_BUTTONS_LEFT;
        state.buttons.dpadRight    = (buttonReading & XBOX_FRONT_PANEL_BUTTONS_RIGHT)   == XBOX_FRONT_PANEL_BUTTONS_RIGHT;
        state.buttons.dpadUp       = (buttonReading & XBOX_FRONT_PANEL_BUTTONS_UP)      == XBOX_FRONT_PANEL_BUTTONS_UP;
        state.buttons.dpadDown     = (buttonReading & XBOX_FRONT_PANEL_BUTTONS_DOWN)    == XBOX_FRONT_PANEL_BUTTONS_DOWN;
        state.buttons.buttonSelect = (buttonReading & XBOX_FRONT_PANEL_BUTTONS_SELECT)  == XBOX_FRONT_PANEL_BUTTONS_SELECT;

        state.lights.light1 = (lightReading & XBOX_FRONT_PANEL_LIGHTS_LIGHT1) == XBOX_FRONT_PANEL_LIGHTS_LIGHT1;
        state.lights.light2 = (lightReading & XBOX_FRONT_PANEL_LIGHTS_LIGHT2) == XBOX_FRONT_PANEL_LIGHTS_LIGHT2;
        state.lights.light3 = (lightReading & XBOX_FRONT_PANEL_LIGHTS_LIGHT3) == XBOX_FRONT_PANEL_LIGHTS_LIGHT3;
        state.lights.light4 = (lightReading & XBOX_FRONT_PANEL_LIGHTS_LIGHT4) == XBOX_FRONT_PANEL_LIGHTS_LIGHT4;
        state.lights.light5 = (lightReading & XBOX_FRONT_PANEL_LIGHTS_LIGHT5) == XBOX_FRONT_PANEL_LIGHTS_LIGHT5;
    }

    void SetLightStates(const XBOX_FRONT_PANEL_LIGHTS &lights)
    {
        if (IsAvailable())
        {
            DX::ThrowIfFailed(mFrontPanelControl->SetLightStates(lights));
        }
    }

    bool IsAvailable() const
    {
        return mFrontPanelControl;
    }
};

FrontPanelInput::Impl* FrontPanelInput::Impl::s_frontPanelInput = nullptr;
#pragma endregion

// --------------------------------------------------------------------------------
// FrontPanelInput methods
// --------------------------------------------------------------------------------
#pragma region  FrontPanelInput methods
// Public constructor.
FrontPanelInput::FrontPanelInput(_In_ IXboxFrontPanelControl * frontPanelControl)
    : pImpl( new Impl(this, frontPanelControl))
{
}

FrontPanelInput::FrontPanelInput(FrontPanelInput&&) noexcept = default;
FrontPanelInput& FrontPanelInput::operator=(FrontPanelInput&&) noexcept = default;
FrontPanelInput::~FrontPanelInput() = default;

FrontPanelInput::State FrontPanelInput::GetState()
{
    State state;
    pImpl->GetState(state);
    return state;
}

void FrontPanelInput::SetLightStates(const XBOX_FRONT_PANEL_LIGHTS &lights)
{
    pImpl->SetLightStates(lights);
}

FrontPanelInput & FrontPanelInput::Get()
{
    if (!Impl::s_frontPanelInput || !Impl::s_frontPanelInput->mOwner)
        throw std::exception("FrontPanelInput is a singleton");

    return *Impl::s_frontPanelInput->mOwner;
}

bool FrontPanelInput::IsAvailable() const
{
    return pImpl->IsAvailable();
}
#pragma endregion

// --------------------------------------------------------------------------------
// FrontPanelInput::ButtonStateTracker methods
// --------------------------------------------------------------------------------
#pragma region FrontPanelInput::ButtonStateTracker methods

#define UPDATE_BUTTON_STATE(field) field = static_cast<ButtonState>( ( !!state.buttons.field ) | ( ( !!state.buttons.field ^ !!lastState.buttons.field ) << 1 ) );

void FrontPanelInput::ButtonStateTracker::Update(const State& state)
{
    buttonsChanged = !!(state.buttons.rawButtons ^ lastState.buttons.rawButtons);

    UPDATE_BUTTON_STATE(button1);

    assert((!state.buttons.button1 && !lastState.buttons.button1) == (button1 == UP));
    assert((state.buttons.button1 && lastState.buttons.button1) == (button1 == HELD));
    assert((!state.buttons.button1 && lastState.buttons.button1) == (button1 == RELEASED));
    assert((state.buttons.button1 && !lastState.buttons.button1) == (button1 == PRESSED));

    UPDATE_BUTTON_STATE(button2);
    UPDATE_BUTTON_STATE(button3);
    UPDATE_BUTTON_STATE(button4);
    UPDATE_BUTTON_STATE(button5);
    UPDATE_BUTTON_STATE(dpadLeft);
    UPDATE_BUTTON_STATE(dpadRight);
    UPDATE_BUTTON_STATE(dpadUp);
    UPDATE_BUTTON_STATE(dpadDown);
    UPDATE_BUTTON_STATE(buttonSelect);

    lastState = state;
}

#undef UPDATE_BUTTON_STATE

void FrontPanelInput::ButtonStateTracker::Reset()
{
    memset(this, 0, sizeof(ButtonStateTracker));
}
#pragma endregion
