//--------------------------------------------------------------------------------------
// EsramVisualize_CS.hlsl
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#define EsramVisualizeRS \
"RootFlags( DENY_VERTEX_SHADER_ROOT_ACCESS   |" \
"           DENY_DOMAIN_SHADER_ROOT_ACCESS   |" \
"           DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
"           DENY_HULL_SHADER_ROOT_ACCESS     |" \
"           DENY_PIXEL_SHADER_ROOT_ACCESS )," \
"CBV(b0)," \
"DescriptorTable( UAV(u0) )"

#define TWO_PI (2.0f * 3.1415926f)
#define TEX_NUM 6

struct Texture
{
    int4 pageRanges[4];

    int numPageRanges;
    int _padding;
    float2 timeRange;

    float4 color;
};

cbuffer Constants : register(b0)
{
    Texture textures[TEX_NUM];

    int4 bounds;
    float4 backgroundColor;

    float backgroundBlend;
    float foregroundBlend;
    int pageCount;
    float duration;

    float time;
    float flashRate;
    float factor;
    int selectedIndex;
};

RWTexture2D<float4> colorTarget : register(u0);

[RootSignature(EsramVisualizeRS)]
[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // Calculate pixel coordinate
    int2 pixelCoords = DTid.xy + bounds.xy;

    // Determine if this pixel is out of bounds of the draw.
    if (pixelCoords.x > bounds.z || pixelCoords.y > bounds.w)
    {
        return;
    }

    // Calculate normalized coordinates.
    float2 coordsNorm = float2(DTid.xy) / float2(bounds.zw - bounds.xy);

    int index = -1;
    for (int i = 0; i < TEX_NUM; ++i)
    {
        float2 timeNorm = textures[i].timeRange / duration;

        for (int j = 0; j < textures[i].numPageRanges; ++j)
        {
            // Calculate normalized page & time ranges.
            float2 pageNorm = float2(textures[i].pageRanges[j].xy) / float(pageCount);

            // Determine if we're within the spatial & temporal ESRAM space occupied by this texture.
            if (timeNorm.x <= coordsNorm.x && coordsNorm.x < timeNorm.y &&
                pageNorm.x <= coordsNorm.y && coordsNorm.y < pageNorm.y)
            {
                index = i;
                break;
            }
        }

        if (index != -1)
        {
            break;
        }
    }

    // Calculate the color this pixel should be.
    if (index != -1)
    {
        // Calculate selected flashing behavior.
        float selectFlash = 0;
        if (index == selectedIndex)
        {
            selectFlash = cos(TWO_PI * time * flashRate) * factor;
        }

        colorTarget[pixelCoords] = lerp(colorTarget[pixelCoords], textures[index].color + selectFlash, foregroundBlend);
    }
    else 
    {
        // Fallback to background coloring.
        colorTarget[pixelCoords] = colorTarget[pixelCoords] + backgroundColor * backgroundBlend;
    }
}
