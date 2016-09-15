//--------------------------------------------------------------------------------------
// File: ControllerHelp.cpp
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include "pch.h"
#include "ControllerHelp.h"
#include "DDSTextureLoader.h"
#include "SimpleMath.h"

#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP)
#include "FindMedia.h"
#endif

#include <stdexcept>

using namespace DirectX;
using namespace SimpleMath;

namespace
{
#if defined(__d3d12_h__) || defined(__d3d12_x_h__)
    enum Descriptors
    {
        Segoe18 = 0,
        Segoe22,
        Segoe36,
        CircleTex,
        GamepadTex,
        BackgroundTex,
        Count,
    };
#endif

    enum HelpFonts
    {
        SEGOE_UI_18PT = 0,
        SEGOE_UI_22PT,
        SEGOE_UI_36PT,
    };

    enum class CalloutType : uint16_t
    {
        NO_CONTAINER,
        LINE_TO_ANCHOR
    };

    enum Alignment
    {
        TO_LEFT = 1,
        TO_RIGHT = 2,
        HORIZONTAL_CENTER = TO_LEFT | TO_RIGHT,
        ABOVE = 4,
        BELOW = 8,
        VERTICAL_CENTER = ABOVE | BELOW,
        FULL_CENTER = TO_LEFT | TO_RIGHT | ABOVE | BELOW
    };

    // Anchor points for each of the callout boxes.
    const Vector2 ANCHOR_TITLE = { 960, 92 };
    const Vector2 ANCHOR_DESCRIPTION = { 960, 990 };
    const Vector2 ANCHOR_LEFT_STICK = { 513, 468 };
    const Vector2 ANCHOR_RIGHT_STICK = { 1405, 599 };
    const Vector2 ANCHOR_LEFT_STICK_CLICK = { 513, 526 };
    const Vector2 ANCHOR_RIGHT_STICK_CLICK = { 1405, 657 };
    const Vector2 ANCHOR_DPAD_ALL = { 513, 605 };
    const Vector2 ANCHOR_RIGHT_SHOULDER = { 1405, 230 };
    const Vector2 ANCHOR_RIGHT_TRIGGER = { 1405, 300 };
    const Vector2 ANCHOR_LEFT_SHOULDER = { 513, 230 };
    const Vector2 ANCHOR_LEFT_TRIGGER = { 513, 300 };
    const Vector2 ANCHOR_A_BUTTON = { 1405, 538 };
    const Vector2 ANCHOR_B_BUTTON = { 1405, 488 };
    const Vector2 ANCHOR_X_BUTTON = { 1405, 389 };
    const Vector2 ANCHOR_Y_BUTTON = { 1405, 440 };
    const Vector2 ANCHOR_MENU = { 1405, 785 };
    const Vector2 ANCHOR_VIEW = { 513, 785 };

    // Anchor points for the mid-point of each of the callout balloons
    const Vector2 MIDPOINT_LINE_LEFT_STICK_CLICK = { 774, 526 };
    const Vector2 MIDPOINT_LINE_RIGHT_STICK_CLICK = { 1054, 657 };
    const Vector2 MIDPOINT_LINE_RIGHT_SHOULDER = { 1097, 230 };
    const Vector2 MIDPOINT_LINE_RIGHT_TRIGGER = { 1171, 300 };
    const Vector2 MIDPOINT_LINE_LEFT_SHOULDER = { 815, 230 };
    const Vector2 MIDPOINT_LINE_LEFT_TRIGGER = { 751, 300 };
    const Vector2 MIDPOINT_LINE_X_BUTTON = { 1097, 389 };
    const Vector2 MIDPOINT_LINE_MENU = { 1012, 785 };
    const Vector2 MIDPOINT_LINE_VIEW = { 907, 785 };

    // Anchor points for each of the callout balloons (what each callout is referring to).
    const Vector2 CALLOUT_LINE_LEFT_STICK = { 740, 468 };
    const Vector2 CALLOUT_LINE_RIGHT_STICK = { 1091, 599 };
    const Vector2 CALLOUT_LINE_LEFT_STICK_CLICK = { 774, 487 };
    const Vector2 CALLOUT_LINE_RIGHT_STICK_CLICK = { 1054, 599 };
    const Vector2 CALLOUT_LINE_DPAD_ALL = { 815, 605 };
    const Vector2 CALLOUT_LINE_RIGHT_SHOULDER = { 1097, 342 };
    const Vector2 CALLOUT_LINE_RIGHT_TRIGGER = { 1171, 336 };
    const Vector2 CALLOUT_LINE_LEFT_SHOULDER = { 815, 342 };
    const Vector2 CALLOUT_LINE_LEFT_TRIGGER = { 751, 336 };
    const Vector2 CALLOUT_LINE_A_BUTTON = { 1168, 538 };
    const Vector2 CALLOUT_LINE_B_BUTTON = { 1215, 488 };
    const Vector2 CALLOUT_LINE_X_BUTTON = { 1097, 465 };
    const Vector2 CALLOUT_LINE_Y_BUTTON = { 1168, 440 };
    const Vector2 CALLOUT_LINE_MENU = { 1012, 503 };
    const Vector2 CALLOUT_LINE_VIEW = { 907, 503 };

