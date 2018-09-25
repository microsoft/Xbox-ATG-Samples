//--------------------------------------------------------------------------------------
// Timeline.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <list>

#include <directxmath.h>
#include <d3d12_x.h>
#include <pix.h>

struct Timeline
{
public:
    Timeline(uint32_t _colorPix, DirectX::XMVECTOR _color, const wchar_t* _name)
        : colorPix(_colorPix)
        , color(_color)
        , name(_name)
        , start(0)
    {
    }

    void Start()
    {
        PIXBeginEvent(colorPix, name);
        assert(start == 0);
        start = __rdtsc() + 1;
    }

    void End()
    {
        intervals.push_front(Interval(start, __rdtsc()));
        intervals.resize(1023);
        assert(start != 0);
        start = 0;
        PIXEndEvent();
    }

    void Render(_In_ ID3D12GraphicsCommandList* commandList, 
        _In_ DirectX::BasicEffect* effect, 
        _In_ DirectX::PrimitiveBatch<DirectX::VertexPositionColor>* primitiveBatch,
        _In_ DirectX::SpriteBatch* spriteBatch, 
        _In_ DirectX::SpriteFont* font, 
        _In_ const D3D12_VIEWPORT& viewport, 
        DirectX::XMFLOAT2 position, 
        uint64_t Latest) const;

    DirectX::XMVECTOR       color;
    uint32_t                colorPix;
    const wchar_t*          name;

    typedef std::pair<uint64_t, uint64_t> Interval;
    typedef std::list<Interval> ListOfIntervals;
    ListOfIntervals         intervals;
    uint64_t                start;
};

