//--------------------------------------------------------------------------------------
// TestController.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "TestController.h"

using namespace Microsoft::Xbox::Input;
using namespace Windows::Xbox::Input;


TestController::TestController() :
    isConnected_(false)
{
}

TestController::TestController(IController^ controller) :
    pIController_(controller),
    isConnected_(true)
{
    if (pIController_->Type == "Microsoft.Xbox.Input.ArcadeStick")
    {
        pIArcadeStick_ = (Microsoft::Xbox::Input::ArcadeStick^)pIController_;
    }
}

TestController::TestController(const TestController& rhs) :
    pIController_(rhs.pIController_),
    pIArcadeStick_(rhs.pIArcadeStick_),
    isConnected_(rhs.isConnected_)
{
}

TestController::TestController(TestController&& rhs) :
    pIController_(rhs.pIController_),
    pIArcadeStick_(rhs.pIArcadeStick_),
    isConnected_(rhs.isConnected_)
{
    rhs.pIController_ = nullptr;
    rhs.pIArcadeStick_ = nullptr;
}

TestController::~TestController()
{
    pIController_ = nullptr;
    pIArcadeStick_ = nullptr;
}

TestController& TestController::operator=(const TestController& rhs)
{
    TestController tmp(rhs);
    tmp.swap(*this);

    return *this;
}

TestController& TestController::operator=(TestController&& rhs)
{
    pIController_ = rhs.pIController_;
    pIArcadeStick_ = rhs.pIArcadeStick_;
    isConnected_ = rhs.isConnected_;

    rhs.pIController_ = nullptr;
    rhs.pIArcadeStick_ = nullptr;
    rhs.isConnected_ = false;

    return *this;
}

bool TestController::operator==(const TestController& rhs)
{
    return (rhs.pIController_ == pIController_);
}

bool TestController::operator==(const IController^ rhs)
{
    return (rhs == pIController_);
}

void TestController::swap(TestController& rhs)
{
    std::swap(pIController_, rhs.pIController_);
    std::swap(pIArcadeStick_, rhs.pIArcadeStick_);
}

IController^ TestController::Controller()
{
    return pIController_;
}

const IController^ TestController::Controller() const
{
    return pIController_;
}

const IArcadeStick^ TestController::ArcadeStick() const
{
    return pIArcadeStick_;
}

IArcadeStick^ TestController::ArcadeStick()
{
    return pIArcadeStick_;
}


const wchar_t* TestController::Type() const
{
    return pIController_->Type->Data();
}

void TestController::OnDisconnect()
{
    if (!isConnected_)
    {
        return;
    }

    isConnected_ = false;
}

void TestController::OnConnect()
{
    if (isConnected_)
    {
        return;
    }

    isConnected_ = true;
}

bool TestController::IsConnected() const
{
    return isConnected_;
}
