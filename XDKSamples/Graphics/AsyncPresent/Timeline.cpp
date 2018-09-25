//--------------------------------------------------------------------------------------
// Timeline.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"

#include "Timeline.h"

using namespace DirectX;

_Use_decl_annotations_
void Timeline::Render(ID3D12GraphicsCommandList* commandList, 
    BasicEffect* effect, 
    PrimitiveBatch<VertexPositionColor>* primitiveBatch, 
    SpriteBatch* spriteBatch, 
    SpriteFont* font, 
    const D3D12_VIEWPORT& viewport, 
    XMFLOAT2 position, 
    uint64_t latest) const
{
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, name);

    spriteBatch->Begin(commandList);

    font->DrawString(spriteBatch, name, position, color);

    spriteBatch->End();

    auto xLeftBoundary = position.x + 150.0f;
    auto xRightBoundary = position.x + viewport.Width - 150.0f;
    auto pixelsPerClock = 0.000002f;

    XMMATRIX proj = XMMatrixOrthographicOffCenterRH(viewport.TopLeftX, 
        viewport.TopLeftX + viewport.Width, 
        viewport.TopLeftY + viewport.Height, 
        viewport.TopLeftY, 
        viewport.MinDepth, 
        viewport.MaxDepth);

    effect->SetProjection(proj);
    effect->Apply(commandList);
    primitiveBatch->Begin(commandList);
    for (auto it = intervals.begin(); it != intervals.end(); ++it)
    {
        if (it->second > latest)
        {
            continue;   // This timeline starts later than 'Latest'
        }
        auto xLeft = xRightBoundary - (latest - it->first) * pixelsPerClock;
        auto yTop = position.y;
        auto xRight = xLeft + (it->second - it->first) * pixelsPerClock;
        auto yBottom = yTop + 25.0f;
        if (xLeft < xLeftBoundary)
        {
            break;      // Display has gone off the left of the screen
        }

        const VertexPositionColor quad[] =
        {
            {
                {xLeft, yTop, 0.0f, }, 
                color,
            },
            {
                {xRight, yTop, 0.0f, }, 
                color,
            },
            {
                {xRight, yBottom, 0.0f, }, 
                color,
            },
            {
                {xLeft, yBottom, 0.0f, }, 
                color,
            },
        };

        primitiveBatch->DrawQuad(quad[0], quad[1], quad[2], quad[3]);
    }
    primitiveBatch->End();

    PIXEndEvent(commandList);
}