    // Description Text for each button
    const wchar_t* LABEL_TEXT_LEFT_STICK = L"LEFT THUMBSTICK";
    const wchar_t* LABEL_TEXT_RIGHT_STICK = L"RIGHT THUMBSTICK";
    const wchar_t* LABEL_TEXT_LEFT_STICK_CLICK = L"(click) LEFT THUMBSTICK";
    const wchar_t* LABEL_TEXT_RIGHT_STICK_CLICK = L"(click) RIGHT THUMBSTICK";
    const wchar_t* PRE_TEXT_DPAD_UP = L"UP - ";
    const wchar_t* PRE_TEXT_DPAD_DOWN = L"DOWN - ";
    const wchar_t* PRE_TEXT_DPAD_LEFT = L"LEFT - ";
    const wchar_t* PRE_TEXT_DPAD_RIGHT = L"RIGHT - ";
    const wchar_t* LABEL_TEXT_DPAD_ALL = L"DPAD";
    const wchar_t* LABEL_TEXT_RIGHT_SHOULDER = L"RIGHT BUMPER";
    const wchar_t* LABEL_TEXT_RIGHT_TRIGGER = L"RIGHT TRIGGER";
    const wchar_t* LABEL_TEXT_LEFT_SHOULDER = L"LEFT BUMPER";
    const wchar_t* LABEL_TEXT_LEFT_TRIGGER = L"LEFT TRIGGER";
    const wchar_t* LABEL_TEXT_LINE_MENU = L"MENU";
    const wchar_t* LABEL_TEXT_LINE_VIEW = L"VIEW";
    const wchar_t* PRE_TEXT_A_BUTTON = L"A ";
    const wchar_t* PRE_TEXT_B_BUTTON = L"B ";
    const wchar_t* PRE_TEXT_X_BUTTON = L"X ";
    const wchar_t* PRE_TEXT_Y_BUTTON = L"Y ";

    const DirectX::SimpleMath::Vector4 HELP_TITLE_COLOR = { 0.478431374f, 0.478431374f, 0.478431374f, 1.f };
    const DirectX::SimpleMath::Vector4 HELP_DESCRIPTION_COLOR = { 0.478431374f, 0.478431374f, 0.478431374f, 1.f };
    const DirectX::SimpleMath::Vector4 HELP_CALLOUT_COLOR = { 0.980392158f, 0.980392158f, 0.980392158f, 1.f };
    const DirectX::SimpleMath::Vector4 HELP_CALLOUT_LABEL_COLOR = { 0.478431374f, 0.478431374f, 0.478431374f, 1.f };
    const DirectX::SimpleMath::Vector4 PRE_TEXT_A_BUTTON_COLOR = { 0.2f, 0.6f, 0.09f, 1.f };
    const DirectX::SimpleMath::Vector4 PRE_TEXT_B_BUTTON_COLOR = { 0.9f, 0.1f, 0.14f, 1.f };
    const DirectX::SimpleMath::Vector4 PRE_TEXT_X_BUTTON_COLOR = { 0.f, 0.5f, 0.7f, 1.f };
    const DirectX::SimpleMath::Vector4 PRE_TEXT_Y_BUTTON_COLOR = { 0.9f, 0.8f, 0.f, 1.f };
};

struct ATG::Help::CalloutBox
{
    ATG::HelpID helpID;         // The HelpID for the callout
    CalloutType type;			// The type of callout to render.
    uint16_t align;				// Alignment relative to anchor point.
    HelpFonts font;				// font to render text in.
    const wchar_t * labelPre;	// Label pre-text
    const wchar_t * label;		// Label text
    Vector4 labelForeground;	// Label Foreground
    Vector4 foreground;			// Foreground color
    Vector2 anchor;				// the anchor point for this callout.
    const wchar_t * text;		// text for the callout.
    Vector2 calloutLine;		// position to draw the line to on the callout from the anchor (if shown).
    Vector2 midpointLine;       // the midpoint of the callout and anchor lines

    static void Create(_Out_ CalloutBox& dest, _In_opt_z_ const wchar_t* text, ATG::HelpID helpID, bool linearColors)
    {
        size_t j = 0;
        for (; j < ATG::HelpID::MAX_COUNT; ++j)
        {
            if (s_CalloutTemplates[j].helpID == helpID)
            {
                dest = s_CalloutTemplates[j];
                if (linearColors)
                {
                    dest.labelForeground = XMColorSRGBToRGB(s_CalloutTemplates[j].labelForeground);
                    dest.foreground = XMColorSRGBToRGB(s_CalloutTemplates[j].foreground);
                }
                break;
            }
        }
        if (j >= ATG::HelpID::MAX_COUNT)
        {
            throw std::exception("CalloutBox::Create can't find help ID");
        }

        dest.text = text;
    }

