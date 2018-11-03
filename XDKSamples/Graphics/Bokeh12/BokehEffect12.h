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

        BokehEffect(
            ID3D12Device* device,
            DXGI_FORMAT format,
            DirectX::GraphicsMemory* graphicsMemory,
            DirectX::ResourceUploadBatch& batch);

        void ResizeResources(ID3D12Device* device, int width, int height);
        void Render(
            ID3D12GraphicsCommandList* cmdList, 
            ID3D12Resource* sceneTexture,
            D3D12_CPU_DESCRIPTOR_HANDLE srcColorSRV,
            D3D12_CPU_DESCRIPTOR_HANDLE srcDepthSRV,
            D3D12_CPU_DESCRIPTOR_HANDLE dstRTV,
            const DirectX::XMMATRIX& matInvProj,
            const Parameters& params,
            bool useDebugShader);

    private:
        void CreateResources(ID3D12Device* device, DirectX::ResourceUploadBatch& batch);
        void CreatePSO(ID3D12Device* device, DXGI_FORMAT format);
        void StartRendering(ID3D12GraphicsCommandList* cmdList, const DirectX::XMMATRIX& matInvProj, const Parameters& params);

        enum RootParameters
        {
            e_rootParameterPS0 = 0,
            e_rootParameterGS4,
            e_rootParameterVS0,
            e_rootParameterGS0,
            e_rootParameterCB0,
            e_rootParameterCB1,
            e_rootParameterCB2,
            e_numRootParameters
        };

        enum DescriptorHeapIndex
        {
            e_iSRVSrcColor = 0,
            e_iSRVIris,
            e_iSRVSrcDepth,
            e_iSRVDOFColor,
            e_iSRVSrcDepth2 = 5,
            e_iSRVSourceColorRGBZCopy,
            e_iSRVSourceColorRGBZHalfCopy,
            e_iSRVEnergies,
            e_iUAVScratch,
            e_iUAVEnergies,
            e_iSRVHeapEnd
        };

        enum RTVDescriptorHeapIndex
        {
            e_iRTVDOFColor = 0,
            e_iRTVSourceColorRGBZCopy,
            e_iRTVSourceColorRGBZHalfCopy,
            e_iRTVDst,
            e_iRTVHeapEnd
        };

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

        DirectX::GraphicsMemory*    m_graphicsMemory;

        DirectX::DescriptorPile     m_srvHeap;      // Descriptor heap for SRVs
        DirectX::DescriptorPile     m_rtvHeap;      // Descriptor heap for RTVs

        // weights texture
        ComPtr<ID3D12Resource>      m_energiesTex;
        ComPtr<ID3D12Resource>      m_scratchTex;

        // the texture that takes front and back blur of the source texture
        ComPtr<ID3D12Resource>      m_DOFColorTexture;

        // in focus copy
        ComPtr<ID3D12Resource>      m_sourceColorTextureRGBZCopy;
        ComPtr<ID3D12Resource>      m_sourceColorTextureRGBZHalfCopy;

        // iris texture
        ComPtr<ID3D12Resource>      m_irisTex;

        // Root signature and PSO
        ComPtr<ID3D12RootSignature> m_commonRS;
        ComPtr<ID3D12PipelineState> m_recombinePSO;
        ComPtr<ID3D12PipelineState> m_recombinePSODebug;
        ComPtr<ID3D12PipelineState> m_createRGBZPSO;
        ComPtr<ID3D12PipelineState> m_downsampleRGBZPSO;
        ComPtr<ID3D12PipelineState> m_quadPointPSO;
        ComPtr<ID3D12PipelineState> m_quadPointFastPSO;

        ComPtr<ID3D12RootSignature> m_createEnergyTexRS;
        ComPtr<ID3D12PipelineState> m_createEnergyTexPSO;

        int                         m_width;
        int                         m_height;
        DXGI_FORMAT                 m_format;
        D3D12_VIEWPORT              m_vpSplitOutput[6];
        D3D12_RECT                  m_scissorSplitOutput[6];
    };
}

