//--------------------------------------------------------------------------------------
// File: WAVFileReader.h
//
// Functions for loading WAV audio files
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#include <stdint.h>
#include <objbase.h>
#include <memory>
#include <mmreg.h>

#if defined(_XBOX_ONE) && defined(_TITLE)
#include <xma2defs.h>
#endif


namespace DX
{
    inline uint32_t GetFormatTag(const WAVEFORMATEX* wfx)
    {
        if (wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
        {
            if (wfx->cbSize < (sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)))
                return 0;

            static const GUID s_wfexBase = { 0x00000000, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 };

            auto wfex = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(wfx);

            if (memcmp(reinterpret_cast<const BYTE*>(&wfex->SubFormat) + sizeof(DWORD),
                reinterpret_cast<const BYTE*>(&s_wfexBase) + sizeof(DWORD), sizeof(GUID) - sizeof(DWORD)) != 0)
            {
                return 0;
            }

            return wfex->SubFormat.Data1;
        }
        else
        {
            return wfx->wFormatTag;
        }
    }

    HRESULT LoadWAVAudioInMemory( _In_reads_bytes_(wavDataSize) const uint8_t* wavData,
                                  _In_ size_t wavDataSize,
                                  _Outptr_ const WAVEFORMATEX** wfx,
                                  _Outptr_ const uint8_t** startAudio,
                                  _Out_ uint32_t* audioBytes );

    HRESULT LoadWAVAudioFromFile( _In_z_ const wchar_t* szFileName, 
                                  _Inout_ std::unique_ptr<uint8_t[]>& wavData,
                                  _Outptr_ const WAVEFORMATEX** wfx,
                                  _Outptr_ const uint8_t** startAudio,
                                  _Out_ uint32_t* audioBytes );

    struct WAVData
    {
        const WAVEFORMATEX* wfx;
        const uint8_t* startAudio;
        uint32_t audioBytes;
        uint32_t loopStart;
        uint32_t loopLength;
        const uint32_t* seek;       // Note: XMA Seek data is Big-Endian
        uint32_t seekCount;

        size_t GetSampleDuration() const
        {
            if (!wfx || !wfx->nChannels)
                return 0;

            switch (GetFormatTag(wfx))
            {
            case WAVE_FORMAT_ADPCM:
            {
                auto adpcmFmt = reinterpret_cast<const ADPCMWAVEFORMAT*>(wfx);

                uint64_t duration = uint64_t(audioBytes / adpcmFmt->wfx.nBlockAlign) * adpcmFmt->wSamplesPerBlock;
                int partial = audioBytes % adpcmFmt->wfx.nBlockAlign;
                if (partial)
                {
                    if (partial >= (7 * adpcmFmt->wfx.nChannels))
                        duration += (partial * 2 / adpcmFmt->wfx.nChannels - 12);
                }
                return static_cast<size_t>(duration);
            }

#if defined(_XBOX_ONE) || (_WIN32_WINNT < _WIN32_WINNT_WIN8) || (_WIN32_WINNT >= _WIN32_WINNT_WIN10)

            case WAVE_FORMAT_WMAUDIO2:
            case WAVE_FORMAT_WMAUDIO3:
                if (seek && seekCount > 0)
                {
                    return seek[seekCount - 1] / uint32_t(2 * wfx->nChannels);
                }
                break;

#endif

#if defined(_XBOX_ONE) && defined(_TITLE)

            case WAVE_FORMAT_XMA2:
                return reinterpret_cast<const XMA2WAVEFORMATEX*>(wfx)->SamplesEncoded;

#endif

            default:
                if (wfx->wBitsPerSample > 0)
                {
                    return static_cast<size_t>((uint64_t(audioBytes) * 8)
                        / uint64_t(wfx->wBitsPerSample * wfx->nChannels));
                }
            }

            return 0;
        }

        size_t GetSampleDurationMS() const
        {
            if (!wfx || !wfx->nSamplesPerSec)
                return 0;

            uint64_t samples = GetSampleDuration();
            return static_cast<size_t>((samples * 1000) / wfx->nSamplesPerSec);
        }
    };

    HRESULT LoadWAVAudioInMemoryEx( _In_reads_bytes_(wavDataSize) const uint8_t* wavData,
                                    _In_ size_t wavDataSize, _Out_ WAVData& result );

    HRESULT LoadWAVAudioFromFileEx( _In_z_ const wchar_t* szFileName, 
                                    _Inout_ std::unique_ptr<uint8_t[]>& wavData,
                                    _Out_ WAVData& result );
}