    void Render(const ATG::Help& help, SpriteBatch* batch)
    {
        // add the callout circle to the spritebatch.  
        if (type == CalloutType::LINE_TO_ANCHOR)
        {
            // callout circle is 12x12 so -6 from x and y to get top left coordinates
#if defined(__d3d12_h__) || defined(__d3d12_x_h__)
            batch->Draw(help.m_descriptorHeap->GetGpuHandle(Descriptors::CircleTex), help.m_circleTexSize,
                Vector2(calloutLine.x - 6, calloutLine.y - 6));
#elif defined(__d3d11_h__) || defined(__d3d11_x_h__)
            batch->Draw(help.m_circleTex.Get(), Vector2(calloutLine.x - 6, calloutLine.y - 6));
#endif
        }

        SpriteFont* spriteFont = help.m_spriteFonts[font].get();

        // Draw callout text
        LONG anchorXOffset = 20;  // space between callout line and label/text
        LONG preLabelXOffset = 6;  // space between a pre-label and text

        if (labelPre)
        {
            // Some labels have a pre-text before the text such as the A,B,X,Y, and DPAD buttons
            Vector2 textSize = spriteFont->MeasureString(text);
            Vector2 labelPreSize = spriteFont->MeasureString(labelPre);

            if (Alignment::TO_RIGHT == align)
            {
                // draw pre-text
                spriteFont->DrawString(batch, labelPre, Vector2((anchor.x - (anchorXOffset + labelPreSize.x + textSize.x + preLabelXOffset)),
                    (anchor.y - (labelPreSize.y / 2))), labelForeground);
                // draw text
                spriteFont->DrawString(batch, text, Vector2((anchor.x - anchorXOffset) - (textSize.x),
                    (anchor.y - (textSize.y / 2))), foreground);
            }
            else if (Alignment::TO_LEFT == align)
            {
                // draw pre-text
                spriteFont->DrawString(batch, labelPre, Vector2((anchor.x + anchorXOffset),
                    (anchor.y - (labelPreSize.y / 2))), labelForeground);

                // draw text
                spriteFont->DrawString(batch, text, Vector2((anchor.x + anchorXOffset) + (labelPreSize.x + preLabelXOffset),
                    (anchor.y - (textSize.y / 2))), foreground);
            }
        }
        else // no pre-text
        {
            if (Alignment::TO_RIGHT == align)  // align to right
            {
                if (label)
                {
                    Vector2 labelSize = help.m_spriteFonts[font]->MeasureString(label);
                    spriteFont->DrawString(batch, label, Vector2((anchor.x - anchorXOffset) - labelSize.x,
                        (anchor.y - (labelSize.y / 2))), labelForeground);
                }

                if (text)
                {
                    Vector2 textSize = help.m_spriteFonts[font]->MeasureString(text);
                    spriteFont->DrawString(batch, text, Vector2((anchor.x - anchorXOffset) - textSize.x,
                        ((anchor.y) + (textSize.y / 3))), foreground);
                }
            }
            else if (Alignment::TO_LEFT == align) // align to left
            {
                if (label)
                {
                    Vector2 labelSize = help.m_spriteFonts[font]->MeasureString(label);
                    spriteFont->DrawString(batch, label, Vector2((anchor.x + anchorXOffset),
                        (anchor.y - (labelSize.y / 2))), labelForeground);
                }

                if (text)
                {
                    Vector2 textSize = help.m_spriteFonts[font]->MeasureString(text);
                    spriteFont->DrawString(batch, text, Vector2((anchor.x + anchorXOffset),
                        ((anchor.y) + (textSize.y / 3))), foreground);
                }
            }
            else if (Alignment::FULL_CENTER == align)  // center
            {
                if (text)
                {
                    Vector2 textSize = help.m_spriteFonts[font]->MeasureString(text);
                    spriteFont->DrawString(batch, text, Vector2((anchor.x - (textSize.x / 2)),
                        ((anchor.y) - (textSize.y / 2))), foreground);
                }
            }
        }
    }

    void Render(PrimitiveBatch<VertexPositionColor>* batch)
    {
        // draw callout lines
        if (type == CalloutType::LINE_TO_ANCHOR)
        {
            XMVECTOR color = labelForeground;

            if (midpointLine.x > 0) // two lines
            {
                // callout to midpoint
                batch->DrawLine(VertexPositionColor(Vector2(anchor.x, anchor.y), color),
                    VertexPositionColor(Vector2(midpointLine.x, midpointLine.y), color));

                // midpoint to anchor
                batch->DrawLine(
                    VertexPositionColor(Vector2(midpointLine.x, midpointLine.y), color),
                    VertexPositionColor(Vector2(calloutLine.x, calloutLine.y), color));
            }
            else
            {
                // single line.  no midpoint.  callout to anchor.
                batch->DrawLine(VertexPositionColor(Vector2(anchor.x, anchor.y), color),
                    VertexPositionColor(Vector2(calloutLine.x, calloutLine.y), color));
            }
        }
    }

private:
    static const CalloutBox s_CalloutTemplates[];
};

