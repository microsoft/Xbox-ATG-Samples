#pragma once


// Assign a name to the object to aid with debugging.
#if defined(_DEBUG) || defined(DBG)
inline void SetName(ID3D12Object* pObject, LPCWSTR name)
{
    pObject->SetName(name);
}
inline void SetNameIndexed(ID3D12Object* pObject, LPCWSTR name, UINT index)
{
    WCHAR fullName[50];
    if (swprintf_s(fullName, L"%s[%u]", name, index) > 0)
    {
        pObject->SetName(fullName);
    }
}
#else
inline void SetName(ID3D12Object*, LPCWSTR)
{
}
inline void SetNameIndexed(ID3D12Object*, LPCWSTR, UINT)
{
}
#endif

#define NAME_D3D12_OBJECT(x) SetName((x).Get(), L#x)

// Round up size to the next 32 bit multiple.
inline constexpr UINT RoundUp32(const UINT size)
{
    return ((size - 1) / sizeof(UINT32) + 1);
}

// Align to a certain value of power of 2.
inline constexpr UINT Align(const UINT size, const UINT alignment)
{
    assert(((alignment - 1) & alignment) == 0);
    return (size + (alignment - 1)) & ~(alignment - 1);
}

// Align to a certain value.
inline constexpr UINT AlignArbitrary(const UINT size, const UINT alignment)
{
    return size - 1 - (size - 1) % alignment + alignment;
}

inline constexpr UINT GetNumGrps(const UINT size, const UINT numThreads)
{
    return (size + numThreads - 1) / numThreads;
}

