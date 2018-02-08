//--------------------------------------------------------------------------------------
// Buttons.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "buttons.h"
#include "ATGColors.h"

using namespace DirectX;

namespace
{
    const unsigned int INVALID_BUTTON_ID = 0xFFFFFFFF;
}

// static member initialization
bool ButtonGrid::m_fIsInitialized = false;


//--------------------------------------------------------------------------------------
// Name: ButtonGrid()
// Desc: Initializing constructor
//--------------------------------------------------------------------------------------
ButtonGrid::ButtonGrid(unsigned int rows, unsigned int cols, ButtonFunc callback, ID3D11DeviceX* d3ddevice, ID3D11DeviceContextX* deviceContext, D3D11_RECT screenRect)
    : m_ScreenRect(screenRect)
{
    using namespace DirectX;

    // check that initial conditions are valid for creating the grid
    if (m_fIsInitialized)
    {
        // don't initialize again if we've already done it
        return;
    }
    if (callback == nullptr)
    {
        // ensure we have a valid callback
        throw std::invalid_argument("Callback pointer");
    }

    m_spriteBatch = std::make_unique<SpriteBatch>(deviceContext);
    m_primBatch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(deviceContext);

    m_font = std::make_unique<SpriteFont>(d3ddevice, L"SegoeUI_24.spritefont");
    m_fontDimensions = m_font->MeasureString(L" ");

    // make sure we can fit the buttons on the screen
    unsigned int width = m_ScreenRect.right - m_ScreenRect.left,
        height = m_ScreenRect.bottom - m_ScreenRect.top;

    m_cellWidth = width / cols,
    m_cellHeight = height / rows;

    // allocate
    m_pGrid = new ButtonData *[rows];
    for (unsigned int i = 0; i < rows; ++i)
    {
        m_pGrid[i] = new ButtonData[cols];
    }

    // initialize

    DoButton = callback;

    // Initialize each button, but don't draw it yet. Buttons are not valid until they're called 
    unsigned int top = m_ScreenRect.top;
    for (unsigned int i = 0; i < rows; ++i)
    {
        unsigned int left = m_ScreenRect.left;

        for (unsigned int j = 0; j < cols; ++j)
        {
            m_pGrid[i][j].rcAddress.Row = i;
            m_pGrid[i][j].rcAddress.Col = j;
            m_pGrid[i][j].left = left + BUTTON_SPACING;
            m_pGrid[i][j].top = top + BUTTON_SPACING;
            m_pGrid[i][j].right = left + m_cellWidth - BUTTON_SPACING;
            m_pGrid[i][j].bottom = top + m_cellHeight -  BUTTON_SPACING;
            m_pGrid[i][j].currState = ButtonState::up;
            m_pGrid[i][j].lastState = ButtonState::up;
            m_pGrid[i][j].currColor = ATG::Colors::White;
            m_pGrid[i][j].id = INVALID_BUTTON_ID;
            m_pGrid[i][j].fValid = false;
            left += m_cellWidth;
        }
        top += m_cellHeight;
    }
    m_pGrid[0][0].currColor = ATG::Colors::Orange;
    m_rowCount = rows;
    m_colCount = cols;
    m_activeButton.Row = 0;
    m_activeButton.Col = 0;
    m_fIsInitialized = true;
}


ButtonGrid::~ButtonGrid()
{
    if (m_pGrid != nullptr)
    {
        for (unsigned int i = 0; i < m_rowCount; ++i)
        {
            delete [] m_pGrid[i];
        }
        delete [] m_pGrid;
    }
}


//--------------------------------------------------------------------------------------
// Name: ButtonGrid()
// Desc: Redraw all valid buttons
//--------------------------------------------------------------------------------------
void ButtonGrid::DrawButtons()
{
    m_primBatch->Begin();
    m_spriteBatch->Begin();
    for (unsigned int row = 0; row < m_rowCount; ++ row)
    {
        for (unsigned int col = 0; col < m_colCount; ++ col)
        {
            if (m_pGrid[row][col].fValid)
            {
                DrawButton(row, col); 
            }
        }
    }
    m_spriteBatch->End();
    m_primBatch->End();
}


//--------------------------------------------------------------------------------------
// Name: ChangeButtonText()
// Desc: Set text for a button specified by its id
//--------------------------------------------------------------------------------------
void ButtonGrid::ChangeButtonText(unsigned int id, std::wstring newText)
{
    bool fFound = false;
    unsigned int row = 0, col;

    while (!fFound && row < m_rowCount)
    {
        col = 0;
        while (!fFound && col < m_colCount)
        {
            if ((m_pGrid[row][col].fValid) && (m_pGrid[row][col].id == id))
            {
                fFound = true;
                m_pGrid[row][col].label = newText;
                return;
            }
            else 
            {
                ++col;
            }
        }
        ++row;
    }
    if (!fFound)
    {
        throw ref new Platform::COMException(E_INVALIDARG, "No such ID exists");
    }   
}