const ATG::Help::CalloutBox ATG::Help::CalloutBox::s_CalloutTemplates[] =
{
    // TITLE_TEXT
    {
        ATG::HelpID::TITLE_TEXT,
        CalloutType::NO_CONTAINER,
        FULL_CENTER,
        SEGOE_UI_36PT,
        nullptr,
        nullptr,
        HELP_CALLOUT_LABEL_COLOR,
        HELP_TITLE_COLOR,
        ANCHOR_TITLE,
        nullptr,
        ANCHOR_TITLE,
        { 0, 0 }
    },
    // DESCRIPTION_TEXT
    {
        ATG::HelpID::DESCRIPTION_TEXT,
        CalloutType::NO_CONTAINER,
        FULL_CENTER,
        SEGOE_UI_22PT,
        nullptr,
        nullptr,
        HELP_CALLOUT_LABEL_COLOR,
        HELP_DESCRIPTION_COLOR,
        ANCHOR_DESCRIPTION,
        nullptr,
        ANCHOR_DESCRIPTION,
        { 0, 0 }
    },
    // LEFT_STICK,
    {
        ATG::HelpID::LEFT_STICK,
        CalloutType::LINE_TO_ANCHOR,
        TO_RIGHT,
        SEGOE_UI_18PT,
        nullptr,
        LABEL_TEXT_LEFT_STICK,
        HELP_CALLOUT_LABEL_COLOR,
        HELP_CALLOUT_COLOR,
        ANCHOR_LEFT_STICK,
        nullptr,
        CALLOUT_LINE_LEFT_STICK,
        { 0, 0 }
    },
    // LEFT_STICK_CLICK,
    {
        ATG::HelpID::LEFT_STICK_CLICK,
        CalloutType::LINE_TO_ANCHOR,
        TO_RIGHT,
        SEGOE_UI_18PT,
        nullptr,
        LABEL_TEXT_LEFT_STICK_CLICK,
        HELP_CALLOUT_LABEL_COLOR,
        HELP_CALLOUT_COLOR,
        ANCHOR_LEFT_STICK_CLICK,
        nullptr,
        CALLOUT_LINE_LEFT_STICK_CLICK,
        MIDPOINT_LINE_LEFT_STICK_CLICK
    },
    // RIGHT_STICK
    {
        ATG::HelpID::RIGHT_STICK,
        CalloutType::LINE_TO_ANCHOR,
        TO_LEFT,
        SEGOE_UI_18PT,
        nullptr,
        LABEL_TEXT_RIGHT_STICK,
        HELP_CALLOUT_LABEL_COLOR,
        HELP_CALLOUT_COLOR,
        ANCHOR_RIGHT_STICK,
        nullptr,
        CALLOUT_LINE_RIGHT_STICK,
        { 0, 0 }
    },
    // RIGHT_STICK_CLICK
    {
        ATG::HelpID::RIGHT_STICK_CLICK,
        CalloutType::LINE_TO_ANCHOR,
        TO_LEFT,
        SEGOE_UI_18PT,
        nullptr,
        LABEL_TEXT_RIGHT_STICK_CLICK,
        HELP_CALLOUT_LABEL_COLOR,
        HELP_CALLOUT_COLOR,
        ANCHOR_RIGHT_STICK_CLICK,
        nullptr,
        CALLOUT_LINE_RIGHT_STICK_CLICK,
        MIDPOINT_LINE_RIGHT_STICK_CLICK
    },
    // DPAD_UP
    {
        ATG::HelpID::DPAD_UP,
        CalloutType::NO_CONTAINER,
        TO_RIGHT,
        SEGOE_UI_18PT,
        PRE_TEXT_DPAD_UP,
        nullptr,
        HELP_CALLOUT_COLOR,
        HELP_CALLOUT_COLOR,
        ANCHOR_DPAD_ALL,
        nullptr,
        CALLOUT_LINE_DPAD_ALL
    },
    // DPAD_DOWN
    {
        ATG::HelpID::DPAD_DOWN,
        CalloutType::NO_CONTAINER,
        TO_RIGHT,
        SEGOE_UI_18PT,
        PRE_TEXT_DPAD_DOWN,
        nullptr,
        HELP_CALLOUT_COLOR,
        HELP_CALLOUT_COLOR,
        ANCHOR_DPAD_ALL,
        nullptr,
        CALLOUT_LINE_DPAD_ALL
    },
    // DPAD_LEFT
    {
        ATG::HelpID::DPAD_LEFT,
        CalloutType::NO_CONTAINER,
        TO_RIGHT,
        SEGOE_UI_18PT,
        PRE_TEXT_DPAD_LEFT,
        nullptr,
        HELP_CALLOUT_COLOR,
        HELP_CALLOUT_COLOR,
        ANCHOR_DPAD_ALL,
        nullptr,
        CALLOUT_LINE_DPAD_ALL
    },
    // DPAD_RIGHT
    {
        ATG::HelpID::DPAD_RIGHT,
        CalloutType::NO_CONTAINER,
        TO_RIGHT,
        SEGOE_UI_18PT,
        PRE_TEXT_DPAD_RIGHT,
        nullptr,
        HELP_CALLOUT_COLOR,
        HELP_CALLOUT_COLOR,
        ANCHOR_DPAD_ALL,
        nullptr,
        CALLOUT_LINE_DPAD_ALL
    },
    // DPAD_ALL
    {
        ATG::HelpID::DPAD_ALL,
        CalloutType::LINE_TO_ANCHOR,
        TO_RIGHT,
        SEGOE_UI_18PT,
        nullptr,
        LABEL_TEXT_DPAD_ALL,
        HELP_CALLOUT_LABEL_COLOR,
        HELP_CALLOUT_COLOR,
        ANCHOR_DPAD_ALL,
        nullptr,
        CALLOUT_LINE_DPAD_ALL,
        { 0, 0 }
    },
    // RIGHT_SHOULDER
    {
        ATG::HelpID::RIGHT_SHOULDER,
        CalloutType::LINE_TO_ANCHOR,
        TO_LEFT,
        SEGOE_UI_18PT,
        nullptr,
        LABEL_TEXT_RIGHT_SHOULDER,
        HELP_CALLOUT_LABEL_COLOR,
        HELP_CALLOUT_COLOR,
        ANCHOR_RIGHT_SHOULDER,
        nullptr,
        CALLOUT_LINE_RIGHT_SHOULDER,
        MIDPOINT_LINE_RIGHT_SHOULDER
    },
    // RIGHT_TRIGGER
    {
        ATG::HelpID::RIGHT_TRIGGER,
        CalloutType::LINE_TO_ANCHOR,
        TO_LEFT,
        SEGOE_UI_18PT,
        nullptr,
        LABEL_TEXT_RIGHT_TRIGGER,
        HELP_CALLOUT_LABEL_COLOR,
        HELP_CALLOUT_COLOR,
        ANCHOR_RIGHT_TRIGGER,
        nullptr,
        CALLOUT_LINE_RIGHT_TRIGGER,
        MIDPOINT_LINE_RIGHT_TRIGGER
    },
    // LEFT_SHOULDER
    {
        ATG::HelpID::LEFT_SHOULDER,
        CalloutType::LINE_TO_ANCHOR,
        TO_RIGHT,
        SEGOE_UI_18PT,
        nullptr,
        LABEL_TEXT_LEFT_SHOULDER,
        HELP_CALLOUT_LABEL_COLOR,
        HELP_CALLOUT_COLOR,
        ANCHOR_LEFT_SHOULDER,
        nullptr,
        CALLOUT_LINE_LEFT_SHOULDER,
        MIDPOINT_LINE_LEFT_SHOULDER
    },
    // LEFT_TRIGGER
    {
        ATG::HelpID::LEFT_TRIGGER,
        CalloutType::LINE_TO_ANCHOR,
        TO_RIGHT,
        SEGOE_UI_18PT,
        nullptr,
        LABEL_TEXT_LEFT_TRIGGER,
        HELP_CALLOUT_LABEL_COLOR,
        HELP_CALLOUT_COLOR,
        ANCHOR_LEFT_TRIGGER,
        nullptr,
        CALLOUT_LINE_LEFT_TRIGGER,
        MIDPOINT_LINE_LEFT_TRIGGER
    },
    // A_BUTTON
    {
        ATG::HelpID::A_BUTTON,
        CalloutType::LINE_TO_ANCHOR,
        TO_LEFT,
        SEGOE_UI_18PT,
        PRE_TEXT_A_BUTTON,
        nullptr,
        PRE_TEXT_A_BUTTON_COLOR,
        HELP_CALLOUT_COLOR,
        ANCHOR_A_BUTTON,
        nullptr,
        CALLOUT_LINE_A_BUTTON,
        { 0, 0 }
    },
    // B_BUTTON
    {
        ATG::HelpID::B_BUTTON,
        CalloutType::LINE_TO_ANCHOR,
        TO_LEFT,
        SEGOE_UI_18PT,
        PRE_TEXT_B_BUTTON,
        nullptr,
        PRE_TEXT_B_BUTTON_COLOR,
        HELP_CALLOUT_COLOR,
        ANCHOR_B_BUTTON,
        nullptr,
        CALLOUT_LINE_B_BUTTON,
        { 0, 0 }
    },
    // X_BUTTON
    {
        ATG::HelpID::X_BUTTON,
        CalloutType::LINE_TO_ANCHOR,
        TO_LEFT,
        SEGOE_UI_18PT,
        PRE_TEXT_X_BUTTON,
        nullptr,
        PRE_TEXT_X_BUTTON_COLOR,
        HELP_CALLOUT_COLOR,
        ANCHOR_X_BUTTON,
        nullptr,
        CALLOUT_LINE_X_BUTTON,
        MIDPOINT_LINE_X_BUTTON
    },
    // Y_BUTTON
    {
        ATG::HelpID::Y_BUTTON,
        CalloutType::LINE_TO_ANCHOR,
        TO_LEFT,
        SEGOE_UI_18PT,
        PRE_TEXT_Y_BUTTON,
        nullptr,
        PRE_TEXT_Y_BUTTON_COLOR,
        HELP_CALLOUT_COLOR,
        ANCHOR_Y_BUTTON,
        nullptr,
        CALLOUT_LINE_Y_BUTTON,
        { 0, 0 }
    },
    // MENU_BUTTON (Same as start)
    {
        ATG::HelpID::MENU_BUTTON,
        CalloutType::LINE_TO_ANCHOR,
        TO_LEFT,
        SEGOE_UI_18PT,
        nullptr,
        LABEL_TEXT_LINE_MENU,
        HELP_CALLOUT_LABEL_COLOR,
        HELP_CALLOUT_COLOR,
        ANCHOR_MENU,
        nullptr,
        CALLOUT_LINE_MENU,
        MIDPOINT_LINE_MENU
    },
    // VIEW_BUTTON 
    {
        ATG::HelpID::VIEW_BUTTON,
        CalloutType::LINE_TO_ANCHOR,
        TO_RIGHT,
        SEGOE_UI_18PT,
        nullptr,
        LABEL_TEXT_LINE_VIEW,
        HELP_CALLOUT_LABEL_COLOR,
        HELP_CALLOUT_COLOR,
        ANCHOR_VIEW,
        nullptr,
        CALLOUT_LINE_VIEW,
        MIDPOINT_LINE_VIEW
    }
};

