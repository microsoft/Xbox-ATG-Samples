//---------------------------------------------pixels-----------------------------------------
// FBC_CPU.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "FBC_CPU.h"

// Undefine to use the (much slower) C codepath
#define USE_SSE2

namespace
{
#pragma pack(push,1)
    struct BC1
    {
        uint16_t    rgb[2]; // 565 colors
        uint32_t    bitmap; // 2bpp rgb bitmap
    };

    struct BC3
    {
        uint8_t     alpha[2];   // alpha values
        uint8_t     bitmap[6];  // 3bpp alpha bitmap
        BC1         bc1;        // BC1 rgb data
    };

    struct BC4U
    {
        uint8_t red_0;
        uint8_t red_1;
        uint8_t indices[6];
    };

    struct BC5U
    {
        BC4U x;
        BC4U y;
    };
#pragma pack(pop)

    inline uint16_t ColorTo565(uint32_t color)
    {
        return ((((color & 0xf8) >> 3) << 11)
            | (((color & 0xfc00) >> 10) << 5)
            | (((color & 0xf80000) >> 19)));
    }

    struct Image
    {
        size_t      width;
        size_t      height;
        DXGI_FORMAT format;
        size_t      rowPitch;
        size_t      slicePitch;
        uint8_t*    pixels;
    };

    void ComputePitch(uint32_t texSize, DXGI_FORMAT fmt, uint32_t& rowPitch, size_t& slicePitch)
    {
        uint32_t bpb = (fmt == DXGI_FORMAT_BC1_TYPELESS
            || fmt == DXGI_FORMAT_BC1_UNORM
            || fmt == DXGI_FORMAT_BC1_UNORM_SRGB
            || fmt == DXGI_FORMAT_BC4_TYPELESS
            || fmt == DXGI_FORMAT_BC4_UNORM
            || fmt == DXGI_FORMAT_BC4_SNORM) ? 8 : 16;

        uint32_t nbw = std::max<uint32_t >(1, (texSize + 3) / 4);
        size_t nbh = std::max<size_t>(1, (texSize + 3) / 4);

        rowPitch = nbw * bpb;
        slicePitch = static_cast<size_t>(rowPitch) * nbh;
    }

#ifdef USE_SSE2

