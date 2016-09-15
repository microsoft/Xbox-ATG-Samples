//--------------------------------------------------------------------------------------
// SimpleBezier.hlsli
//
// Shader demonstrating DirectX tessellation of a bezier surface
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


// The input patch size.  In this sample, it is 16 control points.
// This value should match the call to IASetPrimitiveTopology()
#define INPUT_PATCH_SIZE 16

// The output patch size.  In this sample, it is also 16 control points.
#define OUTPUT_PATCH_SIZE 16

//--------------------------------------------------------------------------------------
// Constant Buffers
//--------------------------------------------------------------------------------------
cbuffer cbPerFrame : register(b0)
{
    row_major matrix g_mViewProjection;
    float3 g_cameraWorldPos;
    float  g_tessellationFactor;
};

//--------------------------------------------------------------------------------------
// Vertex shader section
//--------------------------------------------------------------------------------------
struct VS_CONTROL_POINT_INPUT
{
    float3 pos      : POSITION;
};

struct VS_CONTROL_POINT_OUTPUT
{
    float3 pos      : POSITION;
};

//--------------------------------------------------------------------------------------
// This simple vertex shader passes the control points straight through to the
// hull shader.  In a more complex scene, you might transform the control points
// or perform skinning at this step.
//
// The input to the vertex shader comes from the vertex buffer.
//
// The output from the vertex shader will go into the hull shader.
//--------------------------------------------------------------------------------------
VS_CONTROL_POINT_OUTPUT BezierVS(VS_CONTROL_POINT_INPUT Input)
{
    VS_CONTROL_POINT_OUTPUT output;

    output.pos = Input.pos;

    return output;
}

//--------------------------------------------------------------------------------------
// Constant data function for the BezierHS.  This is executed once per patch.
//--------------------------------------------------------------------------------------
struct HS_CONSTANT_DATA_OUTPUT
{
    float Edges[4]      : SV_TessFactor;
    float Inside[2]     : SV_InsideTessFactor;
};

struct HS_OUTPUT
{
    float3 pos          : BEZIERPOS;
};

//--------------------------------------------------------------------------------------
// This constant hull shader is executed once per patch.  For the simple Mobius strip
// model, it will be executed 4 times.  In this sample, we set the tessellation factor
// via SV_TessFactor and SV_InsideTessFactor for each patch.  In a more complex scene,
// you might calculate a variable tessellation factor based on the camera's distance.
//--------------------------------------------------------------------------------------
HS_CONSTANT_DATA_OUTPUT BezierConstantHS(InputPatch< VS_CONTROL_POINT_OUTPUT, INPUT_PATCH_SIZE > ip,
    uint PatchID : SV_PrimitiveID)
{
    HS_CONSTANT_DATA_OUTPUT output;

    float TessAmount = g_tessellationFactor;

    output.Edges[0] = output.Edges[1] = output.Edges[2] = output.Edges[3] = TessAmount;
    output.Inside[0] = output.Inside[1] = TessAmount;

    return output;
}

//--------------------------------------------------------------------------------------
// BezierHS
// The hull shader is called once per output control point, which is specified with
// outputcontrolpoints.  For this sample, we take the control points from the vertex
// shader and pass them directly off to the domain shader.  In a more complex scene,
// you might perform a basis conversion from the input control points into a Bezier
// patch, such as the SubD11 SimpleBezier.
//
// The input to the hull shader comes from the vertex shader.
//
// The output from the hull shader will go to the domain shader.
// The tessellation factor, topology, and partition mode will go to the fixed function
// tessellator stage to calculate the UVW barycentric coordinates and domain points.
//--------------------------------------------------------------------------------------
[domain("quad")]
[partitioning(BEZIER_HS_PARTITION)]
[outputtopology("triangle_cw")]
[outputcontrolpoints(OUTPUT_PATCH_SIZE)]
[patchconstantfunc("BezierConstantHS")]
HS_OUTPUT BezierHS(InputPatch< VS_CONTROL_POINT_OUTPUT, INPUT_PATCH_SIZE > p,
    uint i : SV_OutputControlPointID,
    uint PatchID : SV_PrimitiveID)
{
    HS_OUTPUT output;
    output.pos = p[i].pos;
    return output;
}

//--------------------------------------------------------------------------------------
// Bezier evaluation domain shader section
//--------------------------------------------------------------------------------------
struct DS_OUTPUT
{
    float4 pos        : SV_POSITION;
    float3 worldPos   : WORLDPOS;
    float3 normal     : NORMAL;
};

