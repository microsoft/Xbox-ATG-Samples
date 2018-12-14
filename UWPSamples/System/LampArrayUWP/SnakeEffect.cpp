//--------------------------------------------------------------------------------------
// SnakeEffect.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"

using namespace Platform;
using namespace Windows::Foundation::Collections;
using namespace Windows::Foundation;
using namespace Windows::Devices::Lights;
using namespace Windows::Devices::Lights::Effects;
using namespace Windows::UI;
using namespace Windows::System;

// All Effects use Ticks; below multiplers transform units to Ticks.
constexpr uint32_t c_secondsMultiplier = 10'000'000;
constexpr uint32_t c_millisecondsMultiplier = 10'000;

SnakeEffect::SnakeEffect(
    Windows::UI::Color color, 
    int32_t length, 
    Windows::Devices::Lights::LampArray^ lampArray) :
        m_snakeColor(color),
        m_snakeLength(length),
        m_lampArray(lampArray),
        m_snakeHead(0)
{
    m_allLampIndexes = ref new Platform::Array<int>(lampArray->LampCount);
    for (int i = 0; i < lampArray->LampCount; i++)
    {
        m_allLampIndexes[i] = i;
    }

    Start();
}

SnakeEffect::~SnakeEffect()
{
}

void SnakeEffect::Start()
{
    m_playList = ref new LampArrayEffectPlaylist();
	m_playList->RepetitionMode = LampArrayRepetitionMode::Forever;

    LampArrayCustomEffect^ snakeEffect = ref new LampArrayCustomEffect(m_lampArray, m_allLampIndexes);
    snakeEffect->UpdateInterval = { 35 * c_millisecondsMultiplier };
    snakeEffect->UpdateRequested += ref new TypedEventHandler<LampArrayCustomEffect^, LampArrayUpdateRequestedEventArgs^>(this, &SnakeEffect::Update);

    // Since the effect will be run inside a playlist that is repeating 'forever', doesn't really matter what duration we have here.
    snakeEffect->Duration = { c_secondsMultiplier };
	m_playList->Append(snakeEffect);

	m_playList->Start();
}

void SnakeEffect::Stop()
{
	if (m_playList)
	{
		m_playList->Stop();
	}
}

bool SnakeEffect::ContainsLampArray(Windows::Devices::Lights::LampArray^ lampArray)
{
    return lampArray->DeviceId == m_lampArray->DeviceId;
}

void SnakeEffect::Update(
    LampArrayCustomEffect^ sender, 
    LampArrayUpdateRequestedEventArgs^ args)
{
    Platform::Array<Windows::UI::Color>^ colors = GetScaledSnakeColors();
    Platform::Array<int>^ positions = GetPositionsBehindHead(m_snakeHead);

    // Clear any Lamps set by previous iteration
    // Note: can introduce flickering if subsequent call not done fast enough, better approach is no 
    args->SetColor({0xFF, 0x00, 0x00, 0x00});
    args->SetColorsForIndices(colors, positions);

    m_snakeHead++;
    if (m_snakeHead == m_lampArray->LampCount)
    {
        m_snakeHead = 0;
    }
}

Platform::Array<int>^ SnakeEffect::GetPositionsBehindHead(
    int head)
{
    std::vector<int> lampIndexes(m_lampArray->LampCount);

    std::generate(lampIndexes.begin(), lampIndexes.end(), [n = 0]() mutable { return n++; });

    std::rotate(lampIndexes.begin(), lampIndexes.begin() + head, lampIndexes.end());

    std::reverse(lampIndexes.begin(), lampIndexes.end());

    Platform::Array<int>^ data = ref new Platform::Array<int>(lampIndexes.data(), (uint32_t)lampIndexes.size());

    return data;
}

Platform::Array<Windows::UI::Color>^ SnakeEffect::GetScaledSnakeColors()
{
    Platform::Array<Windows::UI::Color>^ colors = ref new Platform::Array<Windows::UI::Color>(m_lampArray->LampCount);

    for (int i = 0; i < m_snakeLength; i++)
    {
        float factor = (float)(m_snakeLength - i) / (float)m_snakeLength;
        colors[i] = {0xFF, (byte)(m_snakeColor.R * factor), (byte)(m_snakeColor.G * factor), (byte)(m_snakeColor.B * factor)};
    }

    for (int i = m_snakeLength; i < m_lampArray->LampCount; i++)
    {
        colors[i] = {0xFF, 0, 0, 0};
    }

    return colors;
}