    //-----------------------------------------------------------------------------
    // SSE2 version (byte-based)
    //-----------------------------------------------------------------------------
#define EXTRACT_BLOCK( ptr, rowPitch ) \
            const uint8_t* tempptr = ptr;                                                  \
            __m128i pixels0 = _mm_load_si128( reinterpret_cast<const __m128i*>(tempptr) ); \
            tempptr += rowPitch;                                                           \
                                                                                           \
            __m128i pixels1 = _mm_load_si128( reinterpret_cast<const __m128i*>(tempptr) ); \
            tempptr += rowPitch;                                                           \
                                                                                           \
            __m128i pixels2 = _mm_load_si128( reinterpret_cast<const __m128i*>(tempptr) ); \
            tempptr += rowPitch;                                                           \
                                                                                           \
            __m128i pixels3 = _mm_load_si128( reinterpret_cast<const __m128i*>(tempptr) );

#define GET_MIN_MAX_BBOX() \
            __m128i minColor = _mm_min_epu8( pixels0, pixels1 );                   \
            __m128i maxColor = _mm_max_epu8( pixels0, pixels1 );                   \
                                                                                   \
            minColor = _mm_min_epu8( minColor, pixels2 );                          \
            maxColor = _mm_max_epu8( maxColor, pixels2 );                          \
                                                                                   \
            minColor = _mm_min_epu8( minColor, pixels3 );                          \
            maxColor = _mm_max_epu8( maxColor, pixels3 );                          \
                                                                                   \
            __m128i t1 = _mm_shuffle_epi32( minColor, _MM_SHUFFLE( 3, 2, 3, 2 ) ); \
            __m128i t2 = _mm_shuffle_epi32( maxColor, _MM_SHUFFLE( 3, 2, 3, 2 ) ); \
            minColor = _mm_min_epu8( minColor, t1 );                               \
            maxColor = _mm_max_epu8( maxColor, t2 );                               \
                                                                                   \
            t1 = _mm_shufflelo_epi16( minColor, _MM_SHUFFLE( 3, 2, 3, 2 ) );       \
            t2 = _mm_shufflelo_epi16( maxColor, _MM_SHUFFLE( 3, 2, 3, 2 ) );       \
            minColor = _mm_min_epu8( minColor, t1 );                               \
            maxColor = _mm_max_epu8( maxColor, t2 );

#define INSET_BC1_BBOX() \
            static const int INSET_SHIFT = 4;                  \
            const __m128i zero = _mm_setzero_si128();          \
            minColor = _mm_unpacklo_epi8( minColor, zero );    \
            maxColor = _mm_unpacklo_epi8( maxColor, zero );    \
                                                               \
            t1 = _mm_sub_epi16( maxColor, minColor );          \
            t2 = _mm_srli_epi16( t1, INSET_SHIFT );            \
                                                               \
            minColor = _mm_add_epi16( minColor, t2 );          \
            maxColor = _mm_sub_epi16( maxColor, t2 );          \
                                                               \
            minColor = _mm_packus_epi16( minColor, minColor ); \
            maxColor = _mm_packus_epi16( maxColor, maxColor );  

#define INSET_BC5_BBOX() \
            static const int INSET_ALPHA_SHIFT = 5;            \
            __declspec(align(16)) static const uint16_t s_insetNormal3DcRound[8] = { ((1<<(INSET_ALPHA_SHIFT-1))-1), ((1<<(INSET_ALPHA_SHIFT-1))-1), 0, 0, 0, 0, 0, 0 }; \
            __declspec(align(16)) static const uint16_t s_insetNormal3DcMask[8] = { 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 }; \
            __declspec(align(16)) static const uint16_t s_insetNormal3DcShiftUp[8] = { 1 << INSET_ALPHA_SHIFT, 1 << INSET_ALPHA_SHIFT, 1, 1, 1, 1, 1, 1  }; \
            __declspec(align(16)) static const uint16_t s_insetNormal3DcShiftDown[8] = { 1 << (16 - INSET_ALPHA_SHIFT), 1 << (16 - INSET_ALPHA_SHIFT), 0, 0, 0, 0, 0, 0 }; \
                                                               \
            const __m128i zero = _mm_setzero_si128();          \
            minColor = _mm_unpacklo_epi8( minColor, zero );    \
            maxColor = _mm_unpacklo_epi8( maxColor, zero );    \
                                                               \
            t1 = _mm_sub_epi16( maxColor, minColor );          \
            t1 = _mm_sub_epi16( t1, reinterpret_cast<const __m128i*>(s_insetNormal3DcRound)[0] ); \
            t1 = _mm_and_si128( t1, reinterpret_cast<const __m128i*>(s_insetNormal3DcMask)[0] ); \
                                                               \
            minColor = _mm_mullo_epi16( minColor, reinterpret_cast<const __m128i*>(s_insetNormal3DcShiftUp)[0] ); \
            maxColor = _mm_mullo_epi16( maxColor, reinterpret_cast<const __m128i*>(s_insetNormal3DcShiftUp)[0] ); \
                                                               \
            minColor = _mm_add_epi16( minColor, t1 );          \
            maxColor = _mm_add_epi16( maxColor, t1 );          \
                                                               \
            minColor = _mm_mulhi_epi16( minColor, reinterpret_cast<const __m128i*>(s_insetNormal3DcShiftDown)[0] ); \
            maxColor = _mm_mulhi_epi16( maxColor, reinterpret_cast<const __m128i*>(s_insetNormal3DcShiftDown)[0] ); \
                                                               \
            minColor = _mm_max_epi16( minColor, zero );        \
            maxColor = _mm_max_epi16( maxColor, zero );        \
                                                               \
            minColor = _mm_packus_epi16( minColor, minColor ); \
            maxColor = _mm_packus_epi16( maxColor, maxColor );

#define EMIT_COLOR_INDICES() \
            __declspec(align(16)) static const uint8_t s_colorMask[16] = { 0xF8, 0xFC, 0xF8, 0, 0, 0, 0, 0, 0xF8, 0xFC, 0xF8, 0, 0, 0, 0, 0 };                                      \
            __declspec(align(16)) static const uint16_t s_word_div3[8] = { (1<<16)/3+1, (1<<16)/3+1, (1<<16)/3+1, (1<<16)/3+1, (1<<16)/3+1, (1<<16)/3+1, (1<<16)/3+1, (1<<16)/3+1 }; \
            __declspec(align(16)) static const uint16_t s_word_1[8] = { 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1 };                                                                   \
            __declspec(align(16)) static const uint16_t s_word_2[8] = { 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2 };                                                                   \
                                                                                                    \
            maxColor = _mm_and_si128( maxColor, reinterpret_cast<const __m128i*>(s_colorMask)[0] ); \
            maxColor = _mm_unpacklo_epi8( maxColor, zero );                                         \
            t1 = _mm_shufflelo_epi16( maxColor, _MM_SHUFFLE( 3, 2, 3, 0 ) );                        \
            t2 = _mm_shufflelo_epi16( maxColor, _MM_SHUFFLE( 3, 3, 1, 3 ) );                        \
            t1 = _mm_srli_epi16( t1, 5 );                                                           \
            t2 = _mm_srli_epi16( t2, 6 );                                                           \
            maxColor = _mm_or_si128( maxColor, t1 );                                                \
            maxColor = _mm_or_si128( maxColor, t2 );                                                \
                                                                                                    \
            minColor = _mm_and_si128( minColor, reinterpret_cast<const __m128i*>(s_colorMask)[0] ); \
            minColor = _mm_unpacklo_epi8( minColor, zero );                                         \
            t1 = _mm_shufflelo_epi16( minColor, _MM_SHUFFLE( 3, 2, 3, 0 ) );                        \
            t2 = _mm_shufflelo_epi16( minColor, _MM_SHUFFLE( 3, 3, 1, 3 ) );                        \
            t1 = _mm_srli_epi16( t1, 5 );                                                           \
            t2 = _mm_srli_epi16( t2, 6 );                                                           \
            minColor = _mm_or_si128( minColor, t1 );                                                \
            minColor = _mm_or_si128( minColor, t2 );                                                \
                                                                                                    \
            __m128i color0 = _mm_packus_epi16( maxColor, zero );                                    \
            color0 = _mm_shuffle_epi32( color0, _MM_SHUFFLE( 1, 0, 1, 0 ) );                        \
                                                                                                    \
            __m128i color2 = _mm_add_epi16( maxColor, maxColor );                                   \
            color2 = _mm_add_epi16( color2, minColor );                                             \
            color2 = _mm_mulhi_epi16( color2, reinterpret_cast<const __m128i*>(s_word_div3)[0] );   \
            color2 = _mm_packus_epi16( color2, zero );                                              \
            color2 = _mm_shuffle_epi32( color2, _MM_SHUFFLE( 1, 0, 1, 0 ) );                        \
                                                                                                    \
            __m128i color1 = _mm_packus_epi16( minColor, zero );                                    \
            color1 = _mm_shuffle_epi32( color1, _MM_SHUFFLE( 1, 0, 1, 0 ) );                        \
                                                                                                    \
            __m128i color3 = _mm_add_epi16( minColor, minColor );                                   \
            color3 = _mm_add_epi16( color3, maxColor );                                             \
            color3 = _mm_mulhi_epi16( color3, reinterpret_cast<const __m128i*>(s_word_div3)[0] );   \
            color3 = _mm_packus_epi16( color3, zero );                                              \
            color3 = _mm_shuffle_epi32( color3, _MM_SHUFFLE( 1, 0, 1, 0 ) );                        \
                                                                                                    \
            /* row 2 */                                                              \
            __m128i c1 = _mm_move_epi64( pixels2 );                                  \
            __m128i c2 = _mm_unpackhi_epi64( pixels2, zero );                        \
                                                                                     \
            c1 = _mm_shuffle_epi32( c1, _MM_SHUFFLE( 3, 1, 2, 0 ) );                 \
            c2 = _mm_shuffle_epi32( c2, _MM_SHUFFLE( 3, 1, 2, 0 ) );                 \
                                                                                     \
            t1 = _mm_sad_epu8( c1, color0 );                                         \
            t2 = _mm_sad_epu8( c2, color0 );                                         \
            __m128i d0 = _mm_packs_epi32( t1, t2 );                                  \
                                                                                     \
            t1 = _mm_sad_epu8( c1, color1 );                                         \
            t2 = _mm_sad_epu8( c2, color1 );                                         \
            __m128i d1 = _mm_packs_epi32( t1, t2 );                                  \
                                                                                     \
            t1 = _mm_sad_epu8( c1, color2 );                                         \
            t2 = _mm_sad_epu8( c2, color2 );                                         \
            __m128i d2 = _mm_packs_epi32( t1, t2 );                                  \
                                                                                     \
            t1 = _mm_sad_epu8( c1, color3 );                                         \
            t2 = _mm_sad_epu8( c2, color3 );                                         \
            __m128i d3 = _mm_packs_epi32( t1, t2 );                                  \
                                                                                     \
            /* row 3 */                                                              \
            c1 = _mm_move_epi64( pixels3 );                                          \
            c2 = _mm_unpackhi_epi64( pixels3, zero );                                \
                                                                                     \
            c1 = _mm_shuffle_epi32( c1, _MM_SHUFFLE( 3, 1, 2, 0 ) );                 \
            c2 = _mm_shuffle_epi32( c2, _MM_SHUFFLE( 3, 1, 2, 0 ) );                 \
                                                                                     \
            t1 = _mm_sad_epu8( c1, color0 );                                         \
            t2 = _mm_sad_epu8( c2, color0 );                                         \
            __m128i t = _mm_packs_epi32( t1, t2 );                                   \
            d0 = _mm_packs_epi32( d0, t );                                           \
                                                                                     \
            t1 = _mm_sad_epu8( c1, color1 );                                         \
            t2 = _mm_sad_epu8( c2, color1 );                                         \
            t = _mm_packs_epi32( t1, t2 );                                           \
            d1 = _mm_packs_epi32( d1, t );                                           \
                                                                                     \
            t1 = _mm_sad_epu8( c1, color2 );                                         \
            t2 = _mm_sad_epu8( c2, color2 );                                         \
            t = _mm_packs_epi32( t1, t2 );                                           \
            d2 = _mm_packs_epi32( d2, t );                                           \
                                                                                     \
            t1 = _mm_sad_epu8( c1, color3 );                                         \
            t2 = _mm_sad_epu8( c2, color3 );                                         \
            t = _mm_packs_epi32( t1, t2 );                                           \
            d3 = _mm_packs_epi32( d3, t );                                           \
                                                                                     \
            __m128i b0 = _mm_cmpgt_epi16( d0, d3 );                                  \
            __m128i b1 = _mm_cmpgt_epi16( d1, d2 );                                  \
            __m128i b2 = _mm_cmpgt_epi16( d0, d2 );                                  \
            __m128i b3 = _mm_cmpgt_epi16( d1, d3 );                                  \
            __m128i b4 = _mm_cmpgt_epi16( d2, d3 );                                  \
                                                                                     \
            __m128i x0 = _mm_and_si128( b2, b1 );                                    \
            __m128i x1 = _mm_and_si128( b3, b0 );                                    \
            __m128i x2 = _mm_and_si128( b4, b0 );                                    \
                                                                                     \
            __m128i r = _mm_or_si128( x0, x1 );                                      \
            t1 = _mm_and_si128( x2, reinterpret_cast<const __m128i*>(s_word_1)[0] ); \
            t2 = _mm_and_si128( r, reinterpret_cast<const __m128i*>(s_word_2)[0] );  \
            r = _mm_or_si128( t1, t2 );                                              \
                                                                                     \
            t1 = _mm_shuffle_epi32( r, _MM_SHUFFLE( 1, 0, 3, 2 ) );                  \
                                                                                     \
            r = _mm_unpacklo_epi16( r, zero );                                       \
            t1 = _mm_unpacklo_epi16( t1, zero );                                     \
            t1 = _mm_slli_epi32( t1, 8 );                                            \
                                                                                     \
            __m128i result = _mm_or_si128( t1, r );                                  \
            result = _mm_slli_epi32( result, 16 );                                   \
                                                                                     \
            /* row 0 */                                                              \
            c1 = _mm_move_epi64( pixels0 );                                          \
            c2 = _mm_unpackhi_epi64( pixels0, zero );                                \
                                                                                     \
            c1 = _mm_shuffle_epi32( c1, _MM_SHUFFLE( 3, 1, 2, 0 ) );                 \
            c2 = _mm_shuffle_epi32( c2, _MM_SHUFFLE( 3, 1, 2, 0 ) );                 \
                                                                                     \
            t1 = _mm_sad_epu8( c1, color0 );                                         \
            t2 = _mm_sad_epu8( c2, color0 );                                         \
            d0 = _mm_packs_epi32( t1, t2 );                                          \
                                                                                     \
            t1 = _mm_sad_epu8( c1, color1 );                                         \
            t2 = _mm_sad_epu8( c2, color1 );                                         \
            d1 = _mm_packs_epi32( t1, t2 );                                          \
                                                                                     \
            t1 = _mm_sad_epu8( c1, color2 );                                         \
            t2 = _mm_sad_epu8( c2, color2 );                                         \
            d2 = _mm_packs_epi32( t1, t2 );                                          \
                                                                                     \
            t1 = _mm_sad_epu8( c1, color3 );                                         \
            t2 = _mm_sad_epu8( c2, color3 );                                         \
            d3 = _mm_packs_epi32( t1, t2 );                                          \
                                                                                     \
            /* row 1 */                                                              \
            c1 = _mm_move_epi64( pixels1 );                                          \
            c2 = _mm_unpackhi_epi64( pixels1, zero );                                \
                                                                                     \
            c1 = _mm_shuffle_epi32( c1, _MM_SHUFFLE( 3, 1, 2, 0 ) );                 \
            c2 = _mm_shuffle_epi32( c2, _MM_SHUFFLE( 3, 1, 2, 0 ) );                 \
                                                                                     \
            t1 = _mm_sad_epu8( c1, color0 );                                         \
            t2 = _mm_sad_epu8( c2, color0 );                                         \
            t = _mm_packs_epi32( t1, t2 );                                           \
            d0 = _mm_packs_epi32( d0, t );                                           \
                                                                                     \
            t1 = _mm_sad_epu8( c1, color1 );                                         \
            t2 = _mm_sad_epu8( c2, color1 );                                         \
            t = _mm_packs_epi32( t1, t2 );                                           \
            d1 = _mm_packs_epi32( d1, t );                                           \
                                                                                     \
            t1 = _mm_sad_epu8( c1, color2 );                                         \
            t2 = _mm_sad_epu8( c2, color2 );                                         \
            t = _mm_packs_epi32( t1, t2 );                                           \
            d2 = _mm_packs_epi32( d2, t );                                           \
                                                                                     \
            t1 = _mm_sad_epu8( c1, color3 );                                         \
            t2 = _mm_sad_epu8( c2, color3 );                                         \
            t = _mm_packs_epi32( t1, t2 );                                           \
            d3 = _mm_packs_epi32( d3, t );                                           \
                                                                                     \
            b0 = _mm_cmpgt_epi16( d0, d3 );                                          \
            b1 = _mm_cmpgt_epi16( d1, d2 );                                          \
            b2 = _mm_cmpgt_epi16( d0, d2 );                                          \
            b3 = _mm_cmpgt_epi16( d1, d3 );                                          \
            b4 = _mm_cmpgt_epi16( d2, d3 );                                          \
                                                                                     \
            x0 = _mm_and_si128( b2, b1 );                                            \
            x1 = _mm_and_si128( b3, b0 );                                            \
            x2 = _mm_and_si128( b4, b0 );                                            \
                                                                                     \
            r = _mm_or_si128( x0, x1 );                                              \
            t1 = _mm_and_si128( x2, reinterpret_cast<const __m128i*>(s_word_1)[0] ); \
            t2 = _mm_and_si128( r, reinterpret_cast<const __m128i*>(s_word_2)[0] );  \
            r = _mm_or_si128( t1, t2 );                                              \
                                                                                     \
            t1 = _mm_shuffle_epi32( r, _MM_SHUFFLE( 1, 0, 3, 2 ) );                  \
                                                                                     \
            r = _mm_unpacklo_epi16( r, zero );                                       \
            t1 = _mm_unpacklo_epi16( t1, zero );                                     \
            t1 = _mm_slli_epi32( t1, 8 );                                            \
                                                                                     \
            result = _mm_or_si128( result, t1 );                                     \
            result = _mm_or_si128( result, r );                                      \
                                                                                     \
            t = _mm_shuffle_epi32( result, _MM_SHUFFLE(0, 3, 2, 1) );                \
            t1 = _mm_shuffle_epi32( result, _MM_SHUFFLE(1, 0, 3, 2) );               \
            t2 = _mm_shuffle_epi32( result, _MM_SHUFFLE(2, 1, 0, 3) );               \
                                                                                     \
            t = _mm_slli_epi32(t, 2);                                                \
            t1 = _mm_slli_epi32(t1, 4);                                              \
            t2 = _mm_slli_epi32(t2, 6);                                              \
                                                                                     \
            result = _mm_or_si128(result, t);                                        \
            result = _mm_or_si128(result, t1);                                       \
            result = _mm_or_si128(result, t2);

#define EMIT_ALPHA_INDICES_VARS() \
            __declspec(align(16)) static const uint8_t s_byte_1[16] = { 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01 };                       \
            __declspec(align(16)) static const uint8_t s_byte_2[16] = { 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02 };                       \
            __declspec(align(16)) static const uint8_t s_byte_7[16] = { 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07 };                       \
            __declspec(align(16)) static const uint16_t s_word_div7[8]   = { (1<<16)/7+1, (1<<16)/7+1, (1<<16)/7+1, (1<<16)/7+1, (1<<16)/7+1, (1<<16)/7+1, (1<<16)/7+1, (1<<16)/7+1 };          \
            __declspec(align(16)) static const uint16_t s_word_div14[8]  = { (1<<16)/14+1, (1<<16)/14+1, (1<<16)/14+1, (1<<16)/14+1, (1<<16)/14+1, (1<<16)/14+1, (1<<16)/14+1, (1<<16)/14+1 };  \
            __declspec(align(16)) static const uint16_t s_word_scaleA[8] = { 6, 6, 5, 5, 4, 4, 0, 0 };      \
            __declspec(align(16)) static const uint16_t s_word_scaleB[8] = { 1, 1, 2, 2, 3, 3, 0, 0 };      \
            __declspec(align(16)) static const uint32_t s_alphaMask0[4] = { 7<<0, 0, 7<<0, 0 };             \
            __declspec(align(16)) static const uint32_t s_alphaMask1[4] = { 7<<3, 0, 7<<3, 0 };             \
            __declspec(align(16)) static const uint32_t s_alphaMask2[4] = { 7<<6, 0, 7<<6, 0 };             \
            __declspec(align(16)) static const uint32_t s_alphaMask3[4] = { 7<<9, 0, 7<<9, 0 };             \
            __declspec(align(16)) static const uint32_t s_alphaMask4[4] = { 7<<12, 0, 7<<12, 0 };           \
            __declspec(align(16)) static const uint32_t s_alphaMask5[4] = { 7<<15, 0, 7<<15, 0 };           \
            __declspec(align(16)) static const uint32_t s_alphaMask6[4] = { 7<<18, 0, 7<<18, 0 };           \
            __declspec(align(16)) static const uint32_t s_alphaMask7[4] = { 7<<21, 0, 7<<21, 0 };           \
            __m128i maxa, mina, mid, mid_div_14, ab1, ab2, ab3, ab4, ab5, ab6, ab7, resulta;

#define EMIT_ALPHA_INDICES(alpha) \
            maxa = _mm_set1_epi16( maxAlpha );                                                              \
            mina = _mm_set1_epi16( minAlpha );                                                              \
                                                                                                            \
            mid = _mm_sub_epi16( maxa, mina );                                                              \
            mid_div_14 = _mm_mulhi_epi16( mid, reinterpret_cast<const __m128i*>(s_word_div14)[0] );         \
                                                                                                            \
            ab1 = _mm_add_epi16( mid_div_14, mina );                                                        \
            ab1 = _mm_packus_epi16( ab1, ab1 );                                                             \
                                                                                                            \
            t1 = _mm_mullo_epi16( maxa, reinterpret_cast<const __m128i*>(s_word_scaleA)[0] );               \
            t2 = _mm_mullo_epi16( mina, reinterpret_cast<const __m128i*>(s_word_scaleB)[0] );               \
            t = _mm_add_epi16( t1, t2 );                                                                    \
            t = _mm_mulhi_epi16( t, reinterpret_cast<const __m128i*>(s_word_div7)[0] );                     \
            t = _mm_add_epi16( t, mid_div_14 );                                                             \
                                                                                                            \
            ab2 = _mm_shuffle_epi32( t, _MM_SHUFFLE(0, 0, 0, 0) );                                          \
            ab3 = _mm_shuffle_epi32( t, _MM_SHUFFLE(1, 1, 1, 1) );                                          \
            ab4 = _mm_shuffle_epi32( t, _MM_SHUFFLE(2, 2, 2, 2) );                                          \
            ab2 = _mm_packus_epi16( ab2, ab2 );                                                             \
            ab3 = _mm_packus_epi16( ab3, ab3 );                                                             \
            ab4 = _mm_packus_epi16( ab4, ab4 );                                                             \
                                                                                                            \
            t1 = _mm_mullo_epi16( maxa, reinterpret_cast<const __m128i*>(s_word_scaleB)[0] );               \
            t2 = _mm_mullo_epi16( mina, reinterpret_cast<const __m128i*>(s_word_scaleA)[0] );               \
            t = _mm_add_epi16( t1, t2 );                                                                    \
            t = _mm_mulhi_epi16( t, reinterpret_cast<const __m128i*>(s_word_div7)[0] );                     \
            t = _mm_add_epi16( t, mid_div_14 );                                                             \
                                                                                                            \
            ab5 = _mm_shuffle_epi32( t, _MM_SHUFFLE(2, 2, 2, 2) );                                          \
            ab6 = _mm_shuffle_epi32( t, _MM_SHUFFLE(1, 1, 1, 1) );                                          \
            ab7 = _mm_shuffle_epi32( t, _MM_SHUFFLE(0, 0, 0, 0) );                                          \
            ab5 = _mm_packus_epi16( ab5, ab5 );                                                             \
            ab6 = _mm_packus_epi16( ab6, ab6 );                                                             \
            ab7 = _mm_packus_epi16( ab7, ab7 );                                                             \
                                                                                                            \
            ab1 = _mm_min_epu8( ab1, alpha );                                                               \
            ab2 = _mm_min_epu8( ab2, alpha );                                                               \
            ab3 = _mm_min_epu8( ab3, alpha );                                                               \
            ab4 = _mm_min_epu8( ab4, alpha );                                                               \
            ab5 = _mm_min_epu8( ab5, alpha );                                                               \
            ab6 = _mm_min_epu8( ab6, alpha );                                                               \
            ab7 = _mm_min_epu8( ab7, alpha );                                                               \
                                                                                                            \
            ab1 = _mm_cmpeq_epi8( ab1, alpha );                                                             \
            ab2 = _mm_cmpeq_epi8( ab2, alpha );                                                             \
            ab3 = _mm_cmpeq_epi8( ab3, alpha );                                                             \
            ab4 = _mm_cmpeq_epi8( ab4, alpha );                                                             \
            ab5 = _mm_cmpeq_epi8( ab5, alpha );                                                             \
            ab6 = _mm_cmpeq_epi8( ab6, alpha );                                                             \
            ab7 = _mm_cmpeq_epi8( ab7, alpha );                                                             \
                                                                                                            \
            ab1 = _mm_and_si128( ab1, reinterpret_cast<const __m128i*>(s_byte_1)[0] );                      \
            ab2 = _mm_and_si128( ab2, reinterpret_cast<const __m128i*>(s_byte_1)[0] );                      \
            ab3 = _mm_and_si128( ab3, reinterpret_cast<const __m128i*>(s_byte_1)[0] );                      \
            ab4 = _mm_and_si128( ab4, reinterpret_cast<const __m128i*>(s_byte_1)[0] );                      \
            ab5 = _mm_and_si128( ab5, reinterpret_cast<const __m128i*>(s_byte_1)[0] );                      \
            ab6 = _mm_and_si128( ab6, reinterpret_cast<const __m128i*>(s_byte_1)[0] );                      \
            ab7 = _mm_and_si128( ab7, reinterpret_cast<const __m128i*>(s_byte_1)[0] );                      \
                                                                                                            \
            t1 = _mm_adds_epu8( ab1, reinterpret_cast<const __m128i*>(s_byte_1)[0] );                       \
            t2 = _mm_adds_epu8( ab2, ab3 );                                                                 \
            t = _mm_adds_epu8( t1, t2 );                                                                    \
                                                                                                            \
            t1 = _mm_adds_epu8( ab4, ab5 );                                                                 \
            t2 = _mm_adds_epu8( ab6, ab7 );                                                                 \
            resulta = _mm_adds_epu8( t1, t2 );                                                              \
                                                                                                            \
            resulta = _mm_adds_epu8( resulta, t );                                                          \
            resulta = _mm_and_si128( resulta, reinterpret_cast<const __m128i*>(s_byte_7)[0] );              \
                                                                                                            \
            t = _mm_cmpgt_epi8( reinterpret_cast<const __m128i*>(s_byte_2)[0], resulta );                   \
            t = _mm_and_si128( t, reinterpret_cast<const __m128i*>(s_byte_1)[0] );                          \
                                                                                                            \
            resulta = _mm_xor_si128( resulta, t );                                                          \
                                                                                                            \
            ab1 = _mm_srli_epi64( resulta, 8-3);                                                            \
            ab2 = _mm_srli_epi64( resulta, 16-6);                                                           \
            ab3 = _mm_srli_epi64( resulta, 24-9);                                                           \
            ab4 = _mm_srli_epi64( resulta, 32-12);                                                          \
            ab5 = _mm_srli_epi64( resulta, 40-15);                                                          \
            ab6 = _mm_srli_epi64( resulta, 48-18);                                                          \
            ab7 = _mm_srli_epi64( resulta, 56-21);                                                          \
                                                                                                            \
            resulta = _mm_and_si128( resulta, reinterpret_cast<const __m128i*>(s_alphaMask0)[0] );          \
            ab1 = _mm_and_si128( ab1, reinterpret_cast<const __m128i*>(s_alphaMask1)[0] );                  \
            ab2 = _mm_and_si128( ab2, reinterpret_cast<const __m128i*>(s_alphaMask2)[0] );                  \
            ab3 = _mm_and_si128( ab3, reinterpret_cast<const __m128i*>(s_alphaMask3)[0] );                  \
            ab4 = _mm_and_si128( ab4, reinterpret_cast<const __m128i*>(s_alphaMask4)[0] );                  \
            ab5 = _mm_and_si128( ab5, reinterpret_cast<const __m128i*>(s_alphaMask5)[0] );                  \
            ab6 = _mm_and_si128( ab6, reinterpret_cast<const __m128i*>(s_alphaMask6)[0] );                  \
            ab7 = _mm_and_si128( ab7, reinterpret_cast<const __m128i*>(s_alphaMask7)[0] );                  \
                                                                                                            \
            t1 = _mm_or_si128( resulta, ab1 );                                                              \
            t2 = _mm_or_si128( ab2, ab3 );                                                                  \
            t = _mm_or_si128( t1, t2 );                                                                     \
                                                                                                            \
            t1 = _mm_or_si128( ab4, ab5 );                                                                  \
            t2 = _mm_or_si128( ab6, ab7 );                                                                  \
            resulta = _mm_or_si128( t1, t2 );                                                               \
            resulta = _mm_or_si128( resulta, t );

