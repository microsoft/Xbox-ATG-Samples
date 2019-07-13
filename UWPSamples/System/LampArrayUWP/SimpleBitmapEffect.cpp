//--------------------------------------------------------------------------------------
// SimpleBitmapEffect.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#include "pch.h"

#include "MemoryBuffer.h"

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

SimpleBitmapEffect::SimpleBitmapEffect(
    Windows::Devices::Lights::LampArray^ lampArray) :
        m_flipBitmap(false)
{
    m_lampArray = lampArray;

    m_allLampIndexes = ref new Platform::Array<int>(lampArray->LampCount);
    for (int i = 0; i < lampArray->LampCount; i++)
    {
        m_allLampIndexes[i] = i;
    }

    Start();
}

SimpleBitmapEffect::~SimpleBitmapEffect()
{
}

void SimpleBitmapEffect::Start()
{
    m_playList = ref new LampArrayEffectPlaylist();
    m_playList->RepetitionMode = LampArrayRepetitionMode::Forever;

    // This effect will apply to every Lamp on the device.
    Platform::Array<int>^ allLampIndexes = ref new Platform::Array<int>(m_lampArray->LampCount);
    for (int i = 0; i < m_lampArray->LampCount; i++)
    {
        allLampIndexes[i] = i;
    }

    LampArrayBitmapEffect^ bitmapEffect = ref new LampArrayBitmapEffect(m_lampArray, allLampIndexes);

    // Update handler is triggered; once the playlist starts the effect, at every UpdateInternal, when duration expires.
    bitmapEffect->BitmapRequested += ref new TypedEventHandler<LampArrayBitmapEffect^, LampArrayBitmapRequestedEventArgs^>(this, &SimpleBitmapEffect::UpdateBitmap);

    Windows::Foundation::TimeSpan span;
    span.Duration = 1000LL * static_cast<long long>(c_secondsMultiplier);
    bitmapEffect->Duration = span;

    span.Duration = c_secondsMultiplier;
    bitmapEffect->UpdateInterval = span;

    m_playList = ref new LampArrayEffectPlaylist();
    m_playList->RepetitionMode = LampArrayRepetitionMode::Forever;
    m_playList->Append(bitmapEffect);

    m_playList->Start();
}

void SimpleBitmapEffect::Stop()
{
    if (m_playList)
    {
        m_playList->Stop();
    }
}

bool SimpleBitmapEffect::ContainsLampArray(Windows::Devices::Lights::LampArray^ lampArray)
{
    return lampArray->DeviceId == m_lampArray->DeviceId;
}

void SimpleBitmapEffect::UpdateBitmap(
    LampArrayBitmapEffect^ effect,
    LampArrayBitmapRequestedEventArgs^ args)
{
	auto bitmap = ref new Windows::Graphics::Imaging::SoftwareBitmap(Windows::Graphics::Imaging::BitmapPixelFormat::Rgba8,
		static_cast<int>(effect->SuggestedBitmapSize.Width), static_cast<int>(effect->SuggestedBitmapSize.Height));
	{
		auto buffer = bitmap->LockBuffer(Windows::Graphics::Imaging::BitmapBufferAccessMode::Write);
		{
			Windows::Foundation::IMemoryBufferReference^ reference = buffer->CreateReference();
			{
				Microsoft::WRL::ComPtr<Windows::Foundation::IMemoryBufferByteAccess> bufferByteAccess;

				HRESULT result = reinterpret_cast<IInspectable*>(reference)->QueryInterface(IID_PPV_ARGS(&bufferByteAccess));
				if (result == S_OK)
				{
					byte* bitmapBytes;
					uint32_t capacity;
					bufferByteAccess->GetBuffer(&bitmapBytes, &capacity);

					// Bitmap is an array of RGBA8 colors, so color is 4bytes.  Array of this size MUST be passed for UpdateBitmap
					uint32_t bitmapByteCount = (uint32_t)(effect->SuggestedBitmapSize.Height * effect->SuggestedBitmapSize.Width) * 4;

					// Set 'top' half of bitmap to blue, bottom to yellow
					// Lamps that are 'in the middle' geometrically will be interpolated by averaging the surrounding colors.
					for (uint32_t i = 0; i < bitmapByteCount; i += 4)
					{
						bool renderBlue = m_flipBitmap ^ (i < bitmapByteCount / 2);

						if (renderBlue)
						{
							// Blue.  All colors must be in RGBA8 format.
							bitmapBytes[i] = 0x00;      // R
							bitmapBytes[i + 1] = 0x00;  // G
							bitmapBytes[i + 2] = 0xFF;  // B
							bitmapBytes[i + 3] = 0xFF;  // A
						}
						else
						{
							// Yellow.  All colors must be in RGBA8 format.
							bitmapBytes[i] = 0xFF;      // R
							bitmapBytes[i + 1] = 0xFF;  // G
							bitmapBytes[i + 2] = 0x00;  // B
							bitmapBytes[i + 3] = 0xFF;  // A
						}
					}
				}
			}
		}
	}

	m_flipBitmap = !m_flipBitmap;

    args->UpdateBitmap(bitmap);
}