// Align a constant buffer.
inline constexpr UINT CalculateConstantBufferByteSize(const UINT size)
{
    return Align(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
}

// Pretty-print a state object tree.
inline void PrintStateObjectDesc(const D3D12_STATE_OBJECT_DESC* desc)
{
    std::wstringstream wstr;
    wstr << L"\n";
    wstr << L"--------------------------------------------------------------------\n";
    wstr << L"| D3D12 State Object 0x" << static_cast<const void*>(desc) << L": ";
    if (desc->Type == D3D12_STATE_OBJECT_TYPE_COLLECTION) wstr << L"Collection\n";
    if (desc->Type == D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE) wstr << L"Raytracing Pipeline\n";

    auto ExportTree = [](UINT depth, UINT numExports, const D3D12_EXPORT_DESC* exports)
    {
        std::wostringstream woss;
        for (UINT i = 0; i < numExports; i++)
        {
            woss << L"|";
            if (depth > 0)
            {
                for (UINT j = 0; j < 2 * depth - 1; j++) woss << L" ";
            }
            woss << L" [" << i << L"]: ";
            if (exports[i].ExportToRename) woss << exports[i].ExportToRename << L" --> ";
            woss << exports[i].Name << L"\n";
        }
        return woss.str();
    };

    for (UINT i = 0; i < desc->NumSubobjects; i++)
    {
        wstr << L"| [" << i << L"]: ";
        switch (desc->pSubobjects[i].Type)
        {
        case D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE:
            wstr << L"Global Root Signature 0x" << desc->pSubobjects[i].pDesc << L"\n";
            break;
        case D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE:
            wstr << L"Local Root Signature 0x" << desc->pSubobjects[i].pDesc << L"\n";
            break;
        case D3D12_STATE_SUBOBJECT_TYPE_NODE_MASK:
            wstr << L"Node Mask: 0x" << std::hex << std::setfill(L'0') << std::setw(8) << *static_cast<const UINT*>(desc->pSubobjects[i].pDesc) << std::setw(0) << std::dec << L"\n";
            break;
        case D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY:
        {
            wstr << L"DXIL Library 0x";
            auto lib = static_cast<const D3D12_DXIL_LIBRARY_DESC*>(desc->pSubobjects[i].pDesc);
            wstr << lib->DXILLibrary.pShaderBytecode << L", " << lib->DXILLibrary.BytecodeLength << L" bytes\n";
            wstr << ExportTree(1, lib->NumExports, lib->pExports);
            break;
        }
        case D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION:
        {
            wstr << L"Existing Library 0x";
            auto collection = static_cast<const D3D12_EXISTING_COLLECTION_DESC*>(desc->pSubobjects[i].pDesc);
            wstr << collection->pExistingCollection << L"\n";
            wstr << ExportTree(1, collection->NumExports, collection->pExports);
            break;
        }
        case D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION:
        {
            wstr << L"Subobject to Exports Association (Subobject [";
            auto association = static_cast<const D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION*>(desc->pSubobjects[i].pDesc);
            UINT index = static_cast<UINT>(association->pSubobjectToAssociate - desc->pSubobjects);
            wstr << index << L"])\n";
            for (UINT j = 0; j < association->NumExports; j++)
            {
                wstr << L"|  [" << j << L"]: " << association->pExports[j] << L"\n";
            }
            break;
        }
        case D3D12_STATE_SUBOBJECT_TYPE_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION:
        {
            wstr << L"DXIL Subobjects to Exports Association (";
            auto association = static_cast<const D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION*>(desc->pSubobjects[i].pDesc);
            wstr << association->SubobjectToAssociate << L")\n";
            for (UINT j = 0; j < association->NumExports; j++)
            {
                wstr << L"|  [" << j << L"]: " << association->pExports[j] << L"\n";
            }
            break;
        }
        case D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG:
        {
            wstr << L"Raytracing Shader Config\n";
            auto config = static_cast<const D3D12_RAYTRACING_SHADER_CONFIG*>(desc->pSubobjects[i].pDesc);
            wstr << L"|  [0]: Max Payload Size: " << config->MaxPayloadSizeInBytes << L" bytes\n";
            wstr << L"|  [1]: Max Attribute Size: " << config->MaxAttributeSizeInBytes << L" bytes\n";
            break;
        }
        case D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG:
        {
            wstr << L"Raytracing Pipeline Config\n";
            auto config = static_cast<const D3D12_RAYTRACING_PIPELINE_CONFIG*>(desc->pSubobjects[i].pDesc);
            wstr << L"|  [0]: Max Recursion Depth: " << config->MaxTraceRecursionDepth << L"\n";
            break;
        }
        case D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP:
        {
            wstr << L"Hit Group (";
            auto hitGroup = static_cast<const D3D12_HIT_GROUP_DESC*>(desc->pSubobjects[i].pDesc);
            wstr << (hitGroup->HitGroupExport ? hitGroup->HitGroupExport : L"[none]") << L")\n";
            wstr << L"|  [0]: Any Hit Import: " << (hitGroup->AnyHitShaderImport ? hitGroup->AnyHitShaderImport : L"[none]") << L"\n";
            wstr << L"|  [1]: Closest Hit Import: " << (hitGroup->ClosestHitShaderImport ? hitGroup->ClosestHitShaderImport : L"[none]") << L"\n";
            wstr << L"|  [2]: Intersection Import: " << (hitGroup->IntersectionShaderImport ? hitGroup->IntersectionShaderImport : L"[none]") << L"\n";
            break;
        }
        }
        wstr << L"|--------------------------------------------------------------------\n";
    }
    wstr << L"\n";
    OutputDebugStringW(wstr.str().c_str());
}

// Allocate buffer and fill with input data.
inline void AllocateUploadBuffer(
    ID3D12Device* pDevice,
    void* pData,
    UINT64 datasize,
    ID3D12Resource** ppResource,
    const wchar_t* resourceName = nullptr)
{
    auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(datasize);
    DX::ThrowIfFailed(pDevice->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(ppResource)));
    if (resourceName)
    {
        (*ppResource)->SetName(resourceName);
    }

    if (pData)
    {
        void *pMappedData;
        (*ppResource)->Map(0, nullptr, &pMappedData);
        memcpy(pMappedData, pData, datasize);
        (*ppResource)->Unmap(0, nullptr);
    }
}

// Allocate texture2D and upload data to the GPU.
inline void AllocateTexture2D(
    ID3D12Device* pDevice,
    void *pData,
    ID3D12Resource **ppResource,
    ID3D12Resource **ppUploadResource,
    ID3D12GraphicsCommandList* commandList,
    UINT width,
    UINT height,
    UINT stride,
    DXGI_FORMAT format = DXGI_FORMAT_R32G32B32A32_FLOAT)
{
    auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto texDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1);
    DX::ThrowIfFailed(pDevice->CreateCommittedResource(
        &defaultHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(ppResource)));

    auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC buffer = CD3DX12_RESOURCE_DESC::Buffer(GetRequiredIntermediateSize(*ppResource, 0, 1));

    DX::ThrowIfFailed(pDevice->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &buffer,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(ppUploadResource)));

    // Upload the texture info to the GPU.
    D3D12_SUBRESOURCE_DATA resource;
    resource.pData = pData;
    resource.RowPitch = width * stride;
    resource.SlicePitch = resource.RowPitch * height;

    UpdateSubresources(commandList, *ppResource, *ppUploadResource, 0, 0, 1, &resource);

    // Change state to read.
    D3D12_RESOURCE_BARRIER barrier[] = {
        CD3DX12_RESOURCE_BARRIER::Transition(*ppResource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ)
    };
    commandList->ResourceBarrier(_countof(barrier), barrier);
}

