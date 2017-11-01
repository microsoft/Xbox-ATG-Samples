//--------------------------------------------------------------------------------------
// MemoryBanks.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "MemoryBanks.h"

#include "ATGColors.h"
#include "ControllerFont.h"
#include "MemoryDemo.h"

extern void ExitSample();

using namespace DirectX;

using Microsoft::WRL::ComPtr;

Sample::Sample() :
    m_frame(0)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN);
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(IUnknown* window)
{
    m_randomBankStatus = TestStatus::TEST_NOT_RUN;
    m_fixedBankStatus = TestStatus::TEST_NOT_RUN;
    m_readOnlyBankStatus = TestStatus::TEST_NOT_RUN;
    m_bankSwitchingStatus = TestStatus::TEST_NOT_RUN;
    m_sharedAddressStatus = TestStatus::TEST_NOT_RUN;

    m_gamePad = std::make_unique<GamePad>();

    m_deviceResources->SetWindow(window);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();
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
void Sample::Update(DX::StepTimer const& /*timer*/)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    if (m_frame == 1)
    {
        // Create the two data files that are used by the sample
        m_memoryBankDemo.CreateFixedTestDataFile();
        m_memoryBankDemo.CreateRandomTestDataFile();
    }
    else if (m_frame > 1)
    {
        auto pad = m_gamePad->GetState(0);
        if (pad.IsConnected())
        {
            m_gamePadButtons.Update(pad);

            if (pad.IsAPressed())
            {
                m_randomBankStatus = m_memoryBankDemo.RunTest(ATG::MemoryBankDemoTests::MEMORY_BANK_RANDOM_ADDRESS) ? TestStatus::TEST_SUCCESS : TestStatus::TEST_FAILURE;
            }
            if (pad.IsBPressed())
            {
                m_fixedBankStatus = m_memoryBankDemo.RunTest(ATG::MemoryBankDemoTests::MEMORY_BANK_FIXED_ADDRESS) ? TestStatus::TEST_SUCCESS : TestStatus::TEST_FAILURE;
            }
            if (pad.IsXPressed())
            {
                m_bankSwitchingStatus = m_memoryBankDemo.RunTest(ATG::MemoryBankDemoTests::BANK_SWITCHING) ? TestStatus::TEST_SUCCESS : TestStatus::TEST_FAILURE;
            }
            if (pad.IsYPressed())
            {
                m_sharedAddressStatus = m_memoryBankDemo.RunTest(ATG::MemoryBankDemoTests::SHARED_ADDRESS) ? TestStatus::TEST_SUCCESS : TestStatus::TEST_FAILURE;
            }
            if (pad.IsLeftShoulderPressed())
            {
                m_readOnlyBankStatus = m_memoryBankDemo.RunTest(ATG::MemoryBankDemoTests::READ_ONLY_MEMORY_BANK) ? TestStatus::TEST_SUCCESS : TestStatus::TEST_FAILURE;
            }

            if (pad.IsViewPressed())
            {
                ExitSample();
            }
        }
        else
        {
            m_gamePadButtons.Reset();
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

    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Render");

    RECT safeRect = SimpleMath::Viewport::ComputeTitleSafeArea(1920, 1080);
    XMFLOAT2 pos(float(safeRect.left), float(safeRect.top));

    m_spriteBatch->Begin();
    m_spriteBatch->Draw(m_background.Get(), m_deviceResources->GetOutputSize());

    DrawStatusString(L"[A]", L"Random Address", m_randomBankStatus, pos);
    DrawHelpText(pos, ATG::MemoryBankDemoTests::MEMORY_BANK_RANDOM_ADDRESS);
    pos.y += m_font->GetLineSpacing() * 3;

    DrawStatusString(L"[B]", L"Fixed Address", m_fixedBankStatus, pos);
    DrawHelpText(pos, ATG::MemoryBankDemoTests::MEMORY_BANK_FIXED_ADDRESS);
    pos.y += m_font->GetLineSpacing() * 3;

    DrawStatusString(L"[X]", L"Bank Switching", m_bankSwitchingStatus, pos);
    DrawHelpText(pos, ATG::MemoryBankDemoTests::BANK_SWITCHING);
    pos.y += m_font->GetLineSpacing() * 3;

    DrawStatusString(L"[Y]", L"Shared Address", m_sharedAddressStatus, pos);
    DrawHelpText(pos, ATG::MemoryBankDemoTests::SHARED_ADDRESS);
    pos.y += m_font->GetLineSpacing() * 3;

    DrawStatusString(L"[LB]", L"Read Only Address", m_readOnlyBankStatus, pos);
    DrawHelpText(pos, ATG::MemoryBankDemoTests::READ_ONLY_MEMORY_BANK);

    m_spriteBatch->End();

    PIXEndEvent(context);

    // Show the new frame.
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    m_graphicsMemory->Commit();
    PIXEndEvent(context);
}

void Sample::DrawStatusString(const std::wstring& button, const std::wstring& testName, TestStatus status, XMFLOAT2& pos)
{
    RECT stringBounds;
    std::wstring outputString;
    std::wstring resultString;
    XMVECTORF32 resultColor = ATG::Colors::OffWhite;
    outputString = L"Press ";
    outputString += button;
    outputString += L" to run ";
    outputString += testName;
    outputString += L" Test: ";
    switch (status)
    {
    case TestStatus::TEST_NOT_RUN:
        resultString += L"not run yet";
        break;
    case TestStatus::TEST_SUCCESS:
        resultString += L"success";
        resultColor = ATG::Colors::Green;
        break;
    case TestStatus::TEST_FAILURE:
        resultString += L"failure";
        resultColor = ATG::Colors::Orange;
        break;
    }
    DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(), outputString.c_str(), pos);
    stringBounds = DX::MeasureControllerDrawBounds(m_font.get(), m_ctrlFont.get(), outputString.c_str(), pos);
    pos.x += (stringBounds.right - stringBounds.left);
    m_font->DrawString(m_spriteBatch.get(), resultString.c_str(), pos, resultColor);
    pos.x -= (stringBounds.right - stringBounds.left);
}

void Sample::DrawHelpText(DirectX::XMFLOAT2& pos, ATG::MemoryBankDemoTests whichTest)
{
    static const wchar_t *helpText[] = {
        L"  Baseline for all demonstrations.",
        L"    Allocates a block of memory through VirtualAlloc and then reads a binary tree into the memory block.",
        L"    The pointers within the binary tree are then fixed up to match the address of the memory block.",
        L"  Memory block allocated at a predetermined virtual address.",
        L"    The memory block is allocated at a virtual address that doesn't change between runs of the title.",
        L"    This removes the need for the fix up of the pointers in the binary tree, the values can be saved to disk directly.",
        L"  Two memory blocks that can have their virtual address swapped",
        L"    Two memory banks are created through VirtualAlloc, Bank A and Bank B.",
        L"    The banks then have their virtual address swapped. The virtual address of bank A now points to the physical address of Bank B, and vice versa.",
        L"  Two memory banks that have their own unique virtual addresses, but a shared physical address.",
        L"    One physical memory bank is created through AllocateTitlePhysicalPages.",
        L"    The single physical bank is then mapped to two virtual addresses using MapTitlePhysicalPages.",
        L"  Changing the protection scheme of a memory bank to read-only",
        L"    Uses the shared setup with one physical bank mapped to two virtual addresses, Bank A and Bank B",
        L"    Bank A is set to read-only while Bank B stays as read-write. A protection fault is generated when writing through Bank A",
    };
    uint32_t startIndex(0);
    switch (whichTest)
    {
    case ATG::MemoryBankDemoTests::MEMORY_BANK_RANDOM_ADDRESS:
        startIndex = 0;
        break;
    case ATG::MemoryBankDemoTests::MEMORY_BANK_FIXED_ADDRESS:
        startIndex = 3;
        break;
    case ATG::MemoryBankDemoTests::BANK_SWITCHING:
        startIndex = 6;
        break;
    case ATG::MemoryBankDemoTests::SHARED_ADDRESS:
        startIndex = 9;
        break;
    case ATG::MemoryBankDemoTests::READ_ONLY_MEMORY_BANK:
        startIndex = 12;
        break;
    }
    for (uint32_t i = 0; i < 3; i++)
    {
        pos.y += m_font->GetLineSpacing() * 1.1f;
        m_font->DrawString(m_spriteBatch.get(), helpText[i + startIndex], pos);
    }
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
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto device = m_deviceResources->GetD3DDevice();

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device, m_deviceResources->GetBackBufferCount());

    m_spriteBatch = std::make_unique<SpriteBatch>(context);
    m_font = std::make_unique<SpriteFont>(device, L"SegoeUI_18.spritefont");
    m_ctrlFont = std::make_unique<SpriteFont>(device, L"XboxOneController.spritefont");
    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, L"ATGSampleBackground.DDS", nullptr, m_background.ReleaseAndGetAddressOf()));
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
}
#pragma endregion
