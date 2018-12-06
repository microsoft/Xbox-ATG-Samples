#define HLSL

#include "GlobalSharedHlslCompat.h"
#include "GBufferH.hlsl"

[maxvertexcount(3)]
void main(
    triangle GSInput input[3],
    inout TriangleStream<GSOutput> output
)
{
    for (uint i = 0; i < 3; i++)
    {
        GSOutput element;
        element.position = input[i].position;
        element.normal = input[i].normal;
        element.texcoord = input[i].texcoord;
        element.tangent = input[i].tangent;
        element.instance = input[i].instance;
        output.Append(element);
    }
}