//--------------------------------------------------------------------------------------
// BokehEffect12.h
//
// Demonstrates how to render depth of field using point sprites.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"

namespace ATG
{
    class BokehEffect
    {
    public:
        struct Parameters
        {
            // CoC
            float focusLength;
            float FNumber;
            float focalPlane;

            // performance
            float maxCoCSizeNear;
            float maxCoCSizeFar;
            float switchover1[2];
            float switchover2[2];

            // quality
            float initialEnergyScale;
            bool useFastShader;
        };

        BokehEffect(ID3D11Device* device);

        void ResizeResources(ID3D11Device* device, int width, int height, DXGI_FORMAT format);
        void Render(
            ID3D11DeviceContextX* context,
            ID3D11ShaderResourceView* srcColorSRV,
            ID3D11ShaderResourceView* srcDepthSRV,
            ID3D11RenderTargetView* dstRTV,
            const DirectX::XMMATRIX& matInvProj,
            const Parameters& params,
            bool useDebugShader);

    private:
        void CreateDeviceDependentResources(ID3D11Device* device);
        void StartRendering(ID3D11DeviceContextX* context, const D3D11_TEXTURE2D_DESC& texDesc, const DirectX::XMMATRIX& matInvProj, const Parameters& params);
        void StopRendering(ID3D11DeviceContextX* context);
        
        struct BokehCB
        {
            float   maxCoCDiameterNear;
            float   focusLength;
            float   focalPlane;
            float   FNumber;

            float   depthBufferSize[2];
            float   dofTexSize[2];

            float   srcScreenSize[2];
            float   maxCoCDiameterFar;
            float   irisTextureOffset;

            float   viewports[6][4];

            float   switchover1[2];
            float   switchover2[2];

            float   initialEnergyScale;
            float   pad[3];

            float   mInvProj[16];
        };

    private:
        template <typename T>
        using ComPtr = Microsoft::WRL::ComPtr<T>;
        
        // weights texture
        ComPtr<ID3D11Texture1D>             m_energiesTex;
        ComPtr<ID3D11ShaderResourceView>    m_energiesTexSRV;
        ComPtr<ID3D11UnorderedAccessView>   m_energiesTexUAV;

        ComPtr<ID3D11Texture3D>             m_scratchTex;
        ComPtr<ID3D11UnorderedAccessView>   m_scratchTexUAV;

        // the texture that takes front and back blur of the source texture
        ComPtr<ID3D11Texture2D>             m_DOFColorTexture;
        ComPtr<ID3D11RenderTargetView>      m_DOFColorTextureRTV;
        ComPtr<ID3D11ShaderResourceView>    m_DOFColorTextureSRV;

        // in focus copy
        ComPtr<ID3D11Texture2D>             m_sourceColorTextureRGBZCopy;
        ComPtr<ID3D11RenderTargetView>      m_sourceColorTextureRGBZCopyRTV;
        ComPtr<ID3D11ShaderResourceView>    m_sourceColorTextureRGBZCopySRV;

        ComPtr<ID3D11Texture2D>             m_sourceColorTextureRGBZHalfCopy;
        ComPtr<ID3D11RenderTargetView>      m_sourceColorTextureRGBZHalfCopyRTV;
        ComPtr<ID3D11ShaderResourceView>    m_sourceColorTextureRGBZHalfCopySRV;

        // iris texture
        ComPtr<ID3D11ShaderResourceView>    m_irisTexSRV;

        // constant buffer
        ComPtr<ID3D11Buffer>                m_bokehCB;

        // shaders
        ComPtr<ID3D11VertexShader>          m_quadPointVS;
        ComPtr<ID3D11GeometryShader>        m_quadPointGS;
        ComPtr<ID3D11GeometryShader>        m_quadPointFastGS;
        ComPtr<ID3D11PixelShader>           m_quadPointPS;
        ComPtr<ID3D11VertexShader>          m_quadVS;
        ComPtr<ID3D11PixelShader>           m_recombinePS;
        ComPtr<ID3D11PixelShader>           m_recombineDebugPS;
        ComPtr<ID3D11PixelShader>           m_createRGBZPS;
        ComPtr<ID3D11PixelShader>           m_downsampleRGBZPS;
        ComPtr<ID3D11ComputeShader>         m_createEnergyTexCS;

        ComPtr<ID3D11BlendState>            m_pointsBS;
        ComPtr<ID3D11SamplerState>          m_sampler;

        int                                 m_width;
        int                                 m_height;
        DXGI_FORMAT                         m_format;
        D3D11_VIEWPORT                      m_vpSplitOutput[6];
        D3D11_RECT                          m_scissorSplitOutput[6];
    };
}

