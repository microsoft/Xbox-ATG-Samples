//--------------------------------------------------------------------------------------
// SnakeEffect.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#pragma once

ref class SnakeEffect sealed
{
public:
    SnakeEffect(
        Windows::UI::Color color, 
        int32_t length,
        Windows::Devices::Lights::LampArray^ lampArray);

    void Stop();

    bool ContainsLampArray(
        Windows::Devices::Lights::LampArray^ lampArray);

private:
    ~SnakeEffect();

    void Start();

    void Update(
        Windows::Devices::Lights::Effects::LampArrayCustomEffect^ sender, 
        Windows::Devices::Lights::Effects::LampArrayUpdateRequestedEventArgs^ args);

    Platform::Array<int>^ GetPositionsBehindHead(
        int head);

    Platform::Array<Windows::UI::Color>^ GetScaledSnakeColors();

private:
    int m_snakeHead;

    Windows::UI::Color m_snakeColor;

    int m_snakeLength;

    Windows::Devices::Lights::LampArray^ m_lampArray;

    // Playlist will stop playing on destruction.
    Windows::Devices::Lights::Effects::LampArrayEffectPlaylist^ m_playList;
    
    Platform::Array<int>^ m_allLampIndexes;
};