    void CompressBC1(const Image& src, Image& dst)
    {
        const uint8_t* pSource = src.pixels;
        uint8_t* pDestination = dst.pixels;

        for (size_t j = 0; j < src.height; j += 4)
        {
            const uint8_t* pSrc = pSource;
            uint8_t* pDst = pDestination;

            for (size_t i = 0; i < src.width; i += 4)
            {
                EXTRACT_BLOCK(pSrc, src.rowPitch)

				GET_MIN_MAX_BBOX()

				INSET_BC1_BBOX()

				auto pBC = reinterpret_cast<BC1*>(pDst);
                pBC->rgb[0] = ColorTo565(_mm_cvtsi128_si32(maxColor));
                pBC->rgb[1] = ColorTo565(_mm_cvtsi128_si32(minColor));

                EMIT_COLOR_INDICES()

                pBC->bitmap = _mm_cvtsi128_si32(result);

                pSrc += 16; // 4*4 pixels
                pDst += 8; // 8 Bytes per block
            }

            pSource += src.rowPitch * 4;
            pDestination += dst.rowPitch;
        }
    }

    void CompressBC3(const Image& src, Image& dst)
    {
        const uint8_t* pSource = src.pixels;
        uint8_t* pDestination = dst.pixels;

        for (size_t j = 0; j < src.height; j += 4)
        {
            const uint8_t* pSrc = pSource;
            uint8_t* pDst = pDestination;

            for (size_t i = 0; i < src.width; i += 4)
            {
                EXTRACT_BLOCK(pSrc, src.rowPitch)

                GET_MIN_MAX_BBOX()

                INSET_BC1_BBOX()

                auto pBC = reinterpret_cast<BC3*>(pDst);
                memset(pBC, 0, sizeof(BC3));

                uint32_t maxc = _mm_cvtsi128_si32(maxColor);
                uint32_t minc = _mm_cvtsi128_si32(minColor);

                pBC->bc1.rgb[0] = ColorTo565(maxc);
                uint8_t maxAlpha = (maxc >> 24) & 0xff;

                pBC->bc1.rgb[1] = ColorTo565(minc);
                uint8_t minAlpha = (minc >> 24) & 0xff;

                assert(maxAlpha >= minAlpha);
                pBC->alpha[0] = maxAlpha;
                pBC->alpha[1] = minAlpha;

                EMIT_COLOR_INDICES()

                pBC->bc1.bitmap = _mm_cvtsi128_si32(result);

                __m128i alpha0 = _mm_srli_epi32(pixels0, 24);
                __m128i alpha1 = _mm_srli_epi32(pixels1, 24);
                t1 = _mm_packus_epi16(alpha0, alpha1);

                __m128i alpha2 = _mm_srli_epi32(pixels2, 24);
                __m128i alpha3 = _mm_srli_epi32(pixels3, 24);
                t2 = _mm_packus_epi16(alpha2, alpha3);

                __m128i alpha = _mm_packus_epi16(t1, t2);

                EMIT_ALPHA_INDICES_VARS()
                EMIT_ALPHA_INDICES(alpha)

                uint32_t abits = _mm_cvtsi128_si32(resulta);
                pBC->bitmap[0] = abits & 0xff;
                pBC->bitmap[1] = (abits >> 8) & 0xff;
                pBC->bitmap[2] = (abits >> 16) & 0xff;

                resulta = _mm_shuffle_epi32(resulta, _MM_SHUFFLE(1, 0, 3, 2));
                abits = _mm_cvtsi128_si32(resulta);
                pBC->bitmap[3] = abits & 0xff;
                pBC->bitmap[4] = (abits >> 8) & 0xff;
                pBC->bitmap[5] = (abits >> 16) & 0xff;

                pSrc += 16; // 4*4 pixels
                pDst += 16; // 16 Bytes per block
            }

            pSource += src.rowPitch * 4;
            pDestination += dst.rowPitch;
        }
    }