_Use_decl_annotations_
ATG::Help::Help(const wchar_t* title, const wchar_t* description, const HelpButtonAssignment* buttons, size_t buttonCount, bool linearColors) :
    m_linearColors(linearColors),
    m_calloutCount(0),
    m_callouts(nullptr)
{
    if (!buttons)
    {
        throw std::invalid_argument("buttons must be non null");
    }

    if (!buttonCount)
    {
        throw std::out_of_range("buttonCount is 0!");
    }

    m_calloutCount = buttonCount;

    if (title)
    {
        ++m_calloutCount;
    }

    if (description)
    {
        ++m_calloutCount;
    }

    bool dpadLabels = true;
    float dpadYOffset[4] = {0.f, 0.f, 0.f, 0.f};

    static const float c_fontSize18 = 31.9218750f;

    for (size_t j = 0; j < buttonCount; ++j)
    {
        switch (buttons[j].id)
        {
        case HelpID::DPAD_UP:
            dpadYOffset[0] = c_fontSize18;
            dpadLabels = true;
            break;

        case HelpID::DPAD_DOWN:
            dpadYOffset[1] = c_fontSize18;
            dpadLabels = true;
            break;

        case HelpID::DPAD_RIGHT:
            dpadYOffset[2] = c_fontSize18;
            dpadLabels = true;
            break;

        case HelpID::DPAD_LEFT:
            dpadYOffset[3] = c_fontSize18;
            dpadLabels = true;
            break;
        }
    }

    if (dpadLabels)
    {
        ++m_calloutCount;
    }

    m_callouts = new CalloutBox[m_calloutCount];

    size_t index = 0;
    
    if (title)
    {
        CalloutBox::Create(m_callouts[index++], title, ATG::HelpID::TITLE_TEXT, m_linearColors);
    }

    if (description)
    {
        CalloutBox::Create(m_callouts[index++], description, ATG::HelpID::DESCRIPTION_TEXT, m_linearColors);
    }

    if (dpadLabels)
    {
        CalloutBox::Create(m_callouts[index++], nullptr, ATG::HelpID::DPAD_ALL, m_linearColors);
    }

    for (size_t j = 0; j < buttonCount; ++j)
    {
        CalloutBox::Create(m_callouts[index++], buttons[j].buttonText, buttons[j].id, m_linearColors);

        // DPAD buttons should appear in the order of UP, DOWN, LEFT, RIGHT.  In the event that not all buttons
        // are used, they still need to appear in order.  Apply an offset to each DPAD item using the dpadButtonsYOffset value
        // for the button used.

        switch (buttons[j].id)
        {
        case HelpID::DPAD_UP:
            m_callouts[index - 1].anchor.y += (LONG)dpadYOffset[0];
            break;

        case HelpID::DPAD_DOWN:
            m_callouts[index - 1].anchor.y += (LONG)(dpadYOffset[0] + dpadYOffset[1]);
            break;

        case HelpID::DPAD_RIGHT:
            m_callouts[index - 1].anchor.y += (LONG)(dpadYOffset[0] + dpadYOffset[1] + dpadYOffset[2]);
            break;

        case HelpID::DPAD_LEFT:
            m_callouts[index - 1].anchor.y += (LONG)(dpadYOffset[0] + dpadYOffset[1] + dpadYOffset[2] + dpadYOffset[3]);
            break;

        case HelpID::DPAD_ALL:
            throw std::exception("Do not use DPAD_ALL in help array");
            break;
        }
    }
}