//--------------------------------------------------------------------------------------
// Name: SetButton()
// Desc: Set text and ID for a button at a particular location in the button grid
//--------------------------------------------------------------------------------------
void ButtonGrid::SetButton(uint32_t id, uint8_t row, uint8_t col, std::wstring label)
{
    m_pGrid[row][col].id = id;
    m_pGrid[row][col].label = label;
    m_pGrid[row][col].fValid = true;
}


//--------------------------------------------------------------------------------------
// Name: OnWheelData()
// Desc: Update buttons to reflex input
//--------------------------------------------------------------------------------------
void ButtonGrid::OnWheelData(Windows::Xbox::Input::INavigationReading ^ reading)
{
    m_primBatch->Begin();
    m_spriteBatch->Begin();

    static Windows::Xbox::Input::INavigationReading^ lastReading = nullptr;

    if (reading == nullptr)
    {
        return;
    }

    FLOAT fontY = m_ScreenRect.top + (XMVectorGetY(m_fontDimensions) * LARGE_TEXT_SCALE * 2.5f);
    FLOAT fontYDelta = XMVectorGetY(m_fontDimensions) * NORMAL_TEXT_SCALE * 1.25f;

    // Draw instructions

    fontY += fontYDelta * 6.0f;

    if (lastReading == nullptr)
    {
        lastReading = reading;
    }

    if (reading->IsAcceptPressed)
    {
        UpdateState(ButtonState::down);
        if (ActiveButton.currState != ActiveButton.lastState)
        {
            ButtonData activeButton = GetActiveButton();
            DoButton(&activeButton);
        }
    }
    else
    {
        UpdateState(ButtonState::up);
    }

    if (lastReading->Buttons != reading->Buttons)
    {
        // Draw text indicating which buttons are pressed

        if (reading->IsDownPressed)
        {
            FocusButton(ButtonBelow);
        }

        if (reading->IsUpPressed)
        {
            FocusButton(ButtonAbove);
        }

        if (reading->IsLeftPressed)
        {
            FocusButton(ButtonLeft);
        }

        if (reading->IsRightPressed)
        {
            FocusButton(ButtonRight);
        }

    }

    lastReading = reading;

    m_primBatch->End();
    m_spriteBatch->End();

    return;
}


//--------------------------------------------------------------------------------------
// Name: OnWheelData()
// Desc: Update current state and last state of the active button when it's pressed
//--------------------------------------------------------------------------------------
void ButtonGrid::UpdateState(ButtonState state)
{
    m_pGrid[m_activeButton.Row][m_activeButton.Col].lastState = 
        m_pGrid[m_activeButton.Row][m_activeButton.Col].currState;
    m_pGrid[m_activeButton.Row][m_activeButton.Col].currState = state;
}


//--------------------------------------------------------------------------------------
// Name: FocusButton()
// Desc: Set the focus to a particular button
//--------------------------------------------------------------------------------------
void ButtonGrid::FocusButton(ButtonCoords button)
{
    DrawBox(m_pGrid[button.Row][button.Col].left, 
        m_pGrid[button.Row][button.Col].top, 
        m_pGrid[button.Row][button.Col].right, 
        m_pGrid[button.Row][button.Col].bottom, 
        ATG::Colors::Blue);
    m_pGrid[button.Row][button.Col].currColor = ATG::Colors::Blue;
    if (button.Row != m_activeButton.Row || button.Col != m_activeButton.Col)
    {
        m_pGrid[m_activeButton.Row][m_activeButton.Col].currColor = ATG::Colors::White;
        m_activeButton.Row = button.Row;
        m_activeButton.Col = button.Col;
    }
}


//--------------------------------------------------------------------------------------
// Name: DrawBox()
// Desc: Draw a rectangle on the screen
//--------------------------------------------------------------------------------------
void ButtonGrid::DrawBox(LONG x1, LONG y1, LONG x2, LONG y2, FXMVECTOR vOutlineColor)
{
    XMVECTOR v[5];
    v[0] = XMVectorSet(x1 - 0.5f, y1 - 0.5f, 0.0f, 1.0f);
    v[1] = XMVectorSet(x2 - 0.5f, y1 - 0.5f, 0.0f, 1.0f);
    v[2] = XMVectorSet(x2 - 0.5f, y2 - 0.5f, 0.0f, 1.0f);
    v[3] = XMVectorSet(x1 - 0.5f, y2 - 0.5f, 0.0f, 1.0f);
    v[4] = XMVectorSet(x1 - 0.5f, y1 - 0.5f, 0.0f, 1.0f);

    m_primBatch->DrawLine(VertexPositionColor(v[0], vOutlineColor), VertexPositionColor(v[1], vOutlineColor));
    m_primBatch->DrawLine(VertexPositionColor(v[1], vOutlineColor), VertexPositionColor(v[2], vOutlineColor));
    m_primBatch->DrawLine(VertexPositionColor(v[2], vOutlineColor), VertexPositionColor(v[3], vOutlineColor));
    m_primBatch->DrawLine(VertexPositionColor(v[3], vOutlineColor), VertexPositionColor(v[0], vOutlineColor));

    return;
}