    void CompressBC5U(const Image& src, Image& dst)
    {
        __declspec(align(16)) static const uint32_t s_mask[4] = { 0xff, 0xff, 0xff, 0xff };

        const uint8_t* pSource = src.pixels;
        uint8_t* pDestination = dst.pixels;

        for (size_t j = 0; j < src.height; j += 4)
        {
            const uint8_t* pSrc = pSource;
            uint8_t* pDst = pDestination;

            for (size_t i = 0; i < src.width; i += 4)
            {
                EXTRACT_BLOCK(pSrc, src.rowPitch)

                GET_MIN_MAX_BBOX()

                INSET_BC5_BBOX()

                auto pBC = reinterpret_cast<BC5U*>(pDst);
                memset(pBC, 0, sizeof(BC5U));

                uint32_t maxc = _mm_cvtsi128_si32(maxColor);
                uint32_t minc = _mm_cvtsi128_si32(minColor);

                // X channel
                uint8_t maxAlpha = maxc & 0xff;
                uint8_t minAlpha = minc & 0xff;

                assert(maxAlpha >= minAlpha);
                pBC->x.red_0 = maxAlpha;
                pBC->x.red_1 = minAlpha;

                __m128i alpha0 = _mm_and_si128(pixels0, reinterpret_cast<const __m128i*>(s_mask)[0]);
                __m128i alpha1 = _mm_and_si128(pixels1, reinterpret_cast<const __m128i*>(s_mask)[0]);
                t1 = _mm_packus_epi16(alpha0, alpha1);

                __m128i alpha2 = _mm_and_si128(pixels2, reinterpret_cast<const __m128i*>(s_mask)[0]);
                __m128i alpha3 = _mm_and_si128(pixels3, reinterpret_cast<const __m128i*>(s_mask)[0]);
                t2 = _mm_packus_epi16(alpha2, alpha3);

                __m128i alpha = _mm_packus_epi16(t1, t2);

                __m128i t;
                EMIT_ALPHA_INDICES_VARS()
                EMIT_ALPHA_INDICES(alpha)

                uint32_t abits = _mm_cvtsi128_si32(resulta);
                pBC->x.indices[0] = abits & 0xff;
                pBC->x.indices[1] = (abits >> 8) & 0xff;
                pBC->x.indices[2] = (abits >> 16) & 0xff;

                resulta = _mm_shuffle_epi32(resulta, _MM_SHUFFLE(1, 0, 3, 2));
                abits = _mm_cvtsi128_si32(resulta);
                pBC->x.indices[3] = abits & 0xff;
                pBC->x.indices[4] = (abits >> 8) & 0xff;
                pBC->x.indices[5] = (abits >> 16) & 0xff;

                // Y channel
                maxAlpha = (maxc >> 8) & 0xff;
                minAlpha = (minc >> 8) & 0xff;

                assert(maxAlpha >= minAlpha);
                pBC->y.red_0 = maxAlpha;
                pBC->y.red_1 = minAlpha;

                alpha0 = _mm_srli_epi32(pixels0, 8);
                alpha1 = _mm_srli_epi32(pixels1, 8);
                alpha0 = _mm_and_si128(alpha0, reinterpret_cast<const __m128i*>(s_mask)[0]);
                alpha1 = _mm_and_si128(alpha1, reinterpret_cast<const __m128i*>(s_mask)[0]);
                t1 = _mm_packus_epi16(alpha0, alpha1);

                alpha2 = _mm_srli_epi32(pixels2, 8);
                alpha3 = _mm_srli_epi32(pixels3, 8);
                alpha2 = _mm_and_si128(alpha2, reinterpret_cast<const __m128i*>(s_mask)[0]);
                alpha3 = _mm_and_si128(alpha3, reinterpret_cast<const __m128i*>(s_mask)[0]);
                t2 = _mm_packus_epi16(alpha2, alpha3);

                alpha = _mm_packus_epi16(t1, t2);

                EMIT_ALPHA_INDICES(alpha)

                    abits = _mm_cvtsi128_si32(resulta);
                pBC->y.indices[0] = abits & 0xff;
                pBC->y.indices[1] = (abits >> 8) & 0xff;
                pBC->y.indices[2] = (abits >> 16) & 0xff;

                resulta = _mm_shuffle_epi32(resulta, _MM_SHUFFLE(1, 0, 3, 2));
                abits = _mm_cvtsi128_si32(resulta);
                pBC->y.indices[3] = abits & 0xff;
                pBC->y.indices[4] = (abits >> 8) & 0xff;
                pBC->y.indices[5] = (abits >> 16) & 0xff;

                pSrc += 16; // 4*4 pixels
                pDst += 16; // 16 Bytes per block
            }

            pSource += src.rowPitch * 4;
            pDestination += dst.rowPitch;
        }
    }

#else // !USE_SSE2

