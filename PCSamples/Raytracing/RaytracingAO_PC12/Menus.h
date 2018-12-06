//--------------------------------------------------------------------------------------
// D3D12RaytracingAO.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "ATGColors.h"
#include "Effects.h"
#include "PrimitiveBatch.h"
#include "SpriteFont.h"
#include "VertexTypes.h"

namespace MenuCSUDesc
{
    enum CSUDesc
    {
        SRVSmallFont,
        CSUCount
    };
}

namespace MenuLightingModel
{
    enum LightingModel
    {
        AO,
        SSAO,
        LightingModelCount
    };
}

class Option
{
public:
    Option(DOUBLE pStart, DOUBLE pMin, DOUBLE pMax, DOUBLE pInc, bool pIsint = false) :
        m_start(pStart), m_current(pStart), m_min(pMin), m_max(pMax), m_inc(pInc), m_isInt(pIsint)
    {

    }

    bool Increment()
    {
        auto tmp = m_current;
        m_current += m_inc;

        m_current = std::max(m_current, m_min);
        m_current = std::min(m_current, m_max);

        return tmp != m_current;
    }

    bool Decrement()
    {
        auto tmp = m_current;
        m_current -= m_inc;

        m_current = std::max(m_current, m_min);
        m_current = std::min(m_current, m_max);

        return tmp != m_current;
    }

    DirectX::XMVECTORF32 SlectionColor()
    {
        if (IsLowerLimit())
        {
            return ATG::Colors::Orange;
        }
        else if (IsUpperLimit())
        {
            return ATG::Colors::Green;
        }

        return ATG::Colors::White;
    }

    bool IsLowerLimit()
    {
        return m_current == m_min;
    }

    bool IsUpperLimit()
    {
        return m_current == m_max;
    }

    bool IsInt()
    {
        return m_isInt;
    }

    void Reset()
    {
        m_current = m_start;
    }

    DOUBLE Value()
    {
        return m_current;
    }

private:
    DOUBLE m_start;
    DOUBLE m_current;
    DOUBLE m_min;
    DOUBLE m_max;
    DOUBLE m_inc;
    bool m_isInt;
};

class Menus
{
public:
    void Setup(
        std::shared_ptr<DX::DeviceResources> pDeviceResources);
    void Draw(uint32_t fps, bool halfLine = false);
    void OnSizeChanged();
    bool ProcessKeys(DirectX::Keyboard::KeyboardStateTracker& keyboard);

    // AO.
    Option m_aoDistance = Option(10.f, .1f, 10000.f, .1f);
    Option m_aoFalloff = Option(0.f, -10.f, 10.f, .1f);
    Option m_aoNumSamples = Option(12.f, 1.f, 15.f, 1.f, true);
    Option m_aoSampleType = Option(0.f, 0.f, 1.f, 1.f, true);

    // SSAO.
    Option m_ssaoNoiseFilterTolerance = Option(-3.f, -8.f, .0f, .1f);
    Option m_ssaoBlurTolerance = Option(-5.f, -8.f, -1.f, .1f);
    Option m_ssaoUpsampleTolerance = Option(-7.f, -12.f, -1.f, .1f);
    Option m_ssaoNormalMultiply = Option(1.f, .0f, 5.f, .125f);

    // Misc.
    unsigned int m_lightingModel = 0;

private:
    void CreateDescriptorHeaps();
    void DrawMainMenu(bool center);
    void DrawCenterLine();
    void DrawSplitLabels();
    void DrawLabel();
    void DrawFrameRate(uint32_t fps);

    std::shared_ptr<DX::DeviceResources> m_deviceResources;

    // Font resources.
    std::unique_ptr<DirectX::SpriteBatch> m_batch;
    std::unique_ptr<DirectX::SpriteFont> m_smallFont;

    // Primitives.
    const unsigned int m_lineThickness = 20;
    const unsigned int m_border = 20;
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPosition>> m_primitiveBatch;
    std::unique_ptr<DirectX::BasicEffect> m_basicEffect;

    // Heaps.
    std::unique_ptr<DirectX::DescriptorHeap> m_csuDescriptors;

    // AO.
    std::wstring m_aoSampleNames[2] = {
        L"Uniform",
        L"Cosine"
    };

    Option* m_optionList[9] = {
        &m_aoDistance,
        &m_aoFalloff,
        &m_aoNumSamples,
        &m_aoSampleType,
        &m_ssaoNoiseFilterTolerance,
        &m_ssaoBlurTolerance,
        &m_ssaoUpsampleTolerance,
        &m_ssaoNormalMultiply
    };

    // Logic.
    unsigned int selection = 0;
    float delta = .0f;
    bool m_showFPS = true;
    bool m_showHelp = false;
};