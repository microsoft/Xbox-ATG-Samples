//--------------------------------------------------------------------------------------
// FrontPanelManager.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "pch.h"

#include "FrontPanelManager.h"
#include "TopLevelScreen.h"
#include "QuickActionScreen.h"
#include "QuickActionMappingScreen.h"
#include "FontViewerScreen.h"
#include "GPURenderScreen.h"

using namespace ATG;

using Microsoft::WRL::ComPtr;
using ButtonState = FrontPanelInput::ButtonStateTracker::ButtonState;

FrontPanelManager::FrontPanelManager()
    : m_currentPanelScreen(nullptr)
{

    if (IsXboxFrontPanelAvailable())
    {
        // Get the default front panel
        DX::ThrowIfFailed(GetDefaultXboxFrontPanel(m_frontPanelControl.ReleaseAndGetAddressOf()));

        // Initialize the FrontPanelDisplay object
        m_frontPanelDisplay = std::make_unique<FrontPanelDisplay>(m_frontPanelControl.Get());

        // Initialize the FrontPanelInput object
        m_frontPanelInput = std::make_unique<FrontPanelInput>(m_frontPanelControl.Get());

        // Initialize the button assignments
        for (int i = 0; i < m_buttonActionAssignments.size(); ++i)
        {
            m_buttonActionAssignments[i] = nullptr;
        }
    }
}

