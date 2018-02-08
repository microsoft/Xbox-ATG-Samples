//--------------------------------------------------------------------------------------
// Buttons.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

namespace
{
    const float        LARGE_TEXT_SCALE = 2.0f;
    const float        NORMAL_TEXT_SCALE = 1.0f;
    const float        DEADZONE_BOX_INCREMENT = 0.01f;
    const unsigned int DEFAULT_ENVELOPE_ENTRY_DURATION = 4;
    const unsigned int IMPULSE_TRIGGER_ENVELOPE_ARRAY_MAX = 5;
}

//======================================================================================
// Button management
//======================================================================================

/// <summary>
/// minimum space between buttons
/// </summary> 
#define BUTTON_SPACING 5L

//--------------------------------------------------------------------------------------
// Button coordinates
//--------------------------------------------------------------------------------------

/// <summary>
/// Button grid position by row and column
/// </summary>
class ButtonCoords
{
private:
    unsigned int row, col;

public:
    // constructors

    /// <summary>
    /// initializing constructor
    /// </summary>
    /// <param name="_row">row value</param>
    /// <param name="_col">column value</param>
    ButtonCoords(unsigned int _row, unsigned int _col) 
    {
        row = _row;
        col = _col;
    }

    /// <summary>
    /// default constructor
    /// </summary>
    ButtonCoords():row(0),col(0) {};

    /// <summary>
    /// set row value
    /// </summary>
    /// <param name="_row">new row value</param>
    void putrow(unsigned int _row ) 
    { 
        row = _row;
    }

    /// <summary>
    /// get row value
    /// </summary>
    /// <returns>current row value</returns>
    unsigned int getrow() {
        return row;
    }

    /// <summary>
    /// properties to get/set row
    /// </summary>
    __declspec(property(get = getrow, put = putrow)) unsigned int Row;

    /// <summary>
    /// set column value
    /// </summary>
    /// <param name="_col">new column value</param>
    void putcol(unsigned int _col ) 
    { 
        col = _col;
    }

    /// <summary>
    /// get column value
    /// </summary>
    /// <returns>current column value</returns>
    unsigned int getcol() {
        return col;
    }

    /// <summary>
    /// properties to get/set column
    /// </summary>
    __declspec(property(get = getcol, put = putcol)) unsigned int Col;

}; // ButtonCoords

//--------------------------------------------------------------------------------------
// Button data
//--------------------------------------------------------------------------------------

/// <summary>
/// button pressed/released
/// </summary>
enum class ButtonState {up, down};

/// <summary>
/// screen and grid coordinates, outline color, id and state information
/// </summary>
struct ButtonData
{
    std::wstring label;
    LONG top, left, bottom, right;          // screen coordinates for outline
    ButtonCoords rcAddress;                 // grid row and column
    DirectX::SimpleMath::Vector4 currColor; // outline color    
    bool fValid;                            // false until button is first drawn/fully initialized
    ButtonState currState, lastState;       // button pressed/released
    unsigned int id;
}; // ButtonData

/// <summary>
/// Function type for the callback function to be "attached" to a button. The function
/// will become a member of the button object and will be called when the button is 
/// pressed.
/// </summary>
typedef std::function<void(ButtonData*)> ButtonFunc;

//--------------------------------------------------------------------------------------
// Button grid
//--------------------------------------------------------------------------------------

/// <summary>
/// representation of the matrix of buttons 
/// </summary>
class ButtonGrid
{
private:
    ButtonData ** m_pGrid;              // 2D array of button objects represented on the screen
    ButtonCoords m_activeButton;        // currently active/focused button
    unsigned int m_rowCount, m_colCount;       // number of rows and columns in the grid (may contain invalid buttons
    unsigned int m_cellWidth, m_cellHeight;    // max width and height for each button on the screen

    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>>            m_primBatch;
    std::unique_ptr<DirectX::SpriteBatch>               m_spriteBatch;
    std::unique_ptr<DirectX::SpriteFont>                m_font;
    DirectX::SimpleMath::Vector4                        m_fontDimensions;
    D3D11_RECT m_ScreenRect;

    ButtonGrid();
    // Get the screen coordinates for a button based on the size of its text
    D3D11_RECT ButtonGrid::GetButtonRect(LONG left, LONG top, const wchar_t* str);

    // Draw a rectangle on the screen
    void DrawBox( LONG x1, LONG y1, LONG x2, LONG y2, DirectX::FXMVECTOR vOutlineColor);

    // callback to be executed when a button is pressed.
    ButtonFunc DoButton;
    // Dpad navigation 
    ButtonCoords GetButtonRight();
    ButtonCoords GetButtonLeft();
    ButtonCoords GetButtonAbove();
    ButtonCoords GetButtonBelow();

    // property names for navigation
    __declspec(property(get = GetActiveButton)) ButtonData ActiveButton;
    __declspec(property(get = GetButtonRight)) ButtonCoords ButtonRight;
    __declspec(property(get = GetButtonLeft)) ButtonCoords ButtonLeft;
    __declspec(property(get = GetButtonAbove)) ButtonCoords ButtonAbove;
    __declspec(property(get = GetButtonBelow)) ButtonCoords ButtonBelow;

    // returns button data for currently active button
    ButtonData GetActiveButton() {return m_pGrid[m_activeButton.Row][m_activeButton.Col];}

    // returns button data for a button specified by its ID
//    ButtonData GetButtonByID(unsigned int id);

    // sets m_activeButton and changes the outline color 
    void FocusButton(ButtonCoords button);

    // draw and initialize a button--adds a button to a position on the grid specified by row and column
    void DrawButton(unsigned int row, unsigned int col);



public:
    static bool m_fIsInitialized;       // Used to ensure initialization occurs only once. The viewport isn't 
                                        // initialized until after the render loop begins, and we need that to
                                        // initialize the grid. When the render loop starts, we initialize the 
                                        // grid and set this to true so we don't reinitialize on the next pass.

    // render all valid buttons
    void DrawButtons();
    void ChangeButtonText(unsigned int id, std::wstring newText);
    // update current state and last state (pressed/released) of a button
    void OnWheelData(Windows::Xbox::Input::INavigationReading^ reading);
    void UpdateState(ButtonState state);    

    // constructor and destructor
    ButtonGrid(unsigned int rows, unsigned int cols, ButtonFunc callback, ID3D11DeviceX* d3ddevice, ID3D11DeviceContextX* deviceContext, D3D11_RECT screenRect);
    ~ButtonGrid();

    void SetButton(uint32_t id, uint8_t row, uint8_t col, std::wstring label);

private:
    ButtonGrid& operator=(const ButtonGrid&);
};
