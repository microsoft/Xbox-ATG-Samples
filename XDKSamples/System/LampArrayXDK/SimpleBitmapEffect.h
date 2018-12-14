//--------------------------------------------------------------------------------------
// SimpleBitmapEffect.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#pragma once

ref class SimpleBitmapEffect sealed
{
public:
    SimpleBitmapEffect(
        Windows::Devices::Lights::LampArray^ lampArray);

    void Stop();

    bool ContainsLampArray(
        Windows::Devices::Lights::LampArray^ lampArray);

private:
    ~SimpleBitmapEffect();

    void Start();

    void UpdateBitmap(
        Windows::Devices::Lights::Effects::LampArrayBitmapEffect^ effect,
        Windows::Devices::Lights::Effects::LampArrayBitmapRequestedEventArgs^ args);

private:
    Windows::Devices::Lights::LampArray^ m_lampArray;

    // Playlist will stop playing on destruction.
    Windows::Devices::Lights::Effects::LampArrayEffectPlaylist^ m_playList;

    Platform::Array<int>^ m_allLampIndexes;

    bool m_flipBitmap;
};