    //-----------------------------------------------------------------------------
    // C-fallback versions
    //-----------------------------------------------------------------------------
    inline void ExtractBlock(const uint8_t* pSource, size_t pitch, uint8_t* pPixels)
    {
        memcpy_s(&pPixels[0], 4 * 4, pSource, 4 * 4);
        pSource += pitch;

        memcpy_s(&pPixels[16], 4 * 4, pSource, 4 * 4);
        pSource += pitch;

        memcpy_s(&pPixels[32], 4 * 4, pSource, 4 * 4);
        pSource += pitch;

        memcpy_s(&pPixels[48], 4 * 4, pSource, 4 * 4);
        pSource += pitch;
    }

    inline void GetMinMaxColors(const uint8_t* pPixels, uint32_t& minColor, uint32_t& maxColor)
    {
        // Inset bounding-box
        static const int INSET_SHIFT = 4;

        uint8_t minclr[4] = { 255, 255, 255, 255 };
        uint8_t maxclr[4] = { 0, 0, 0, 0 };
        uint8_t inset[4];

        for (size_t i = 0; i < 16; i++)
        {
            if (pPixels[i * 4 + 0] < minclr[0]) { minclr[0] = pPixels[i * 4 + 0]; }
            if (pPixels[i * 4 + 1] < minclr[1]) { minclr[1] = pPixels[i * 4 + 1]; }
            if (pPixels[i * 4 + 2] < minclr[2]) { minclr[2] = pPixels[i * 4 + 2]; }
            if (pPixels[i * 4 + 3] < minclr[3]) { minclr[3] = pPixels[i * 4 + 3]; }

            if (pPixels[i * 4 + 0] > maxclr[0]) { maxclr[0] = pPixels[i * 4 + 0]; }
            if (pPixels[i * 4 + 1] > maxclr[1]) { maxclr[1] = pPixels[i * 4 + 1]; }
            if (pPixels[i * 4 + 2] > maxclr[2]) { maxclr[2] = pPixels[i * 4 + 2]; }
            if (pPixels[i * 4 + 3] > maxclr[3]) { maxclr[3] = pPixels[i * 4 + 3]; }
        }

        inset[0] = (maxclr[0] - minclr[0]) >> INSET_SHIFT;
        inset[1] = (maxclr[1] - minclr[1]) >> INSET_SHIFT;
        inset[2] = (maxclr[2] - minclr[2]) >> INSET_SHIFT;
        inset[3] = (maxclr[3] - minclr[3]) >> INSET_SHIFT;

        minclr[0] = (minclr[0] + inset[0] <= 255) ? minclr[0] + inset[0] : 255;
        minclr[1] = (minclr[1] + inset[1] <= 255) ? minclr[1] + inset[1] : 255;
        minclr[2] = (minclr[2] + inset[2] <= 255) ? minclr[2] + inset[2] : 255;
        minclr[3] = (minclr[3] + inset[3] <= 255) ? minclr[3] + inset[3] : 255;

        maxclr[0] = (maxclr[0] >= inset[0]) ? maxclr[0] - inset[0] : 0;
        maxclr[1] = (maxclr[1] >= inset[1]) ? maxclr[1] - inset[1] : 0;
        maxclr[2] = (maxclr[2] >= inset[2]) ? maxclr[2] - inset[2] : 0;
        maxclr[3] = (maxclr[3] >= inset[3]) ? maxclr[3] - inset[3] : 0;

        minColor = (minclr[0]) | (minclr[1] << 8) | (minclr[2] << 16) | (minclr[3] << 24);
        maxColor = (maxclr[0]) | (maxclr[1] << 8) | (maxclr[2] << 16) | (maxclr[3] << 24);
    }