void FrontPanelManager::CreateScreens()
{
    // Early out if there is no front panel
    if (!m_frontPanelControl)
        return;

    // Create the demo screens
    auto& buttonActions = CreateChild<TopLevelScreen>(
        L"Quick Actions",
        L"1",
        L"Scroll down to see the action assignment\n"
        L"for each of the 5 front panel buttons.");
    auto& fontViewer = CreateChild<TopLevelScreen>(
        L"CPU Fonts",
        L"2",
        L"Scroll down to see the different font styles and\n"
        L"scroll left and right to change the font size.");
    auto& gpuRender = CreateChild<TopLevelScreen>(
        L"GPU to Front Panel",
        L"3",
        L"Scroll down to use the GPU to render for the\n"
        L"Front Panel.");
    auto& buttonMappings = CreateChild<TopLevelScreen>(
        L"Button Mappings",
        L"4",
        L"Scroll down to change the action assigment\n"
        L"for each of the 5 front panel buttons.\n");
    

    auto& quickAction1 = CreateChild<QuickActionScreen>(XBOX_FRONT_PANEL_BUTTONS_BUTTON1);
    auto& quickAction2 = CreateChild<QuickActionScreen>(XBOX_FRONT_PANEL_BUTTONS_BUTTON2);
    auto& quickAction3 = CreateChild<QuickActionScreen>(XBOX_FRONT_PANEL_BUTTONS_BUTTON3);
    auto& quickAction4 = CreateChild<QuickActionScreen>(XBOX_FRONT_PANEL_BUTTONS_BUTTON4);
    auto& quickAction5 = CreateChild<QuickActionScreen>(XBOX_FRONT_PANEL_BUTTONS_BUTTON5);

    auto& fontViewArial = CreateChild<FontViewerScreen>(L"Arial Narrow", 16, L"assets\\ArialNarrow16.rasterfont");
    fontViewArial.AddFontFile(12, L"assets\\ArialNarrow12.rasterfont");
    fontViewArial.AddFontFile(24, L"assets\\ArialNarrow24.rasterfont");
    fontViewArial.AddFontFile(32, L"assets\\ArialNarrow32.rasterfont");
    fontViewArial.AddFontFile(64, L"assets\\ArialNarrow64.rasterfont");

    auto& fontViewLucida = CreateChild<FontViewerScreen>(L"Lucida Console", 16, L"assets\\LucidaConsole16.rasterfont");
    fontViewLucida.AddFontFile(12, L"assets\\LucidaConsole12.rasterfont");
    fontViewLucida.AddFontFile(24, L"assets\\LucidaConsole24.rasterfont");
    fontViewLucida.AddFontFile(32, L"assets\\LucidaConsole32.rasterfont");
    fontViewLucida.AddFontFile(64, L"assets\\LucidaConsole64.rasterfont");

    auto& fontViewSegoe = CreateChild<FontViewerScreen>(L"Segoe UI", 16, L"assets\\Segoe_UI16.rasterfont");
    fontViewSegoe.AddFontFile(12, L"assets\\Segoe_UI12.rasterfont");
    fontViewSegoe.AddFontFile(24, L"assets\\Segoe_UI24.rasterfont");
    fontViewSegoe.AddFontFile(32, L"assets\\Segoe_UI32.rasterfont");
    fontViewSegoe.AddFontFile(64, L"assets\\Segoe_UI64.rasterfont");

    auto& fontViewSegoeBold = CreateChild<FontViewerScreen>(L"Segoe UI Bold", 16, L"assets\\Segoe_UI_bold16.rasterfont");
    fontViewSegoeBold.AddFontFile(12, L"assets\\Segoe_UI_bold12.rasterfont");
    fontViewSegoeBold.AddFontFile(24, L"assets\\Segoe_UI_bold24.rasterfont");
    fontViewSegoeBold.AddFontFile(32, L"assets\\Segoe_UI_bold32.rasterfont");
    fontViewSegoeBold.AddFontFile(64, L"assets\\Segoe_UI_bold64.rasterfont");

    auto& gpuRenderScreen = CreateChild<GPURenderScreen>();

    auto& quickActionMapper = CreateChild<QuickActionMappingScreen>();

    
    buttonActions.SetLeftNeighbor(&buttonMappings);
    buttonActions.SetRightNeighbor(&fontViewer);
    buttonActions.SetDownNeighbor(&quickAction1);

    fontViewer.SetLeftNeighbor(&buttonActions);
    fontViewer.SetRightNeighbor(&gpuRender);
    fontViewer.SetDownNeighbor(&fontViewArial);
    
    fontViewArial.SetUpNeighbor(&fontViewer);
    fontViewArial.SetDownNeighbor(&fontViewLucida);

    fontViewLucida.SetUpNeighbor(&fontViewArial);
    fontViewLucida.SetDownNeighbor(&fontViewSegoe);

    fontViewSegoe.SetUpNeighbor(&fontViewLucida);
    fontViewSegoe.SetDownNeighbor(&fontViewSegoeBold);

    fontViewSegoeBold.SetUpNeighbor(&fontViewSegoe);

    gpuRender.SetLeftNeighbor(&fontViewer);
    gpuRender.SetRightNeighbor(&buttonMappings);
    gpuRender.SetDownNeighbor(&gpuRenderScreen);
    gpuRenderScreen.SetUpNeighbor(&gpuRender);

    buttonMappings.SetLeftNeighbor(&gpuRender);
    buttonMappings.SetRightNeighbor(&buttonActions);
    buttonMappings.SetDownNeighbor(&quickActionMapper);


    quickAction1.SetUpNeighbor(&buttonActions);
    quickAction1.SetLeftNeighbor(&quickAction5);
    quickAction1.SetRightNeighbor(&quickAction2);

    quickAction2.SetUpNeighbor(&buttonActions);
    quickAction2.SetLeftNeighbor(&quickAction1);
    quickAction2.SetRightNeighbor(&quickAction3);
    
    quickAction3.SetUpNeighbor(&buttonActions);
    quickAction3.SetLeftNeighbor(&quickAction2);
    quickAction3.SetRightNeighbor(&quickAction4);
    
    quickAction4.SetUpNeighbor(&buttonActions);
    quickAction4.SetLeftNeighbor(&quickAction3);
    quickAction4.SetRightNeighbor(&quickAction5);
    
    quickAction5.SetUpNeighbor(&buttonActions);
    quickAction5.SetLeftNeighbor(&quickAction4);
    quickAction5.SetRightNeighbor(&quickAction1);
    
    quickActionMapper.SetUpNeighbor(&buttonMappings);
    
    buttonActions.Resume(nullptr);
}