//--------------------------------------------------------------------------------------
// Name: GetButtonRect()
// Desc: Get the screen coordinates for a button based on the size of its text.
//--------------------------------------------------------------------------------------
D3D11_RECT ButtonGrid::GetButtonRect(LONG left, LONG top, const wchar_t* str)
{
    D3D11_RECT rc = {};

    if (str == nullptr)
    {
        throw ref new Platform::COMException(E_INVALIDARG);
    }

    float fontHeight = XMVectorGetY(m_fontDimensions);


    XMVECTOR vecStringDimensions = m_font->MeasureString(str); 

    rc.left = left;
    rc.top = top;
    rc.bottom = rc.top + (LONG)(XMVectorGetY(vecStringDimensions) + fontHeight* 1.2 - fontHeight);
    rc.right = rc.left + (LONG)(XMVectorGetX(vecStringDimensions) + XMVectorGetX(m_font->MeasureString(L"  ")));

    return rc;
}


//--------------------------------------------------------------------------------------
// Name: DrawButton()
// Desc: Draw a button on the screen located at the given row and column.
//--------------------------------------------------------------------------------------
void ButtonGrid::DrawButton(unsigned int row, unsigned int col)
{
    ButtonData& btnData = m_pGrid[row][col];
    LONG offset = m_ScreenRect.left;

    D3D11_RECT rc = GetButtonRect(btnData.left, btnData.top, btnData.label.c_str());
    DrawBox(rc.left, rc.top, rc.right, rc.bottom, btnData.currColor);
    btnData.left = rc.left;
    btnData.top = rc.top;
    btnData.bottom = rc.bottom;
    btnData.right = rc.right;

    m_font->DrawString(m_spriteBatch.get(), btnData.label.c_str(), XMFLOAT2(XMVectorGetX(m_fontDimensions) + rc.left - offset, rc.top - XMVectorGetY(m_fontDimensions) * LARGE_TEXT_SCALE), ATG::Colors::White);
}


//--------------------------------------------------------------------------------------
// Name: GetButtonRight()
// Desc: Get the row and column for the next valid button to the right of the currently active button.
//--------------------------------------------------------------------------------------
ButtonCoords ButtonGrid::GetButtonRight()
{
    ButtonCoords right;
    unsigned int currRow = m_activeButton.Row,
          currCol = m_activeButton.Col;
    do {
        right.Row = currRow;
        right.Col = (currCol + 1) % m_colCount;
        currCol = right.Col;
    } while (!m_pGrid[right.Row][right.Col].fValid && right.Col != m_activeButton.Col);
    return right;
}


//--------------------------------------------------------------------------------------
// Name: GetButtonLeft()
// Desc: Get the row and column for the next valid button to the left of the currently active button.
//--------------------------------------------------------------------------------------
ButtonCoords ButtonGrid::GetButtonLeft()
{
    ButtonCoords left;
    unsigned int currRow = m_activeButton.Row,
          currCol = m_activeButton.Col;
    do {
        left.Row = currRow;
        left.Col = (currCol + m_colCount - 1) % m_colCount;
        currCol = left.Col;
    } while (!m_pGrid[left.Row][left.Col].fValid && left.Col != m_activeButton.Col);
    return left;
}


//--------------------------------------------------------------------------------------
// Name: GetButtonBelow()
// Desc: Get the row and column for the next valid button below the currently active button.
//--------------------------------------------------------------------------------------
ButtonCoords ButtonGrid::GetButtonBelow()
{
    ButtonCoords down;
    unsigned int currRow = m_activeButton.Row,
          currCol = m_activeButton.Col;
    do {
        down.Col = currCol;
        down.Row = (currRow + 1) % m_rowCount;
        currRow = down.Row;
    } while (!m_pGrid[down.Row][down.Col].fValid && down.Row != m_activeButton.Row);
    return down;
}


//--------------------------------------------------------------------------------------
// Name: GetButtonAbove()
// Desc: Get the row and column for the next valid button above the currently active button.
//--------------------------------------------------------------------------------------
ButtonCoords ButtonGrid::GetButtonAbove()
{
    ButtonCoords up;
    unsigned int currRow = m_activeButton.Row,
          currCol = m_activeButton.Col;
    do {
        up.Col = currCol;
        up.Row = (currRow + m_rowCount - 1) % m_rowCount;
        currRow = up.Row;
    } while (!m_pGrid[up.Row][up.Col].fValid && up.Row != m_activeButton.Row);
    return up;
}