    inline void GetMinMaxNormals(const uint8_t* pPixels, uint8_t& minNormalX, uint8_t& maxNormalX, uint8_t& minNormalY, uint8_t& maxNormalY)
    {
        // Inset bounding-box
        static const int INSET_ALPHA_SHIFT = 5;

        uint8_t minn[2] = { 255, 255 };
        uint8_t maxn[2] = { 0, 0 };

        for (size_t i = 0; i < 16; i++)
        {
            if (pPixels[i * 4 + 0] < minn[0]) { minn[0] = pPixels[i * 4 + 0]; }
            if (pPixels[i * 4 + 1] < minn[1]) { minn[1] = pPixels[i * 4 + 1]; }

            if (pPixels[i * 4 + 0] > maxn[0]) { maxn[0] = pPixels[i * 4 + 0]; }
            if (pPixels[i * 4 + 1] > maxn[1]) { maxn[1] = pPixels[i * 4 + 1]; }
        }

        int inset[2];
        inset[0] = static_cast<int>(maxn[0] - minn[0]) - ((1 << (INSET_ALPHA_SHIFT - 1)) - 1);
        inset[1] = static_cast<int>(maxn[1] - minn[1]) - ((1 << (INSET_ALPHA_SHIFT - 1)) - 1);

        int mini[2];
        mini[0] = ((static_cast<int>(minn[0]) << INSET_ALPHA_SHIFT) + inset[0]) >> INSET_ALPHA_SHIFT;
        mini[1] = ((static_cast<int>(minn[1]) << INSET_ALPHA_SHIFT) + inset[1]) >> INSET_ALPHA_SHIFT;

        int maxi[2];
        maxi[0] = ((static_cast<int>(maxn[0]) << INSET_ALPHA_SHIFT) - inset[0]) >> INSET_ALPHA_SHIFT;
        maxi[1] = ((static_cast<int>(maxn[1]) << INSET_ALPHA_SHIFT) - inset[1]) >> INSET_ALPHA_SHIFT;

        minNormalX = static_cast<uint8_t>((mini[0] >= 0) ? mini[0] : 0);
        minNormalY = static_cast<uint8_t>((mini[1] >= 0) ? mini[1] : 0);

        maxNormalX = static_cast<uint8_t>((maxi[0] <= 255) ? maxi[0] : 255);
        maxNormalY = static_cast<uint8_t>((maxi[1] <= 255) ? maxi[1] : 255);
    }