//--------------------------------------------------------------------------------------
float4 BernsteinBasis(float t)
{
    float invT = 1.0f - t;

    return float4(invT * invT * invT,
        3.0f * t * invT * invT,
        3.0f * t * t * invT,
        t * t * t);
}

//--------------------------------------------------------------------------------------
float4 dBernsteinBasis(float t)
{
    float invT = 1.0f - t;

    return float4(-3 * invT * invT,
        3 * invT * invT - 6 * t * invT,
        6 * t * invT - 3 * t * t,
        3 * t * t);
}

//--------------------------------------------------------------------------------------
float3 EvaluateBezier(const OutputPatch< HS_OUTPUT, OUTPUT_PATCH_SIZE > BezPatch,
    float4 BasisU,
    float4 BasisV)
{
    float3 value = float3(0, 0, 0);
    value = BasisV.x * (BezPatch[0].pos * BasisU.x + BezPatch[1].pos * BasisU.y + BezPatch[2].pos * BasisU.z + BezPatch[3].pos * BasisU.w);
    value += BasisV.y * (BezPatch[4].pos * BasisU.x + BezPatch[5].pos * BasisU.y + BezPatch[6].pos * BasisU.z + BezPatch[7].pos * BasisU.w);
    value += BasisV.z * (BezPatch[8].pos * BasisU.x + BezPatch[9].pos * BasisU.y + BezPatch[10].pos * BasisU.z + BezPatch[11].pos * BasisU.w);
    value += BasisV.w * (BezPatch[12].pos * BasisU.x + BezPatch[13].pos * BasisU.y + BezPatch[14].pos * BasisU.z + BezPatch[15].pos * BasisU.w);

    return value;
}


//--------------------------------------------------------------------------------------
// The domain shader is run once per vertex and calculates the final vertex's position
// and attributes.  It receives the UVW from the fixed function tessellator and the
// control point outputs from the hull shader.  Since we are using the DirectX 11
// Tessellation pipeline, it is the domain shader's responsibility to calculate the
// final SV_POSITION for each vertex.  In this sample, we evaluate the vertex's
// position using a Bernstein polynomial and the normal is calculated as the cross
// product of the U and V derivatives.
//
// The input SV_DomainLocation to the domain shader comes from fixed function
// tessellator.  And the OutputPatch comes from the hull shader.  From these, you
// must calculate the final vertex position, color, texcoords, and other attributes.
//
// The output from the domain shader will be a vertex that will go to the video card's
// rasterization pipeline and get drawn to the screen.
//--------------------------------------------------------------------------------------
[domain("quad")]
DS_OUTPUT BezierDS(HS_CONSTANT_DATA_OUTPUT input,
    float2 UV : SV_DomainLocation,
    const OutputPatch< HS_OUTPUT, OUTPUT_PATCH_SIZE > BezPatch)
{
    float4 BasisU = BernsteinBasis(UV.x);
    float4 BasisV = BernsteinBasis(UV.y);
    float4 dBasisU = dBernsteinBasis(UV.x);
    float4 dBasisV = dBernsteinBasis(UV.y);

    float3 worldPos = EvaluateBezier(BezPatch, BasisU, BasisV);
    float3 tangent = EvaluateBezier(BezPatch, dBasisU, BasisV);
    float3 biTangent = EvaluateBezier(BezPatch, BasisU, dBasisV);
    float3 normal = normalize(cross(tangent, biTangent));

    DS_OUTPUT output;
    output.pos = mul(float4(worldPos, 1), g_mViewProjection);
    output.worldPos = worldPos;
    output.normal = normal;

    return output;
}

//--------------------------------------------------------------------------------------
// Smooth shading pixel shader section
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// The pixel shader works the same as it would in a normal graphics pipeline.
// In this sample, it performs very simple N dot L lighting.
//--------------------------------------------------------------------------------------
float4 BezierPS(DS_OUTPUT Input) : SV_TARGET
{
    float3 N = normalize(Input.normal);
    float3 L = normalize(Input.worldPos - g_cameraWorldPos);
    return abs(dot(N, L)) * float4(1, 0, 0, 1);
}

//--------------------------------------------------------------------------------------
// Solid color shading pixel shader (used for wireframe overlay)
//--------------------------------------------------------------------------------------
float4 SolidColorPS(DS_OUTPUT Input) : SV_TARGET
{
    // Return a dark green color
    return float4(0, 0.25, 0, 1);
}
