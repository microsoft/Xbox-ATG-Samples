//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once

#define SizeOfInUint32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)

// Align a constant buffer.
inline constexpr unsigned int CalculateRaytracingRecordByteSize(const unsigned int size)
{
    return Align(size, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
}

// Enable experimental features required for compute-based raytracing fallback.
// This will set active D3D12 devices to DEVICE_REMOVED state.
// Returns bool whether the call succeeded and the device supports the feature.
inline bool EnableComputeRaytracingFallback(IDXGIAdapter1* adapter)
{
    Microsoft::WRL::ComPtr<ID3D12Device> testDevice;
    UUID experimentalFeatures[] = { D3D12ExperimentalShaderModels };

    return SUCCEEDED(D3D12EnableExperimentalFeatures(1, experimentalFeatures, nullptr, nullptr))
        && SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&testDevice)));
}

inline bool IsDirectXRaytracingSupported(IDXGIAdapter1* adapter)
{
    Microsoft::WRL::ComPtr<ID3D12Device> testDevice;
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 featureSupportData = {};

    return SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&testDevice)))
        && SUCCEEDED(testDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &featureSupportData, sizeof(featureSupportData)))
        && featureSupportData.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
}


inline void SerializeAndCreateRaytracingRootSignature(
    ID3D12RaytracingFallbackDevice* fallbackDevice,
    D3D12_ROOT_SIGNATURE_DESC& desc,
    Microsoft::WRL::ComPtr<ID3D12RootSignature>* rootSig)
{
    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    Microsoft::WRL::ComPtr<ID3DBlob> error;

    DX::ThrowIfFailed(fallbackDevice->D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), error ? static_cast<wchar_t*>(error->GetBufferPointer()) : nullptr);
    DX::ThrowIfFailed(fallbackDevice->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig))));
}

// Shader record = {{Shader ID}, {RootArguments}}
class ShaderRecord
{
public:
    ShaderRecord(void* pShaderIdentifier, unsigned int shaderIdentifierSize) :
        shaderIdentifier(pShaderIdentifier, shaderIdentifierSize)
    {
    }

    ShaderRecord(void* pShaderIdentifier, unsigned int shaderIdentifierSize, void* pLocalRootArguments, unsigned int localRootArgumentsSize) :
        shaderIdentifier(pShaderIdentifier, shaderIdentifierSize),
        localRootArguments(pLocalRootArguments, localRootArgumentsSize)
    {
    }

    void CopyTo(void* dest) const
    {
        uint8_t* byteDest = static_cast<uint8_t*>(dest);
        memcpy(byteDest, shaderIdentifier.ptr, shaderIdentifier.size);
        if (localRootArguments.ptr)
        {
            memcpy(byteDest + shaderIdentifier.size, localRootArguments.ptr, localRootArguments.size);
        }
    }

    struct PointerWithSize {
        void *ptr;
        unsigned int size;

        PointerWithSize() : ptr(nullptr), size(0) {}
        PointerWithSize(void* _ptr, unsigned int _size) : ptr(_ptr), size(_size) {};
    };
    PointerWithSize shaderIdentifier;
    PointerWithSize localRootArguments;
};

// Shader table = {{ ShaderRecord 1}, {ShaderRecord 2}, ...}
class ShaderTable
{
    Microsoft::WRL::ComPtr<ID3D12Resource> m_bufferResource;

    uint8_t* m_mappedShaderRecords;
    unsigned int m_shaderRecordSize;

    bool closed;

    // Debug support
    std::wstring m_name;
    std::vector<ShaderRecord> m_shaderRecords;

    ShaderTable() : closed(FALSE) {}
public:
    ShaderTable(ID3D12Device* device, unsigned int numShaderRecords, unsigned int shaderRecordSize, LPCWSTR resourceName = nullptr)
        : m_name(resourceName), closed(FALSE)
    {
        m_shaderRecordSize = CalculateRaytracingRecordByteSize(shaderRecordSize);
        m_shaderRecords.reserve(numShaderRecords);
        unsigned int bufferSize = numShaderRecords * m_shaderRecordSize;
        AllocateUploadBuffer(device, nullptr, bufferSize, &m_bufferResource, resourceName);

        // Map the data.
        CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU.
        DX::ThrowIfFailed(m_bufferResource->Map(0, &readRange, reinterpret_cast<void**>(&m_mappedShaderRecords)));
    }

    void Add(const ShaderRecord& shaderRecord)
    {
        DX::ThrowIfFalse(!closed, L"Cannot add to a closed ShaderTable.");
        DX::ThrowIfFalse(m_shaderRecords.size() < m_shaderRecords.capacity());
        m_shaderRecords.push_back(shaderRecord);
        shaderRecord.CopyTo(m_mappedShaderRecords);
        m_mappedShaderRecords += m_shaderRecordSize;
    }

    void Close()
    {
        DX::ThrowIfFalse(!closed, L"Cannot close an already closed ShaderTable.");
        closed = TRUE;

        m_bufferResource->Unmap(0, nullptr);
    }

    Microsoft::WRL::ComPtr<ID3D12Resource> GetResource(){ return m_bufferResource; };

    unsigned int GetShaderRecordSize() { return m_shaderRecordSize; }

    // Pretty-print the shader records.
    void DebugPrint(std::unordered_map<void*, std::wstring> shaderIdToStringMap)
    {
        std::wstringstream wstr;
        wstr << L"|--------------------------------------------------------------------\n";
        wstr << L"|Shader table - " << m_name.c_str() << L": "
            << m_shaderRecordSize << L" | "
            << m_shaderRecords.size() * m_shaderRecordSize << L" bytes\n";

        for (unsigned int i = 0; i < m_shaderRecords.size(); i++)
        {
            wstr << L"| [" << i << L"]: ";
            wstr << shaderIdToStringMap[m_shaderRecords[i].shaderIdentifier.ptr] << L", ";
            wstr << m_shaderRecords[i].shaderIdentifier.size << L" + " << m_shaderRecords[i].localRootArguments.size << L" bytes \n";
        }
        wstr << L"|--------------------------------------------------------------------\n";
        wstr << L"\n";
        OutputDebugStringW(wstr.str().c_str());
    }
};