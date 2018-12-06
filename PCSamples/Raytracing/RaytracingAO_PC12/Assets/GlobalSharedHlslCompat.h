#ifndef GLOBALSHAREDHLSLCOMPAT_H
#define GLOBALSHAREDHLSLCOMPAT_H

#ifdef HLSL
#include "HlslCompat.h"
#else
using namespace DirectX;
#endif

#define NEAR_PLANE 1.f
#define FAR_PLANE 200.0f
#define ZSCALE (float(FAR_PLANE - NEAR_PLANE) / float(NEAR_PLANE))
#define BACKGROUND XMFLOAT4(0.0f, 0.2f, 0.4f, 1.0f)

struct SceneConstantBuffer
{
    XMMATRIX worldView;

    XMMATRIX worldViewProjection;
    XMMATRIX projectionToWorld;

    // Eye position.
    XMVECTOR cameraPosition;
    XMVECTOR frustumPoint;
    XMVECTOR frustumHDelta;
    XMVECTOR frustumVDelta;

    // Specifies the number of times noise texture is tiled.
    XMFLOAT4 noiseTile;
};

struct MaterialConstantBuffer
{
    XMFLOAT3 ambient;
    BOOL isDiffuseTexture;
    XMFLOAT3 diffuse;
    BOOL isSpecularTexture;
    XMFLOAT3 specular;
    BOOL isNormalTexture;
};

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT2 texcoord;
    XMFLOAT3 tangent;
};

#endif