inline void AllocateTexture2DArr(
    ID3D12Device* pDevice,
    ID3D12Resource **ppResource,
    UINT width,
    UINT height,
    UINT arr,
    D3D12_CLEAR_VALUE* pClear = nullptr,
    DXGI_FORMAT format = DXGI_FORMAT_R32G32B32A32_FLOAT,
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_GENERIC_READ,
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
{
    auto texDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, (UINT16)arr, 1, 1, 0, flags);
    CD3DX12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    DX::ThrowIfFailed(pDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        state,
        pClear,
        IID_PPV_ARGS(ppResource)));
}

// Allocate UAV Buffer.
inline void AllocateUAVBuffer(
    ID3D12Device* pDevice,
    UINT64 bufferSize,
    ID3D12Resource **ppResource,
    D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
    const wchar_t* resourceName = nullptr)
{
    if (bufferSize == 0)
    {
        *ppResource = nullptr;
        return;
    }

    auto defaultProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    DX::ThrowIfFailed(pDevice->CreateCommittedResource(
        &defaultProperties,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        initialResourceState,
        nullptr,
        IID_PPV_ARGS(ppResource)));
    if (resourceName)
    {
        (*ppResource)->SetName(resourceName);
    }
}

// Allocate Buffer.
inline void AllocateBuffer(
    ID3D12Device* pDevice,
    UINT64 bufferSize,
    ID3D12Resource **ppResource,
    D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
    const wchar_t* resourceName = nullptr)
{
    auto defaultProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
    DX::ThrowIfFailed(pDevice->CreateCommittedResource(
        &defaultProperties,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        initialResourceState,
        nullptr,
        IID_PPV_ARGS(ppResource)));
    if (resourceName)
    {
        (*ppResource)->SetName(resourceName);
    }
}

// Allocate Readback Buffer.
inline void AllocateReadBackBuffer(
    ID3D12Device* pDevice,
    UINT64 bufferSize,
    ID3D12Resource **ppResource,
    const wchar_t* resourceName = nullptr)
{
    auto defaultProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
    DX::ThrowIfFailed(pDevice->CreateCommittedResource(
        &defaultProperties,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(ppResource)));
    if (resourceName)
    {
        (*ppResource)->SetName(resourceName);
    }
}

// Create CBV.
inline void CreateCBV(
    ID3D12Device* pDevice,
    ID3D12Resource* pResources,
    UINT sizeInBytes,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle)
{
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = pResources->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = sizeInBytes;

    pDevice->CreateConstantBufferView(&cbvDesc, cpuDescriptorHandle);
}

// Create rtv.
inline void CreateRTV(
    ID3D12Device* pDevice,
    ID3D12Resource* pResources,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle,
    D3D12_RENDER_TARGET_VIEW_DESC* rtvDesc = nullptr)
{
    pDevice->CreateRenderTargetView(pResources, rtvDesc, cpuDescriptorHandle);
}

// Create dsv.
inline void CreateDSV(
    ID3D12Device* pDevice,
    ID3D12Resource* pResources,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle,
    D3D12_DEPTH_STENCIL_VIEW_DESC* dsvDesc = nullptr
)
{
    pDevice->CreateDepthStencilView(pResources, dsvDesc, cpuDescriptorHandle);
}

// Create uav.
inline void CreateUAV(
    ID3D12Device* pDevice,
    ID3D12Resource* pResources,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle,
    D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc = nullptr,
    ID3D12Resource* pCounterResource = nullptr)
{
    pDevice->CreateUnorderedAccessView(
        pResources,
        pCounterResource,
        uavDesc,
        cpuDescriptorHandle
    );
}

// Create srv.
inline void CreateSRV(
    ID3D12Device* pDevice,
    ID3D12Resource* pResources,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle,
    D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc = nullptr)
{
    pDevice->CreateShaderResourceView(
        pResources,
        srvDesc,
        cpuDescriptorHandle
    );
}

// Create SRV for a buffer.
inline void CreateBufferSRV(
    ID3D12Device* pDevice,
    ID3D12Resource* pResources,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle,
    UINT firstElement,
    UINT numElements,
    UINT elementSize,
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN,
    D3D12_BUFFER_SRV_FLAGS flags = D3D12_BUFFER_SRV_FLAG_NONE)
{
    // SRV
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = format;
    srvDesc.Buffer.NumElements = numElements;
    srvDesc.Buffer.Flags = flags;
    srvDesc.Buffer.StructureByteStride = elementSize;
    srvDesc.Buffer.FirstElement = firstElement;

    CreateSRV(pDevice, pResources, cpuDescriptorHandle, &srvDesc);
}

