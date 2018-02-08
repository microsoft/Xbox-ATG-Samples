//--------------------------------------------------------------------------------------
// TestController.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

//
// Simple class for controller under test
//
class TestController
{
    public:
        TestController();
        explicit TestController(Windows::Xbox::Input::IController^ controller);
        TestController(const TestController& rhs);
        TestController(TestController&& rhs);
        ~TestController();

        TestController& operator=(const TestController& rhs);
        TestController& operator=(TestController&& rhs);

        bool operator==(const TestController& rhs);
        bool operator==(const Windows::Xbox::Input::IController^ rhs);

        void swap(TestController& rhs);

        const Windows::Xbox::Input::IController^ Controller() const;
        Windows::Xbox::Input::IController^ Controller();

        const Microsoft::Xbox::Input::IArcadeStick^ ArcadeStick() const;
        Microsoft::Xbox::Input::IArcadeStick^ ArcadeStick();

        const wchar_t* Type() const;

        void OnDisconnect();
        void OnConnect();

        bool IsConnected() const;

    private:
        Microsoft::Xbox::Input::IArcadeStick^   pIArcadeStick_;
        Windows::Xbox::Input::IController^      pIController_;

        bool isConnected_;
};

inline bool operator==(const Windows::Xbox::Input::IController^ lhs, const TestController& rhs)
{
    return (lhs == rhs.Controller());
}