void FrontPanelManager::Update(DX::StepTimer const & timer)
{
    // Early out if there is no front panel
    if (!m_frontPanelControl)
        return;

    auto fpInput = m_frontPanelInput->GetState();
    m_frontPanelInputButtons.Update(fpInput);

    if (m_frontPanelInputButtons.buttonsChanged)
    {
        if(m_frontPanelInputButtons.button1 == ButtonState::PRESSED)
            OnButtonPressed(XBOX_FRONT_PANEL_BUTTONS_BUTTON1);

        if (m_frontPanelInputButtons.button2 == ButtonState::PRESSED)
            OnButtonPressed(XBOX_FRONT_PANEL_BUTTONS_BUTTON2);

        if (m_frontPanelInputButtons.button3 == ButtonState::PRESSED)
            OnButtonPressed(XBOX_FRONT_PANEL_BUTTONS_BUTTON3);

        if (m_frontPanelInputButtons.button4 == ButtonState::PRESSED)
            OnButtonPressed(XBOX_FRONT_PANEL_BUTTONS_BUTTON4);

        if (m_frontPanelInputButtons.button5 == ButtonState::PRESSED)
            OnButtonPressed(XBOX_FRONT_PANEL_BUTTONS_BUTTON5);

        if (m_frontPanelInputButtons.dpadLeft == ButtonState::PRESSED)
            OnButtonPressed(XBOX_FRONT_PANEL_BUTTONS_LEFT);

        if (m_frontPanelInputButtons.dpadRight == ButtonState::PRESSED)
            OnButtonPressed(XBOX_FRONT_PANEL_BUTTONS_RIGHT);

        if (m_frontPanelInputButtons.dpadUp == ButtonState::PRESSED)
            OnButtonPressed(XBOX_FRONT_PANEL_BUTTONS_UP);

        if (m_frontPanelInputButtons.dpadDown == ButtonState::PRESSED)
            OnButtonPressed(XBOX_FRONT_PANEL_BUTTONS_DOWN);

        if (m_frontPanelInputButtons.buttonSelect == ButtonState::PRESSED)
            OnButtonPressed(XBOX_FRONT_PANEL_BUTTONS_SELECT);
    }

    m_currentPanelScreen->Update(timer);
}

void FrontPanelManager::Navigate(const PanelScreen & navToChild)
{
    // Early out if there is no front panel
    if (!m_frontPanelControl)
        return;

    SetAssignedLigts();

    for (int i = 0; i < m_children.size(); ++i)
    {
        if (m_children[i].get() == &navToChild)
        {
            m_currentPanelScreen = m_children[i].get();
            return;
        }
    }
}

bool FrontPanelManager::OnButtonPressed(XBOX_FRONT_PANEL_BUTTONS whichButton)
{
    // Give the current screen first chance to handle the button press
    if (m_currentPanelScreen->OnButtonPressed(whichButton))
    {
        return true;
    }

    // Otherwise handle it ourselves
    switch (whichButton)
    {
    default:
        break;

    case XBOX_FRONT_PANEL_BUTTONS_SELECT:
        // Capture the front panel display (written in a number of file formats)
        m_frontPanelDisplay->SaveDDSToFile(L"D:\\FrontPanelScreen.dds");
        m_frontPanelDisplay->SaveWICToFile(L"D:\\FrontPanelScreen.bmp", GUID_ContainerFormatBmp);
        m_frontPanelDisplay->SaveWICToFile(L"D:\\FrontPanelScreen.gif", GUID_ContainerFormatGif);
        m_frontPanelDisplay->SaveWICToFile(L"D:\\FrontPanelScreen.jpg", GUID_ContainerFormatJpeg);
        m_frontPanelDisplay->SaveWICToFile(L"D:\\FrontPanelScreen.png", GUID_ContainerFormatPng);
        m_frontPanelDisplay->SaveWICToFile(L"D:\\FrontPanelScreen.tif", GUID_ContainerFormatTiff);
#ifdef _DEBUG
        OutputDebugStringA("Screenshot of front panel display written to development drive.\n");
#endif
        break;

    case XBOX_FRONT_PANEL_BUTTONS_BUTTON1:
    case XBOX_FRONT_PANEL_BUTTONS_BUTTON2:
    case XBOX_FRONT_PANEL_BUTTONS_BUTTON3:
    case XBOX_FRONT_PANEL_BUTTONS_BUTTON4:
    case XBOX_FRONT_PANEL_BUTTONS_BUTTON5:
        return InvokeButtonAction(int(GetIndexForButtonId(whichButton)));;
    }

    return false;
}

bool FrontPanelManager::InvokeButtonAction(int buttonId)
{
    const ButtonAction *action = m_buttonActionAssignments[buttonId];
    if (action)
    {
        (*action).Invoke();
        return true;
    }

    return false;
}

void FrontPanelManager::SetAssignedLigts() const
{
    auto& fpi = FrontPanelInput::Get();
    fpi.SetLightStates(GetAssignedLights());
}

void FrontPanelManager::CreateDeviceDependentResources(DX::DeviceResources * deviceResources)
{
    // Early out if there is no front panel
    if (!m_frontPanelControl)
        return;

    for (int i = 0; i < m_children.size(); ++i)
    {
        m_children[i]->CreateDeviceDependentResources(deviceResources, m_frontPanelControl.Get());
    }
}