    inline uint32_t EmitColorIndices(const uint8_t* pPixels, uint32_t minColor, uint32_t maxColor)
    {
        uint8_t colors[4][4];

        uint8_t r = maxColor & 0xff;
        uint8_t g = (maxColor >> 8) & 0xff;
        uint8_t b = (maxColor >> 16) & 0xff;
        colors[0][0] = (r & 0xF8) | (r >> 5);
        colors[0][1] = (g & 0xFC) | (g >> 6);
        colors[0][2] = (b & 0xF8) | (b >> 5);

        r = minColor & 0xff;
        g = (minColor >> 8) & 0xff;
        b = (minColor >> 16) & 0xff;
        colors[1][0] = (r & 0xF8) | (r >> 5);
        colors[1][1] = (g & 0xFC) | (g >> 6);
        colors[1][2] = (b & 0xF8) | (b >> 5);

        colors[2][0] = (2 * colors[0][0] + 1 * colors[1][0]) / 3;
        colors[2][1] = (2 * colors[0][1] + 1 * colors[1][1]) / 3;
        colors[2][2] = (2 * colors[0][2] + 1 * colors[1][2]) / 3;
        colors[3][0] = (1 * colors[0][0] + 2 * colors[1][0]) / 3;
        colors[3][1] = (1 * colors[0][1] + 2 * colors[1][1]) / 3;
        colors[3][2] = (1 * colors[0][2] + 2 * colors[1][2]) / 3;

        uint32_t result = 0;

        for (int i = 15; i >= 0; --i)
        {
            int c0 = pPixels[i * 4 + 0];
            int c1 = pPixels[i * 4 + 1];
            int c2 = pPixels[i * 4 + 2];
            int d0 = abs(colors[0][0] - c0) + abs(colors[0][1] - c1) + abs(colors[0][2] - c2);
            int d1 = abs(colors[1][0] - c0) + abs(colors[1][1] - c1) + abs(colors[1][2] - c2);
            int d2 = abs(colors[2][0] - c0) + abs(colors[2][1] - c1) + abs(colors[2][2] - c2);
            int d3 = abs(colors[3][0] - c0) + abs(colors[3][1] - c1) + abs(colors[3][2] - c2);
            int b0 = d0 > d3;
            int b1 = d1 > d2;
            int b2 = d0 > d2;
            int b3 = d1 > d3;
            int b4 = d2 > d3;
            int x0 = b1 & b2;
            int x1 = b0 & b3;
            int x2 = b0 & b4;

            result |= (x2 | ((x0 | x1) << 1)) << (i << 1);
        }

        return result;
    }

    inline void EmitAlphaIndices(const uint8_t* pPixels, int channelIndex, uint8_t minAlpha, uint8_t maxAlpha, uint8_t bitmap[6])
    {
        assert(maxAlpha >= minAlpha);

        uint8_t indices[16];

        uint8_t mid = (maxAlpha - minAlpha) / (2 * 7);

        uint8_t ab1 = minAlpha + mid;
        uint8_t ab2 = (6 * maxAlpha + 1 * minAlpha) / 7 + mid;
        uint8_t ab3 = (5 * maxAlpha + 2 * minAlpha) / 7 + mid;
        uint8_t ab4 = (4 * maxAlpha + 3 * minAlpha) / 7 + mid;
        uint8_t ab5 = (3 * maxAlpha + 4 * minAlpha) / 7 + mid;
        uint8_t ab6 = (2 * maxAlpha + 5 * minAlpha) / 7 + mid;
        uint8_t ab7 = (1 * maxAlpha + 6 * minAlpha) / 7 + mid;

        const uint8_t* pAlpha = &pPixels[channelIndex];

        for (int i = 0; i < 16; ++i)
        {
            uint8_t a = pAlpha[i * 4];

            int b1 = (a <= ab1);
            int b2 = (a <= ab2);
            int b3 = (a <= ab3);
            int b4 = (a <= ab4);
            int b5 = (a <= ab5);
            int b6 = (a <= ab6);
            int b7 = (a <= ab7);
            int index = (b1 + b2 + b3 + b4 + b5 + b6 + b7 + 1) & 7;

            indices[i] = static_cast<uint8_t>(index ^ (2 > index));
        }

        bitmap[0] = (indices[0] >> 0) | (indices[1] << 3) | (indices[2] << 6);
        bitmap[1] = (indices[2] >> 2) | (indices[3] << 1) | (indices[4] << 4) | (indices[5] << 7);
        bitmap[2] = (indices[5] >> 1) | (indices[6] << 2) | (indices[7] << 5);
        bitmap[3] = (indices[8] >> 0) | (indices[9] << 3) | (indices[10] << 6);
        bitmap[4] = (indices[10] >> 2) | (indices[11] << 1) | (indices[12] << 4) | (indices[13] << 7);
        bitmap[5] = (indices[13] >> 1) | (indices[14] << 2) | (indices[15] << 5);
    }

