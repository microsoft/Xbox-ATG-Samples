//--------------------------------------------------------------------------------------
// D3D12RaytracingAO.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "GlobalSharedHlslCompat.h"
#include "Mesh.h"

using namespace DX;
using namespace DirectX;

Mesh::Mesh(
    ID3D12Device* device,
    ID3D12CommandQueue* commandQueue,
    const wchar_t* pFileName)
{
    // Load sdk mesh into memory.
    m_model = Model::CreateFromSDKMESH(pFileName, device);

    // Check the sample's assumptions are correct.
    for (auto& mesh : m_model->meshes)
    {
        ThrowIfFalse(
            mesh->alphaMeshParts.size() == 0,
            L"Alpha Mesh is not allowed in this sample."
        );

        for (unsigned int i = 0; i < mesh->opaqueMeshParts.size(); i++)
        {
            ThrowIfFalse(
                mesh->opaqueMeshParts[i]->indexFormat == DXGI_FORMAT_R32_UINT,
                L"Only 32bit unsigned int indices can be used for this sample.");

            ThrowIfFalse(
                mesh->opaqueMeshParts[i]->primitiveType == D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
                L"Only triangle lists are supported for this sample.");

            ThrowIfFalse(
                mesh->opaqueMeshParts[i]->vertexStride == sizeof(Vertex),
                L"Vertex format does not mach that of the sample."
            );

            // Increase length;
            length++;
        }
    }

    // Check if length is too large.
    ThrowIfFalse(
        length < Parts::MaxParts,
        L"The number of parts in the scene exceeds MaxParts."
    );

    // Load Textures.
    ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin();

    // Extract filepath and create texture factory.
    std::wstring fileName(pFileName);
    auto lastPos = fileName.find_last_of('\\');

    ThrowIfFalse(
        lastPos != std::string::npos,
        (L"Could not find path to file " + fileName).c_str()
    );

    m_textureFactory = m_model->LoadTextures(
        device,
        resourceUpload,
        fileName.substr(0, lastPos).c_str()
    );
    
    // Move buffers out of upload state.
    m_model->LoadStaticBuffers(device, resourceUpload);

    // Wait on texture upload.
    auto future = resourceUpload.End(commandQueue);
    future.wait();
    
    // Texture factory is only a nullptr if there are no textures.
    if (m_textureFactory)
    {
        ThrowIfFalse(
            m_textureFactory->ResourceCount() < Textures::MaxTextures,
            L"The number of textures in the scene exceeds MaxTextures.");

        // Check texture type is 2D.
        for (unsigned int i = 0; i < m_textureFactory->ResourceCount(); i++)
        {
            bool isCubeMap;
            Microsoft::WRL::ComPtr<ID3D12Resource> resource;
            m_textureFactory->GetResource(i, &resource, &isCubeMap);

            ThrowIfFalse(
                !isCubeMap,
                L"Cube maps are not supported for this sample."
            );

            ThrowIfFalse(
                resource->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                L"Only Texture2D images are supported in assets."
            );

            ThrowIfFalse(
                resource->GetDesc().DepthOrArraySize == 1,
                L"Only depth and array sizes of one are supported in assets."
            );
        }
    }
}

void Mesh::SetTextureSRV(
    ID3D12Device* device,
    D3D12_CPU_DESCRIPTOR_HANDLE pStartHandle,
    int matIndex)
{
    if (m_textureFactory && (matIndex != -1))
    {
        bool isCubeMap;
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        m_textureFactory->GetResource(matIndex, &resource, &isCubeMap);
        device->CreateShaderResourceView(
            resource.Get(),
            nullptr,
            pStartHandle
        );
    }
}

void Mesh::SetTextureSRVs(
    ID3D12Device* device,
    D3D12_CPU_DESCRIPTOR_HANDLE pStartHandle,
    SIZE_T increment)
{
    if (m_textureFactory)
    {
        for (unsigned int i = 0; i < m_textureFactory->ResourceCount(); i++)
        {
            bool isCubeMap;
            ID3D12Resource* resource;
            m_textureFactory->GetResource(i, &resource, &isCubeMap);
            device->CreateShaderResourceView(
                resource,
                nullptr,
                { pStartHandle.ptr + increment * SIZE_T(i) }
            );
        }
    }
}