void FrontPanelManager::CreateWindowSizeDependentResources(DX::DeviceResources * deviceResources)
{
    // Early out if there is no front panel
    if (!m_frontPanelControl)
        return;

    for (int i = 0; i < m_children.size(); ++i)
    {
        m_children[i]->CreateWindowSizeDependentResources(deviceResources);
    }
}

void FrontPanelManager::GPURender(DX::DeviceResources * deviceResources)
{
    // Early out if there is no front panel
    if (!m_frontPanelControl)
        return;

    m_currentPanelScreen->GPURender(deviceResources);
}

size_t FrontPanelManager::GetIndexForButtonId(XBOX_FRONT_PANEL_BUTTONS buttonId)
{
    switch (buttonId)
    {
    default:
        break;

    case XBOX_FRONT_PANEL_BUTTONS_BUTTON1:
        // TODO: perform action for button1
        return 0;

    case XBOX_FRONT_PANEL_BUTTONS_BUTTON2:
        // TODO: perform the action for button2
        return 1;

    case XBOX_FRONT_PANEL_BUTTONS_BUTTON3:
        // TODO: perform the action for button3
        return 2;

    case XBOX_FRONT_PANEL_BUTTONS_BUTTON4:
        // TODO: perform the action for button4
        return 3;

    case XBOX_FRONT_PANEL_BUTTONS_BUTTON5:
        // TODO: perform the action for button5
        return 4;
    }

    throw std::out_of_range("Invalid button Id");
}

size_t FrontPanelManager::ButtonActionCount() const
{
    return m_buttonActions.size();
}

const FrontPanelManager::ActionRecord &FrontPanelManager::operator[](size_t idx) const
{
    return *m_buttonActions[idx];
}

const FrontPanelManager::ActionRecord &FrontPanelManager::CreateButtonAction(const wchar_t * name, const wchar_t * description, std::function<void()> op)
{
    unsigned id = unsigned(m_buttonActions.size());

    ButtonAction *action = new ButtonAction;
    action->id = id;
    action->name = name;
    action->description = description;
    action->Invoke = op;

    m_buttonActions.emplace_back(action);
    return *m_buttonActions[id];
}

bool FrontPanelManager::IsActionAssigned(XBOX_FRONT_PANEL_BUTTONS buttonId) const
{
    size_t id = GetIndexForButtonId(buttonId);
    const ButtonAction *action = m_buttonActionAssignments[id];
    return action != nullptr;
}

const FrontPanelManager::ActionRecord &FrontPanelManager::GetActionAssignment(XBOX_FRONT_PANEL_BUTTONS buttonId) const
{
    size_t id = GetIndexForButtonId(buttonId);
    const ButtonAction *action = m_buttonActionAssignments[id];
    if (action == nullptr)
    {
        throw std::exception("No action assignment for button");
    }

    return *action;
}

void FrontPanelManager::AssignActionToButton(const ActionRecord &action, XBOX_FRONT_PANEL_BUTTONS buttonId)
{
    if (action.id >= m_buttonActions.size())
    {
        throw std::range_error("Invalid button action ID");
    }

    size_t id = GetIndexForButtonId(buttonId);
    m_buttonActionAssignments[id] = m_buttonActions[action.id].get();

    SetAssignedLigts();
}

void FrontPanelManager::ClearActionAssignment(XBOX_FRONT_PANEL_BUTTONS buttonId)
{
    size_t id = GetIndexForButtonId(buttonId);
    m_buttonActionAssignments[id] = nullptr;

    SetAssignedLigts();
}

XBOX_FRONT_PANEL_LIGHTS FrontPanelManager::GetAssignedLights() const
{
    XBOX_FRONT_PANEL_LIGHTS lights = XBOX_FRONT_PANEL_LIGHTS_NONE;

    for (int i = 0; i < m_buttonActionAssignments.size(); ++i)
    {
        if (m_buttonActionAssignments[i] != nullptr)
        {
            lights = static_cast<XBOX_FRONT_PANEL_LIGHTS>(lights | XBOX_FRONT_PANEL_LIGHTS_LIGHT1 << i);
        }
    }

    return lights;
}

void FrontPanelManager::AddChild(PanelScreen * child)
{
    m_children.emplace_back(child);
    if (m_currentPanelScreen == nullptr)
    {
        m_currentPanelScreen = child;
    }
}