    //-----------------------------------------------------------------------------
    void CompressBC1(const Image& src, Image& dst)
    {
        uint8_t pixels[16 * 4];

        const uint8_t* pSource = src.pixels;
        uint8_t* pDestination = dst.pixels;

        for (size_t j = 0; j < src.height; j += 4)
        {
            const uint8_t* pSrc = pSource;
            uint8_t* pDst = pDestination;

            for (size_t i = 0; i < src.width; i += 4)
            {
                ExtractBlock(pSrc, src.rowPitch, pixels);

                uint32_t minColor, maxColor;
                GetMinMaxColors(pixels, minColor, maxColor);

                auto pBC = reinterpret_cast<BC1*>(pDst);
                pBC->rgb[0] = ColorTo565(maxColor);
                pBC->rgb[1] = ColorTo565(minColor);

                pBC->bitmap = EmitColorIndices(pixels, minColor, maxColor);

                pSrc += 16; // 4*4 pixels
                pDst += 8; // 8 Bytes per block
            }

            pSource += src.rowPitch * 4;
            pDestination += dst.rowPitch;
        }
    }

    //-----------------------------------------------------------------------------
    void CompressBC3(const Image& src, Image& dst)
    {
        uint8_t pixels[16 * 4];

        const uint8_t* pSource = src.pixels;
        uint8_t* pDestination = dst.pixels;

        for (size_t j = 0; j < src.height; j += 4)
        {
            const uint8_t* pSrc = pSource;
            uint8_t* pDst = pDestination;

            for (size_t i = 0; i < src.width; i += 4)
            {
                ExtractBlock(pSrc, src.rowPitch, pixels);

                uint32_t minColor, maxColor;
                GetMinMaxColors(pixels, minColor, maxColor);

                auto pBC = reinterpret_cast<BC3*>(pDst);
                pBC->bc1.rgb[0] = ColorTo565(maxColor);
                pBC->bc1.rgb[1] = ColorTo565(minColor);
                pBC->bc1.bitmap = EmitColorIndices(pixels, minColor, maxColor);

                uint8_t minAlpha = (minColor >> 24) & 0xff;
                uint8_t maxAlpha = (maxColor >> 24) & 0xff;

                pBC->alpha[0] = maxAlpha;
                pBC->alpha[1] = minAlpha;
                EmitAlphaIndices(pixels, 3, minAlpha, maxAlpha, pBC->bitmap);

                pSrc += 16; // 4*4 pixels
                pDst += 16; // 16 Bytes per block
            }

            pSource += src.rowPitch * 4;
            pDestination += dst.rowPitch;
        }
    }

    //-----------------------------------------------------------------------------
    void CompressBC5U(const Image& src, Image& dst)
    {
        uint8_t pixels[16 * 4];

        const uint8_t* pSource = src.pixels;
        uint8_t* pDestination = dst.pixels;

        for (size_t j = 0; j < src.height; j += 4)
        {
            const uint8_t* pSrc = pSource;
            uint8_t* pDst = pDestination;

            for (size_t i = 0; i < src.width; i += 4)
            {
                ExtractBlock(pSrc, src.rowPitch, pixels);

                uint8_t minX, minY, maxX, maxY;
                GetMinMaxNormals(pixels, minX, maxX, minY, maxY);

                auto pBC = reinterpret_cast<BC5U*>(pDst);
                pBC->x.red_0 = maxX;
                pBC->x.red_1 = minX;
                EmitAlphaIndices(pixels, 0, minX, maxX, pBC->x.indices);

                pBC->y.red_0 = maxY;
                pBC->y.red_1 = minY;
                EmitAlphaIndices(pixels, 1, minY, maxY, pBC->y.indices);

                pSrc += 16; // 4*4 pixels
                pDst += 16; // 16 Bytes per block
            }

            pSource += src.rowPitch * 4;
            pDestination += dst.rowPitch;
        }
    }

#endif // !USE_SSE2 
}

CompressorCPU::CompressorCPU()
{
}

HRESULT CompressorCPU::Prepare(
    uint32_t texSize,
    DXGI_FORMAT bcFormat,
    uint32_t mipLevels,
    std::unique_ptr<uint8_t, aligned_deleter>& result,
    std::vector<D3D12_SUBRESOURCE_DATA>& subresources)
{
    subresources.clear();

    if (texSize < 4 || !mipLevels || mipLevels > D3D12_REQ_MIP_LEVELS)
        return E_INVALIDARG;

    subresources.reserve(mipLevels);

    if ((texSize % 4) != 0)
        return E_INVALIDARG;

    switch (bcFormat)
    {
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC5_UNORM:
        break;

    default:
        return E_INVALIDARG;
    }

    // Compute total size
    size_t totalSize = 0;

    for (uint32_t level = 0; level < mipLevels; ++level)
    {
        uint32_t mipSize = std::max(texSize >> level, 1u);

        uint32_t rowPitch;
        size_t slicePitch;
        ComputePitch(mipSize, bcFormat, rowPitch, slicePitch);

        totalSize += slicePitch;
    }

    auto mem = reinterpret_cast<uint8_t*>(_aligned_malloc(totalSize, 16));
    if (!mem)
        return E_OUTOFMEMORY;

    result.reset(mem);

#ifdef _DEBUG
	memset(mem, 0xcc, totalSize);
#endif

    // Set up subresources for result data
    const uint8_t* ptr = mem;

    for (uint32_t level = 0; level < mipLevels; ++level)
    {
        uint32_t mipSize = std::max(texSize >> level, 1u);

        uint32_t rowPitch;
        size_t slicePitch;
        ComputePitch(mipSize, bcFormat, rowPitch, slicePitch);

        D3D12_SUBRESOURCE_DATA initData;
        initData.pData = ptr;
        initData.RowPitch = rowPitch;
        initData.SlicePitch = slicePitch;
        subresources.emplace_back(initData);

        ptr += slicePitch;
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT CompressorCPU::Compress(
	uint32_t texSize,
	uint32_t mipLevels,
	const D3D12_SUBRESOURCE_DATA* subresources,
	DXGI_FORMAT bcFormat,
	const D3D12_SUBRESOURCE_DATA* bcSubresources)
{
	if (texSize < 4 || !mipLevels || mipLevels > D3D12_REQ_MIP_LEVELS)
		return E_INVALIDARG;

	if ((texSize % 4) != 0)
		return E_INVALIDARG;

	if (!subresources || !bcSubresources)
		return E_INVALIDARG;

	for (uint32_t level = 0; level < mipLevels; ++level)
	{
		// Input memory must be 16-byte aligned
		if ((reinterpret_cast<uintptr_t>(subresources[level].pData) % 16) != 0)
			return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

		uint32_t mipSize = std::max(texSize >> level, 4u);

		Image src;
		src.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		src.width = src.height = mipSize;
		src.pixels = reinterpret_cast<uint8_t*>(const_cast<void*>(subresources[level].pData));
		src.rowPitch = subresources[level].RowPitch;
		src.slicePitch = subresources[level].SlicePitch;

		Image dst;
		dst.format = bcFormat;
		dst.width = src.height = mipSize;
		dst.pixels = reinterpret_cast<uint8_t*>(const_cast<void*>(bcSubresources[level].pData));
		dst.rowPitch = bcSubresources[level].RowPitch;
		dst.slicePitch = bcSubresources[level].SlicePitch;

		switch (bcFormat)
		{
		case DXGI_FORMAT_BC1_UNORM:
			CompressBC1(src, dst);
			break;

		case DXGI_FORMAT_BC3_UNORM:
			CompressBC3(src, dst);
			break;

		case DXGI_FORMAT_BC5_UNORM:
			CompressBC5U(src, dst);
			break;

		default:
			return E_INVALIDARG;
		}
	}

	return S_OK;
}