ATG::Help::~Help()
{
    if (m_callouts)
    {
        delete[] m_callouts;
        m_callouts = nullptr;
    }
}

#if defined(__d3d12_h__) || defined(__d3d12_x_h__)
void ATG::Help::Render(ID3D12GraphicsCommandList* commandList)
{
    // Set the descriptor heaps
    ID3D12DescriptorHeap* descriptorHeaps[] =
    {
        m_descriptorHeap->Heap()
    };
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    D3D12_VIEWPORT vp1{ 0.0f, 0.0f, static_cast<float>(m_screenSize.right), static_cast<float>(m_screenSize.bottom),
        D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
    
    m_spriteBatch->SetViewport(vp1);
    m_spriteBatch->Begin(commandList, SpriteSortMode_Immediate);

    // Draw background image
    m_spriteBatch->Draw(m_descriptorHeap->GetGpuHandle(Descriptors::BackgroundTex), m_backgroundTexSize, m_screenSize);

    // Draw gamepad controller
    m_spriteBatch->Draw(m_descriptorHeap->GetGpuHandle(Descriptors::GamepadTex), m_gamepadTexSize, m_screenSize);

    m_spriteBatch->End();

    D3D12_VIEWPORT vp2{ 0.0f, 0.f, 1920.f, 1080.f, D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
    m_spriteBatch->SetViewport(vp2);
    m_spriteBatch->Begin(commandList, SpriteSortMode_Deferred);

    // Process sprites
    for (size_t j = 0; j < m_calloutCount; ++j)
    {
        m_callouts[j].Render(*this, m_spriteBatch.get());
    }

    m_spriteBatch->End();

    XMMATRIX proj = XMMatrixOrthographicOffCenterRH(0.f, 1920.f, 1080.f, 0.f, 0.f, 1.f);
    m_lineEffect->SetProjection(proj);
    
    m_lineEffect->Apply(commandList);

    m_primBatch->Begin(commandList);

    for (size_t j = 0; j < m_calloutCount; ++j)
    {
        m_callouts[j].Render(m_primBatch.get());
    }

    m_primBatch->End();
}
#elif defined(__d3d11_h__) || defined(__d3d11_x_h__)
void ATG::Help::Render()
{
    CD3D11_VIEWPORT vp1(0.0f, 0.0f, static_cast<float>(m_screenSize.right), static_cast<float>(m_screenSize.bottom));
    m_spriteBatch->SetViewport(vp1);
    m_spriteBatch->Begin(SpriteSortMode_Immediate, m_states->AlphaBlend());

    // Draw background image
    m_spriteBatch->Draw(m_backgroundTex.Get(), m_screenSize);

    // Draw gamepad controller
    m_spriteBatch->Draw(m_gamepadTex.Get(), m_screenSize);

    m_spriteBatch->End();

    CD3D11_VIEWPORT vp2(0.0f, 0.f, 1920.f, 1080.f);
    m_spriteBatch->SetViewport(vp2);
    m_spriteBatch->Begin(SpriteSortMode_Deferred, m_states->AlphaBlend());

    // Process sprites
    for (size_t j = 0; j < m_calloutCount; ++j)
    {
        m_callouts[j].Render(*this, m_spriteBatch.get());
    }

    m_spriteBatch->End();

    XMMATRIX proj = XMMatrixOrthographicOffCenterRH(0.f, 1920.f, 1080.f, 0.f, 0.f, 1.f);
    m_lineEffect->SetProjection(proj);

    m_context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
    m_context->OMSetDepthStencilState(m_states->DepthNone(), 0);
    m_context->RSSetState(m_states->CullNone());

    m_lineEffect->Apply(m_context.Get());
    m_context->IASetInputLayout(m_lineLayout.Get());

    m_primBatch->Begin();

    for (size_t j = 0; j < m_calloutCount; ++j)
    {
        m_callouts[j].Render(m_primBatch.get());
    }

    m_primBatch->End();
}
#endif

void ATG::Help::ReleaseDevice()
{
    m_spriteBatch.reset();
    m_primBatch.reset();
    m_lineEffect.reset();

    for (int i = 0; i < _countof(m_spriteFonts); ++i)
    {
        m_spriteFonts[i].reset();
    }

    m_circleTex.Reset();
    m_gamepadTex.Reset();
    m_backgroundTex.Reset();

#if defined(__d3d12_h__) || defined(__d3d12_x_h__)
    m_descriptorHeap.reset();
#elif defined(__d3d11_h__) || defined(__d3d11_x_h__)
    m_states.reset();
    m_lineLayout.Reset();
    m_context.Reset();
#endif
}

#if defined(__d3d12_h__) || defined(__d3d12_x_h__)
void ATG::Help::RestoreDevice(ID3D12Device* device, ResourceUploadBatch& uploadBatch, const RenderTargetState& rtState)
{
    m_descriptorHeap = std::make_unique<DescriptorHeap>(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, Descriptors::Count);
    
    SpriteBatchPipelineStateDescription sbPsoDesc(rtState, &CommonStates::AlphaBlend);    
    m_spriteBatch = std::make_unique<SpriteBatch>(device, uploadBatch, sbPsoDesc);

    m_primBatch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(device);
        
    EffectPipelineStateDescription fxPsoDesc(&VertexPositionColor::InputLayout, CommonStates::Opaque,
        CommonStates::DepthNone, CommonStates::CullNone, rtState, D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
    m_lineEffect = std::make_unique<BasicEffect>(device, EffectFlags::VertexColor, fxPsoDesc);

#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP)
    wchar_t buff[MAX_PATH];
    DX::FindMediaFile(buff, MAX_PATH, L"Media//Fonts//SegoeUI_18.spritefont");
    m_spriteFonts[SEGOE_UI_18PT] = std::make_unique<SpriteFont>(device, uploadBatch, buff, m_descriptorHeap->GetCpuHandle(Descriptors::Segoe18), m_descriptorHeap->GetGpuHandle(Descriptors::Segoe18));

    DX::FindMediaFile(buff, MAX_PATH, L"Media//Fonts//SegoeUI_22.spritefont");
    m_spriteFonts[SEGOE_UI_22PT] = std::make_unique<SpriteFont>(device, uploadBatch, buff, m_descriptorHeap->GetCpuHandle(Descriptors::Segoe22), m_descriptorHeap->GetGpuHandle(Descriptors::Segoe22));

    DX::FindMediaFile(buff, MAX_PATH, L"Media//Fonts//SegoeUI_36.spritefont");
    m_spriteFonts[SEGOE_UI_36PT] = std::make_unique<SpriteFont>(device, uploadBatch, buff, m_descriptorHeap->GetCpuHandle(Descriptors::Segoe36), m_descriptorHeap->GetGpuHandle(Descriptors::Segoe36));

    DX::FindMediaFile(buff, MAX_PATH, L"Media//Textures//callout_circle.dds");
    DX::ThrowIfFailed(CreateDDSTextureFromFileEx(device, uploadBatch, buff, 0, D3D12_RESOURCE_FLAG_NONE, m_linearColors, false, m_circleTex.ReleaseAndGetAddressOf()));

    DX::FindMediaFile(buff, MAX_PATH, L"Media//Textures//gamepad.dds");
    DX::ThrowIfFailed(CreateDDSTextureFromFileEx(device, uploadBatch, buff, 0, D3D12_RESOURCE_FLAG_NONE, m_linearColors, false, m_gamepadTex.ReleaseAndGetAddressOf()));

    DX::FindMediaFile(buff, MAX_PATH, L"Media//Textures//ATGSampleBackground.DDS");
    DX::ThrowIfFailed(CreateDDSTextureFromFileEx(device, uploadBatch, buff, 0, D3D12_RESOURCE_FLAG_NONE, m_linearColors, false, m_backgroundTex.ReleaseAndGetAddressOf()));
#else
    m_spriteFonts[SEGOE_UI_18PT] = std::make_unique<SpriteFont>(device, uploadBatch, L"SegoeUI_18.spritefont", m_descriptorHeap->GetCpuHandle(Descriptors::Segoe18), m_descriptorHeap->GetGpuHandle(Descriptors::Segoe18));
    m_spriteFonts[SEGOE_UI_22PT] = std::make_unique<SpriteFont>(device, uploadBatch, L"SegoeUI_22.spritefont", m_descriptorHeap->GetCpuHandle(Descriptors::Segoe22), m_descriptorHeap->GetGpuHandle(Descriptors::Segoe22));
    m_spriteFonts[SEGOE_UI_36PT] = std::make_unique<SpriteFont>(device, uploadBatch, L"SegoeUI_36.spritefont", m_descriptorHeap->GetCpuHandle(Descriptors::Segoe36), m_descriptorHeap->GetGpuHandle(Descriptors::Segoe36));

    DX::ThrowIfFailed(CreateDDSTextureFromFileEx(device, uploadBatch, L"callout_circle.dds", 0, D3D12_RESOURCE_FLAG_NONE, m_linearColors, false, m_circleTex.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(CreateDDSTextureFromFileEx(device, uploadBatch, L"gamepad.dds", 0, D3D12_RESOURCE_FLAG_NONE, m_linearColors, false, m_gamepadTex.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(CreateDDSTextureFromFileEx(device, uploadBatch, L"ATGSampleBackground.DDS", 0, D3D12_RESOURCE_FLAG_NONE, m_linearColors, false, m_backgroundTex.ReleaseAndGetAddressOf()));
#endif

    DirectX::CreateShaderResourceView(device, m_circleTex.Get(), m_descriptorHeap->GetCpuHandle(Descriptors::CircleTex), false);
    DirectX::CreateShaderResourceView(device, m_gamepadTex.Get(), m_descriptorHeap->GetCpuHandle(Descriptors::GamepadTex), false);
    DirectX::CreateShaderResourceView(device, m_backgroundTex.Get(), m_descriptorHeap->GetCpuHandle(Descriptors::BackgroundTex), false);

    m_circleTexSize = XMUINT2(uint32_t(m_circleTex->GetDesc().Width), uint32_t(m_circleTex->GetDesc().Height));
    m_gamepadTexSize = XMUINT2(uint32_t(m_gamepadTex->GetDesc().Width), uint32_t(m_gamepadTex->GetDesc().Height));
    m_backgroundTexSize = XMUINT2(uint32_t(m_backgroundTex->GetDesc().Width), uint32_t(m_backgroundTex->GetDesc().Height));
}
#elif defined(__d3d11_h__) || defined(__d3d11_x_h__)
void ATG::Help::RestoreDevice(ID3D11DeviceContext* context)
{
    m_context = context;

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    context->GetDevice(device.GetAddressOf());

    m_spriteBatch = std::make_unique<SpriteBatch>(context);

    m_states = std::make_unique<CommonStates>(device.Get());

    m_primBatch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(context);

    m_lineEffect = std::make_unique<BasicEffect>(device.Get());
    m_lineEffect->SetVertexColorEnabled(true);

    {
        void const* shaderByteCode;
        size_t shaderLen;

        m_lineEffect->GetVertexShaderBytecode(&shaderByteCode, &shaderLen);

        DX::ThrowIfFailed(
            device->CreateInputLayout(VertexPositionColor::InputElements, VertexPositionColor::InputElementCount,
                                      shaderByteCode, shaderLen,
                                      m_lineLayout.ReleaseAndGetAddressOf())
            );
    }

#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP)
    wchar_t buff[MAX_PATH];
    DX::FindMediaFile(buff, MAX_PATH, L"Media//Fonts//SegoeUI_18.spritefont");
    m_spriteFonts[SEGOE_UI_18PT] = std::make_unique<SpriteFont>(device.Get(), buff);

    DX::FindMediaFile(buff, MAX_PATH, L"Media//Fonts//SegoeUI_22.spritefont");
    m_spriteFonts[SEGOE_UI_22PT] = std::make_unique<SpriteFont>(device.Get(), buff);

    DX::FindMediaFile(buff, MAX_PATH, L"Media//Fonts//SegoeUI_36.spritefont");
    m_spriteFonts[SEGOE_UI_36PT] = std::make_unique<SpriteFont>(device.Get(), buff);

    DX::FindMediaFile(buff, MAX_PATH, L"Media//Textures//callout_circle.dds");
    DX::ThrowIfFailed(CreateDDSTextureFromFileEx(device.Get(), buff, 0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0, m_linearColors, nullptr, m_circleTex.ReleaseAndGetAddressOf()));

    DX::FindMediaFile(buff, MAX_PATH, L"Media//Textures//gamepad.dds");
    DX::ThrowIfFailed(CreateDDSTextureFromFileEx(device.Get(), buff, 0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0, m_linearColors, nullptr, m_gamepadTex.ReleaseAndGetAddressOf()));

    DX::FindMediaFile(buff, MAX_PATH, L"Media//Textures//ATGSampleBackground.DDS");
    DX::ThrowIfFailed(CreateDDSTextureFromFileEx(device.Get(), buff, 0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0, m_linearColors, nullptr, m_backgroundTex.ReleaseAndGetAddressOf()));
#else
    m_spriteFonts[SEGOE_UI_18PT] = std::make_unique<SpriteFont>(device.Get(), L"SegoeUI_18.spritefont");
    m_spriteFonts[SEGOE_UI_22PT] = std::make_unique<SpriteFont>(device.Get(), L"SegoeUI_22.spritefont");
    m_spriteFonts[SEGOE_UI_36PT] = std::make_unique<SpriteFont>(device.Get(), L"SegoeUI_36.spritefont");

    DX::ThrowIfFailed(CreateDDSTextureFromFileEx(device.Get(), L"callout_circle.dds", 0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0, m_linearColors, nullptr, m_circleTex.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(CreateDDSTextureFromFileEx(device.Get(), L"gamepad.dds", 0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0, m_linearColors, nullptr, m_gamepadTex.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(CreateDDSTextureFromFileEx(device.Get(), L"ATGSampleBackground.DDS", 0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0, m_linearColors, nullptr, m_backgroundTex.ReleaseAndGetAddressOf()));
#endif
}
#endif

void ATG::Help::SetWindow(const RECT& output)
{
    m_screenSize = output;
}
