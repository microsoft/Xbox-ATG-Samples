//--------------------------------------------------------------------------------------
// File: DirectXTexXbox.h
//
// DirectXTex Auxillary functions for Xbox One texture processing
//
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#pragma once

#ifndef _M_X64
#error This tool is only supported for x64 native
#endif

#include "directxtex.h"

#include <xg.h>

#if defined(_XBOX_ONE) && defined(_TITLE)
#include <d3d11_x.h>
#else
#include <d3d11_1.h>
#endif

#define DIRECTX_TEX_XBOX_VERSION 101

namespace Xbox
{
    class XboxImage
    {
    public:
        XboxImage() : dataSize(0), baseAlignment(0), tilemode(XG_TILE_MODE_INVALID), metadata{}, memory(nullptr)
        {
        }
        ~XboxImage()
        {
            Release();
        }

        HRESULT Initialize(_In_ const XG_TEXTURE1D_DESC& desc, _In_ const XG_RESOURCE_LAYOUT& layout, _In_ uint32_t miscFlags2 = 0);
        HRESULT Initialize(_In_ const XG_TEXTURE2D_DESC& desc, _In_ const XG_RESOURCE_LAYOUT& layout, _In_ uint32_t miscFlags2 = 0);
        HRESULT Initialize(_In_ const XG_TEXTURE3D_DESC& desc, _In_ const XG_RESOURCE_LAYOUT& layout, _In_ uint32_t miscFlags2 = 0);
        HRESULT Initialize(_In_ const DirectX::TexMetadata& mdata, _In_ XG_TILE_MODE tm, _In_ uint32_t size, _In_ uint32_t alignment);

        void Release();

        const DirectX::TexMetadata& GetMetadata() const { return metadata; }
        XG_TILE_MODE GetTileMode() const { return tilemode; }

        uint32_t GetSize() const { return dataSize; }
        uint32_t GetAlignment() const { return baseAlignment; }
        uint8_t* GetPointer() const { return memory; }

    private:
        uint32_t                dataSize;
        uint32_t                baseAlignment;
        XG_TILE_MODE            tilemode;
        DirectX::TexMetadata    metadata;
        uint8_t*                memory;

        // Hide copy constructor and assignment operator
        XboxImage(const XboxImage&);
        XboxImage& operator=(const XboxImage&);
    };

    //---------------------------------------------------------------------------------
    // Image I/O

    HRESULT GetMetadataFromDDSMemory(
        _In_reads_bytes_(size) const void* pSource, _In_ size_t size,
        _Out_ DirectX::TexMetadata& metadata, _Out_ bool& isXbox);
    HRESULT GetMetadataFromDDSFile(
        _In_z_ const wchar_t* szFile, _Out_ DirectX::TexMetadata& metadata, _Out_ bool& isXbox);

    HRESULT LoadFromDDSMemory(
        _In_reads_bytes_(size) const void* pSource, _In_ size_t size,
        _Out_opt_ DirectX::TexMetadata* metadata, _Out_ XboxImage& image);
    HRESULT LoadFromDDSFile(
        _In_z_ const wchar_t* szFile,
        _Out_opt_ DirectX::TexMetadata* metadata, _Out_ XboxImage& image);

    HRESULT SaveToDDSMemory(_In_ const XboxImage& xbox, _Out_ DirectX::Blob& blob);
    HRESULT SaveToDDSFile(_In_ const XboxImage& xbox, _In_z_ const wchar_t* szFile);

    //---------------------------------------------------------------------------------
    // Xbox One Texture Tiling / Detiling (requires XG DLL to be present at runtime)

    HRESULT Tile(_In_ const DirectX::Image& srcImage, _Out_ XboxImage& xbox, _In_ XG_TILE_MODE mode = XG_TILE_MODE_INVALID);
    HRESULT Tile(
        _In_ const DirectX::Image* srcImages, _In_ size_t nimages, _In_ const DirectX::TexMetadata& metadata,
        _Out_ XboxImage& xbox, _In_ XG_TILE_MODE mode = XG_TILE_MODE_INVALID);

    HRESULT Detile(_In_ const XboxImage& xbox, _Out_ DirectX::ScratchImage& image);

    //---------------------------------------------------------------------------------
    // Direct3D 11.X functions

#if defined(_XBOX_ONE) && defined(_TITLE) && defined(__d3d11_x_h__)

    HRESULT CreateTexture(
        _In_ ID3D11DeviceX* d3dDevice,
        _In_ const XboxImage& xbox, _Outptr_opt_ ID3D11Resource** ppResource, _Outptr_ void** grfxMemory);

    HRESULT CreateShaderResourceView(
        _In_ ID3D11DeviceX* d3dDevice,
        _In_ const XboxImage& xbox, _Outptr_opt_ ID3D11ShaderResourceView** ppSRV, _Outptr_ void** grfxMemory);

    void FreeTextureMemory(_In_ ID3D11DeviceX* d3dDevice, _In_opt_ void* grfxMemory);

#endif

    //---------------------------------------------------------------------------------
    // Direct3D 12.X functions

#if defined(_XBOX_ONE) && defined(_TITLE) && defined(__d3d12_x_h__)

    HRESULT CreateTexture(
        _In_ ID3D12Device* d3dDevice,
        _In_ const XboxImage& xbox, _Outptr_opt_ ID3D12Resource** ppResource, _Outptr_ void** grfxMemory);

    void FreeTextureMemory(_In_ ID3D12Device* d3dDevice, _In_opt_ void* grfxMemory);

#endif

}; // namespace
