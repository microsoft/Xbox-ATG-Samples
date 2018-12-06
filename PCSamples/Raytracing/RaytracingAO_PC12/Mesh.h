//--------------------------------------------------------------------------------------
// D3D12RaytracingAO.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <GraphicsMemory.h>
#include <Model.h>

namespace Parts
{
    constexpr unsigned int MaxParts = 10;
}

namespace Textures
{
    constexpr unsigned int MaxTextures = 10;
}

namespace Materials
{
    constexpr unsigned int MaxMaterials = 10;
}

struct MeshInfo
{
    unsigned int matID;
    unsigned int numIndices;
    unsigned int numVertices;
    unsigned int stride;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
};

class MeshType
{
public:
    MeshType(std::shared_ptr<DirectX::Model> pModel) :
        m_model(pModel), m_meshIndex(0), m_partIndex(0)
    {

    }

    MeshType(
        std::shared_ptr<DirectX::Model> pModel,
        size_t pMeshIndex,
        size_t pPartIndex) :
        m_model(pModel), m_meshIndex(pMeshIndex), m_partIndex(pPartIndex)
    {

    }

    bool operator!=(const MeshType& rhs) const
    {
        return m_meshIndex != rhs.m_meshIndex || m_partIndex != rhs.m_partIndex;
    }

    MeshInfo operator*()
    {
        auto& meshPtr = m_model->meshes[m_meshIndex]->opaqueMeshParts[m_partIndex];

        MeshInfo meshInfo {
            meshPtr->materialIndex,
            meshPtr->indexCount,
            meshPtr->vertexCount,
            meshPtr->vertexStride,
            meshPtr->staticIndexBuffer,
            meshPtr->staticVertexBuffer
        };

        return std::move(meshInfo);
    }

    MeshType& operator++()
    {
        // Do not update if at end.
        if (m_meshIndex == m_model->meshes.size())
        {
            return *this;
        }

        m_partIndex++;

        if (m_partIndex == m_model->meshes[m_meshIndex]->opaqueMeshParts.size())
        {
            m_meshIndex++;
            m_partIndex = 0;
        }

        return *this;
    }

private:
    std::shared_ptr<DirectX::Model> m_model;
    size_t m_meshIndex, m_partIndex;
};

class Mesh : public std::iterator<std::forward_iterator_tag, MeshType>
{
public:
    Mesh(
        ID3D12Device* device,
        ID3D12CommandQueue* commandQueue,
        const wchar_t* pFileName);

    void SetTextureSRV(
        ID3D12Device* device,
        D3D12_CPU_DESCRIPTOR_HANDLE pStartHandle,
        int matIndex);

    void SetTextureSRVs(
        ID3D12Device* device,
        D3D12_CPU_DESCRIPTOR_HANDLE pStartHandle,
        SIZE_T increment);

    size_t size()
    {
        return length;
    }

    // Iterator
    MeshType begin()
    {
        return std::move(
            MeshType(
                m_model,
                0,
                0)
        );
    }

    const MeshType begin() const
    {
        return std::move(
            MeshType(
                m_model,
                0,
                0)
        );
    }

    MeshType end()
    {
        return std::move(
            MeshType(
                m_model,
                m_model->meshes.size(),
                0)
        );
    }

    const MeshType end() const
    {
        return std::move(
            MeshType(
                m_model,
                m_model->meshes.size(),
                0)
        );
    }

    std::shared_ptr<DirectX::Model> getModel()
    {
        return m_model;
    }

private:
    std::shared_ptr<DirectX::Model> m_model;
    std::unique_ptr<DirectX::EffectTextureFactory> m_textureFactory;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexResource;

    size_t length = 0;
};