// Create SRV for a texture2D.
inline void CreateTexture2DSRV(
    ID3D12Device* pDevice,
    ID3D12Resource* pResources,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle,
    DXGI_FORMAT format = DXGI_FORMAT_R32G32B32A32_FLOAT,
    UINT mipLevels = 1)
{
    // SRV
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = format;
    srvDesc.Texture2D.MipLevels = mipLevels;

    CreateSRV(pDevice, pResources, cpuDescriptorHandle, &srvDesc);
};

inline void CreateTexture2DUAV(
    ID3D12Device* pDevice,
    ID3D12Resource* pResources,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle,
    DXGI_FORMAT format = DXGI_FORMAT_R32G32B32A32_FLOAT,
    UINT mipSlice = 0)
{
    // UAV
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Format = format;
    uavDesc.Texture2D.MipSlice = mipSlice;

    CreateUAV(pDevice, pResources, cpuDescriptorHandle, &uavDesc);
}

inline void CreateTexture2DArrSRV(
    ID3D12Device* pDevice,
    ID3D12Resource* pResources,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle,
    UINT arraySize,
    DXGI_FORMAT format = DXGI_FORMAT_R32G32B32A32_FLOAT,
    UINT mipLevels = 1)
{
    // SRV
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = format;
    srvDesc.Texture2DArray.ArraySize = arraySize;
    srvDesc.Texture2DArray.MipLevels = mipLevels;

    CreateSRV(pDevice, pResources, cpuDescriptorHandle, &srvDesc);
}

inline void CreateTexture2DArrUAV(
    ID3D12Device* pDevice,
    ID3D12Resource* pResources,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle,
    UINT arraySize,
    DXGI_FORMAT format = DXGI_FORMAT_R32G32B32A32_FLOAT)
{
    // UAV
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
    uavDesc.Format = format;
    uavDesc.Texture2DArray.ArraySize = arraySize;

    CreateUAV(pDevice, pResources, cpuDescriptorHandle, &uavDesc);
}

// Create sampler for a texture2D.
inline void CreateTexture2DSampler(
    ID3D12Device* pDevice,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle,
    D3D12_FILTER filter,
    D3D12_TEXTURE_ADDRESS_MODE addressU,
    D3D12_TEXTURE_ADDRESS_MODE addressV,
    D3D12_TEXTURE_ADDRESS_MODE addressW,
    FLOAT* borderColor = nullptr,
    D3D12_COMPARISON_FUNC compFunc = D3D12_COMPARISON_FUNC_ALWAYS,
    UINT maxAnisotropy = 1,
    FLOAT minLod = 0,
    FLOAT maxLod = D3D12_FLOAT32_MAX
)
{
    D3D12_SAMPLER_DESC samplerDescNoWrap = {};
    samplerDescNoWrap.Filter = filter;
    samplerDescNoWrap.AddressU = addressU;
    samplerDescNoWrap.AddressV = addressV;
    samplerDescNoWrap.AddressW = addressW;
    samplerDescNoWrap.MinLOD = minLod;
    samplerDescNoWrap.MaxLOD = maxLod;
    samplerDescNoWrap.MipLODBias = 0.0f;
    samplerDescNoWrap.MaxAnisotropy = maxAnisotropy;
    samplerDescNoWrap.ComparisonFunc = compFunc;

    if (borderColor != nullptr)
        memcpy(samplerDescNoWrap.BorderColor, borderColor, sizeof(samplerDescNoWrap.BorderColor));

    pDevice->CreateSampler(&samplerDescNoWrap, cpuDescriptorHandle);
}

// Create root signature.
inline void SerializeAndCreateRootSignature(
    ID3D12Device* device,
    D3D12_ROOT_SIGNATURE_DESC& desc,
    Microsoft::WRL::ComPtr<ID3D12RootSignature>& rootSig)
{
    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    DX::ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error));
    DX::ThrowIfFailed(device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(rootSig.GetAddressOf())));
}

union Param
{
    Param(FLOAT f) : f(f) {}
    Param(UINT u) : u(u) {}
    Param(INT i) : i(i) {}

    void operator= (FLOAT pf) { f = pf; }
    void operator= (UINT pu) { u = pu; }
    void operator= (INT pi) { i = pi; }

    union
    {
        FLOAT f;
        UINT u;
        INT i;
    };
};