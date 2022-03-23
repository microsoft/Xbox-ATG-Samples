//--------------------------------------------------------------------------------------
// FrontPanelInput.h
//
// Microsoft Xbox One XDK
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#if _XDK_VER < 0x3AD703ED /* XDK Edition 170300 */
#error This code requires the March 2017 XDK or later
#endif

#include <memory>

struct IXboxFrontPanelControl;
enum XBOX_FRONT_PANEL_LIGHTS;

namespace ATG
{
    class FrontPanelInput
    {
    public:
        FrontPanelInput(_In_ IXboxFrontPanelControl *frontPanelControl);

        FrontPanelInput(FrontPanelInput &&) noexcept;
        FrontPanelInput& operator=(FrontPanelInput &&) noexcept;

        FrontPanelInput(FrontPanelInput const&) = delete;
        FrontPanelInput& operator=(FrontPanelInput const&) = delete;

        virtual ~FrontPanelInput();

        struct Buttons
        {
            bool button1;
            bool button2;
            bool button3;
            bool button4;
            bool button5;
            bool dpadLeft;
            bool dpadRight;
            bool dpadUp;
            bool dpadDown;
            bool buttonSelect;
            uint32_t rawButtons;
        };

        struct Lights
        {
            bool light1;
            bool light2;
            bool light3;
            bool light4;
            bool light5;
            uint32_t rawLights;
        };

        struct State
        {
            Buttons buttons;
            Lights lights;
        };

        class ButtonStateTracker
        {
        public:
            enum ButtonState
            {
                UP = 0,         // Button is up
                HELD = 1,       // Button is held down
                RELEASED = 2,   // Button was just released
                PRESSED = 3,    // Button was just pressed
            };

            ButtonState button1;
            ButtonState button2;
            ButtonState button3;
            ButtonState button4;
            ButtonState button5;
            ButtonState dpadLeft;
            ButtonState dpadRight;
            ButtonState dpadUp;
            ButtonState dpadDown;
            ButtonState buttonSelect;
            bool buttonsChanged;

            ButtonStateTracker() { Reset(); }

            void Update(const State& state);

            void Reset();

            State GetLastState() const { return lastState; }

        private:
            State lastState;
        };

        // Retrieve the current state of the FrontPanel buttons
        State GetState();

        // Set the lights on the FrontPanel
        void SetLightStates(const XBOX_FRONT_PANEL_LIGHTS &lights);
        
        // Singleton
        static FrontPanelInput& Get();

        // Determine whether the front panel is available
        bool IsAvailable() const;

    private:
        // Private implementation.
        class Impl;

        std::unique_ptr<Impl> pImpl;
    };
} // namespace ATG
