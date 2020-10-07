//--------------------------------------------------------------------------------------
// FrontPanelDolphin.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "FrontPanelDolphin.h"

#include "ATGColors.h"
#include "ReadData.h"

extern void ExitSample() noexcept;

using namespace DirectX;
using namespace ATG;

using Microsoft::WRL::ComPtr;

namespace
{
    // Vertex data structure used to
    // create a constant buffer.
    struct VS_CONSTANT_BUFFER
    {
        XMVECTOR vZero;
        XMVECTOR vConstants;
        XMVECTOR vWeight;

        XMMATRIX matTranspose;
        XMMATRIX matCameraTranspose;
        XMMATRIX matViewTranspose;
        XMMATRIX matProjTranspose;

        XMVECTOR vLight;
        XMVECTOR vLightDolphinSpace;
        XMVECTOR vDiffuse;
        XMVECTOR vAmbient;
        XMVECTOR vFog;
        XMVECTOR vCaustics;
    };

    // Pixel data structure used to
    // create a constant buffer.
    struct PS_CONSTANT_BUFFER
    {
        float fAmbient[4];
        float fFogColor[4];
    };

    static_assert((sizeof(VS_CONSTANT_BUFFER) % 16) == 0, "CB must be 16 byte aligned");
    static_assert((sizeof(PS_CONSTANT_BUFFER) % 16) == 0, "CB must be 16 byte aligned");

    // Custom effect for sea floor
    class SeaEffect : public IEffect
    {
    public:
        SeaEffect(ID3D11Device* device, ID3D11PixelShader* pixelShader) :
            m_pixelShader(pixelShader)
        {
            m_shaderBlob = DX::ReadData(L"SeaFloorVS.cso");
            DX::ThrowIfFailed(device->CreateVertexShader(m_shaderBlob.data(), m_shaderBlob.size(), nullptr, m_vertexShader.ReleaseAndGetAddressOf()));
        }

        virtual void Apply(ID3D11DeviceContext* context) override
        {
            context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
            context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
        }

        virtual void GetVertexShaderBytecode(_Out_ void const** pShaderByteCode, _Out_ size_t* pByteCodeLength) override
        {
            *pShaderByteCode = m_shaderBlob.data();
            *pByteCodeLength = m_shaderBlob.size();
        }

    private:
        ComPtr<ID3D11VertexShader>  m_vertexShader;
        ComPtr<ID3D11PixelShader>   m_pixelShader;
        std::vector<uint8_t>        m_shaderBlob;
    };
}

Sample::Sample() noexcept(false)
    : m_frame(0),
    m_currentCausticTextureView(0)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();

    // Create the dolphins
    for (unsigned int i = 0; i < m_dolphins.size(); i++)
    {
        auto d = std::make_shared<Dolphin>();
        d->Translate(XMVectorSet(0, -1.0f + 2.0f * static_cast<float>(i), 10, 0));
        m_dolphins[i] = d;
    }

    // Set the water color to a nice blue.
    SetWaterColor(0.0f, 0.5f, 1.0f);

    // Set the ambient light.
    m_ambient[0] = 0.25f;
    m_ambient[1] = 0.25f;
    m_ambient[2] = 0.25f;
    m_ambient[3] = 0.25f;

    if (IsXboxFrontPanelAvailable())
    {
        // Construct the front panel render target
        m_frontPanelRenderTarget = std::make_unique<FrontPanelRenderTarget>();
        
        // Get the default front panel
        DX::ThrowIfFailed(GetDefaultXboxFrontPanel(m_frontPanelControl.ReleaseAndGetAddressOf()));
        
        // Initialize the FrontPanelDisplay object
        m_frontPanelDisplay = std::make_unique<FrontPanelDisplay>(m_frontPanelControl.Get());

        // Intiailize the FrontPanelInput object
        m_frontPanelInput = std::make_unique<FrontPanelInput>(m_frontPanelControl.Get());
    }
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(IUnknown* window)
{
    m_gamePad = std::make_unique<GamePad>();

    m_deviceResources->SetWindow(window);

    m_deviceResources->CreateDeviceResources();  
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();
}

#pragma region Frame Update
// Executes basic render loop.
void Sample::Tick()
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Frame %llu", m_frame);

    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();

    PIXEndEvent();
    m_frame++;
}

// Updates the world.
void Sample::Update(DX::StepTimer const& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    float elapsedTime = float(timer.GetElapsedSeconds());
    float totalTime = float(timer.GetTotalSeconds());
    
    // Update all the dolphins
    for (unsigned int i = 0; i < m_dolphins.size(); i++)
        m_dolphins[i]->Update(totalTime, elapsedTime);

    SetWaterColor(0.0f, 0.5f, 1.0f);

    // Animate the caustic textures
    m_currentCausticTextureView = (static_cast<unsigned int>(totalTime *32)) % 32;

    auto context = m_deviceResources->GetD3DDeviceContext();

    D3D11_MAPPED_SUBRESOURCE mappedResource;

    context->Map(m_VSConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    {
        VS_CONSTANT_BUFFER *vertShaderConstData = (VS_CONSTANT_BUFFER*)mappedResource.pData;

        vertShaderConstData->vZero = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
        vertShaderConstData->vConstants = XMVectorSet(1.0f, 0.5f, 0.2f, 0.05f);

        // weight is for dolphins, so the value doesn't matter here (since we're setting it for the sea floor)
        vertShaderConstData->vWeight = XMVectorSet(0, 0, 0, 0);

        // Lighting vectors (in world space and in dolphin model space)
        // and other constants
        vertShaderConstData->vLight = XMVectorSet(0.00f, 1.00f, 0.00f, 0.00f);
        vertShaderConstData->vLightDolphinSpace = XMVectorSet(0.00f, 1.00f, 0.00f, 0.00f);
        vertShaderConstData->vDiffuse = XMVectorSet(1.00f, 1.00f, 1.00f, 1.00f);
        vertShaderConstData->vAmbient = XMVectorSet(m_ambient[0], m_ambient[1], m_ambient[2], m_ambient[3]);
        vertShaderConstData->vFog = XMVectorSet(0.50f, 50.00f, 1.00f / (50.0f - 1.0f), 0.00f);
        vertShaderConstData->vCaustics = XMVectorSet(0.05f, 0.05f, sinf(totalTime) / 8, cosf(totalTime) / 10);

        XMVECTOR vDeterminant;
        XMMATRIX matDolphin = XMMatrixIdentity();
        XMMATRIX matDolphinInv = XMMatrixInverse(&vDeterminant, matDolphin);
        vertShaderConstData->vLightDolphinSpace = XMVector4Normalize(XMVector4Transform(vertShaderConstData->vLight, matDolphinInv));

        // Vertex shader operations use transposed matrices
        XMMATRIX mat, matCamera;
        matCamera = XMMatrixMultiply(matDolphin, XMLoadFloat4x4(&m_matView));
        mat = XMMatrixMultiply(matCamera, m_matProj);
        vertShaderConstData->matTranspose = XMMatrixTranspose(mat);
        vertShaderConstData->matCameraTranspose = XMMatrixTranspose(matCamera);
        vertShaderConstData->matViewTranspose = XMMatrixTranspose(m_matView);
        vertShaderConstData->matProjTranspose = XMMatrixTranspose(m_matProj);

    }
    context->Unmap(m_VSConstantBuffer.Get(), 0);

    context->Map(m_PSConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    {
        PS_CONSTANT_BUFFER* pPsConstData = (PS_CONSTANT_BUFFER*)mappedResource.pData;

        memcpy(pPsConstData->fAmbient, m_ambient, sizeof(m_ambient));
        memcpy(pPsConstData->fFogColor, m_waterColor, sizeof(m_waterColor));
    }
    context->Unmap(m_PSConstantBuffer.Get(), 0);

    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);

        if (pad.IsViewPressed())
        {
            ExitSample();
        }
    }
    else
    {
        m_gamePadButtons.Reset();
    }

    // Use the front panel select button to capture the front panel dispaly
    if (m_frontPanelControl)
    {
        auto fpInput = m_frontPanelInput->GetState();
        m_frontPanelInputButtons.Update(fpInput);

        using ButtonState = FrontPanelInput::ButtonStateTracker::ButtonState;

        if (m_frontPanelInputButtons.buttonSelect == ButtonState::PRESSED)
        {
            m_frontPanelDisplay->SaveDDSToFile(L"D:\\FrontPanelScreen.dds");
        }
    }

    PIXEndEvent();
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Sample::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    // Prepare the render target to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Render");

    SetWaterColor(0.0f, 0.5f, 1.0f);

    // Set state
    context->OMSetBlendState(m_states->Opaque(), 0, 0xffffffff);
    context->RSSetState(m_states->CullNone());
    context->OMSetDepthStencilState(m_states->DepthDefault(), 0);

    ID3D11SamplerState* pSamplers[] = { m_pSamplerMirror.Get(), m_states->LinearWrap() };
    context->PSSetSamplers(0, 2, pSamplers);
    
    context->VSSetConstantBuffers(0, 1, m_VSConstantBuffer.GetAddressOf());
    context->PSSetConstantBuffers(0, 1, m_PSConstantBuffer.GetAddressOf());

    /////////////////////////////////////////////////////
    //
    // Render sea floor
    //
    ////////////////////////////////////////////////////
    assert(!m_seafloor->meshes.empty());
    assert(!m_seafloor->meshes[0]->meshParts.empty());
    m_seafloor->meshes[0]->meshParts[0]->Draw(context, m_seaEffect.get(), m_seaFloorVertexLayout.Get(), [&]
    {
        context->PSSetShaderResources(0, 1, m_seaFloorTextureView.GetAddressOf());
        context->PSSetShaderResources(1, 1, m_causticTextureViews[m_currentCausticTextureView].GetAddressOf());
    });

    /////////////////////////////////////////////////////
    //
    // Render Dolphins  
    //
    ////////////////////////////////////////////////////
    for (unsigned int i = 0; i < m_dolphins.size(); i++)
        DrawDolphin(*m_dolphins[i].get());

    if (m_frontPanelControl)
    {
        // Blit to the Front Panel render target and then present to the Front Panel
        m_frontPanelRenderTarget->GPUBlit(m_deviceResources->GetD3DDeviceContext(), m_mainRenderTargetSRV.Get());
        auto fpDesc = m_frontPanelDisplay->GetBufferDescriptor();
        m_frontPanelRenderTarget->CopyToBuffer(m_deviceResources->GetD3DDeviceContext(), fpDesc);
        m_frontPanelDisplay->Present();
    }

    // Show the new frame.
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    m_graphicsMemory->Commit();

    PIXEndEvent(context);
}

// Helper method to clear the back buffers.
void Sample::Clear()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.
    auto renderTarget = m_deviceResources->GetRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    // Use linear clear color for gamma-correct rendering.
    context->ClearRenderTargetView(renderTarget, m_waterColor);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

    // Set the viewport.
    auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    PIXEndEvent(context);
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Sample::OnSuspending()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    context->Suspend(0);
}

void Sample::OnResuming()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    context->Resume();
    m_timer.ResetElapsedTime();
    m_gamePadButtons.Reset();
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Sample::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto context = m_deviceResources->GetD3DDeviceContext();
    m_graphicsMemory = std::make_unique<GraphicsMemory>(device, m_deviceResources->GetBackBufferCount());

    // Set up for rendering to the front panel via the GPU
    if (m_frontPanelControl)
    {
        // Create the front panel render target resources
        m_frontPanelRenderTarget->CreateDeviceDependentResources(m_frontPanelControl.Get(), device);
    }

    ////////////////////////////////
    //
    // Create constant buffers
    //
    ////////////////////////////////
    {
        D3D11_BUFFER_DESC cbDesc;
        cbDesc.ByteWidth = sizeof(VS_CONSTANT_BUFFER);
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        cbDesc.MiscFlags = 0;

        DX::ThrowIfFailed(device->CreateBuffer(&cbDesc, NULL, m_VSConstantBuffer.GetAddressOf()));

        cbDesc.ByteWidth = sizeof(PS_CONSTANT_BUFFER);

        DX::ThrowIfFailed(device->CreateBuffer(&cbDesc, NULL, m_PSConstantBuffer.GetAddressOf()));
    }

    m_fxFactory = std::make_unique<EffectFactory>(device);
    m_fxFactory->SetDirectory(L"Assets\\textures");

    ////////////////////////////////
    //
    // Load the dolphins
    //
    //////////////////////////////
    for (unsigned int i = 0; i < m_dolphins.size(); i++)
        m_dolphins[i]->Load(device, context, m_fxFactory.get());

    ////////////////////////////////
    //
    // Create the textures resources
    //
    ////////////////////////////////
    m_fxFactory->CreateTexture(L"Seafloor.bmp", context, m_seaFloorTextureView.ReleaseAndGetAddressOf());

    for (DWORD t = 0; t < 32; t++)
    {
        std::wstring path;
        if (t < 10)
        {
            int count = _scwprintf(L"caust0%i.DDS", t);
            path.resize(count + 1);
            swprintf_s(&path[0], path.size(), L"caust0%i.DDS", t);
        }
        else
        {
            int count = _scwprintf(L"caust%i.DDS", t);
            path.resize(count + 1);
            swprintf_s(&path[0], path.size(), L"caust%i.DDS", t);
        }

        m_fxFactory->CreateTexture(path.c_str(), context, m_causticTextureViews[t].ReleaseAndGetAddressOf());
    }

    // Create the common pixel shader
    {
        auto blob = DX::ReadData(L"CausticsPS.cso");
        DX::ThrowIfFailed(device->CreatePixelShader(blob.data(), blob.size(), nullptr, m_pixelShader.ReleaseAndGetAddressOf()));
    }

    ////////////////////////////////
    //
    // Create the mesh resources
    //
    ////////////////////////////////

    // Create sea floor objects
    m_seafloor = Model::CreateFromSDKMESH(device, L"assets\\mesh\\seafloor.sdkmesh", *m_fxFactory);

    m_seaEffect = std::make_unique<SeaEffect>(device, m_pixelShader.Get());

    m_seafloor->meshes[0]->meshParts[0]->CreateInputLayout(device, m_seaEffect.get(), m_seaFloorVertexLayout.ReleaseAndGetAddressOf());

    m_states = std::make_unique<CommonStates>(device);

    {
        D3D11_SAMPLER_DESC desc = {};
        desc.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_MIRROR;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR;
        desc.MaxLOD = D3D11_FLOAT32_MAX;
        desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        DX::ThrowIfFailed(device->CreateSamplerState(&desc, m_pSamplerMirror.ReleaseAndGetAddressOf()));
    }

    // Determine the aspect ratio
    float fAspectRatio = 1920.0f / 1080.0f;

    // Set the transform matrices
    static const XMVECTORF32 c_vEyePt = { 0.0f, 0.0f, -5.0f, 0.0f };
    static const XMVECTORF32 c_vLookatPt = { 0.0f, 0.0f, 0.0f, 0.0f };
    static const XMVECTORF32 c_vUpVec = { 0.0f, 1.0f, 0.0f, 0.0f };

    m_matView = XMMatrixLookAtLH(c_vEyePt, c_vLookatPt, c_vUpVec);
    m_matProj = XMMatrixPerspectiveFovLH(XM_PI / 3, fAspectRatio, 1.0f, 10000.0f);

}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    if (m_frontPanelControl)
    {
        // Create a shader resource view for the main render target
        auto device = m_deviceResources->GetD3DDevice();
        DX::ThrowIfFailed(device->CreateShaderResourceView(m_deviceResources->GetRenderTarget(), nullptr, m_mainRenderTargetSRV.GetAddressOf()));
    }
}
#pragma endregion

void Sample::SetWaterColor(float red, float green, float blue)
{
    m_waterColor[0] = red;
    m_waterColor[1] = green;
    m_waterColor[2] = blue;
    m_waterColor[3] = 1.0f;
}

void Sample::DrawDolphin(Dolphin &dolphin)
{
    auto context = m_deviceResources->GetD3DDeviceContext();

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    context->Map(m_VSConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    {
        VS_CONSTANT_BUFFER* vertShaderConstData = (VS_CONSTANT_BUFFER*)mappedResource.pData;

        vertShaderConstData->vZero = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
        vertShaderConstData->vConstants = XMVectorSet(1.0f, 0.5f, 0.2f, 0.05f);

        FLOAT fBlendWeight = dolphin.GetBlendWeight();
        FLOAT fWeight1;
        FLOAT fWeight2;
        FLOAT fWeight3;

        if (fBlendWeight > 0.0f)
        {
            fWeight1 = fabsf(fBlendWeight);
            fWeight2 = 1.0f - fabsf(fBlendWeight);
            fWeight3 = 0.0f;
        }
        else
        {
            fWeight1 = 0.0f;
            fWeight2 = 1.0f - fabsf(fBlendWeight);
            fWeight3 = fabsf(fBlendWeight);
        }
        vertShaderConstData->vWeight = XMVectorSet(fWeight1, fWeight2, fWeight3, 0.0f);

        // Lighting vectors (in world space and in dolphin model space)
        // and other constants
        vertShaderConstData->vLight = XMVectorSet(0.00f, 1.00f, 0.00f, 0.00f);
        vertShaderConstData->vLightDolphinSpace = XMVectorSet(0.00f, 1.00f, 0.00f, 0.00f);
        vertShaderConstData->vDiffuse = XMVectorSet(1.00f, 1.00f, 1.00f, 1.00f);
        vertShaderConstData->vAmbient = XMVectorSet(m_ambient[0], m_ambient[1], m_ambient[2], m_ambient[3]);
        vertShaderConstData->vFog = XMVectorSet(0.50f, 50.00f, 1.00f / (50.0f - 1.0f), 0.00f);

        float totalSeconds = float(m_timer.GetTotalSeconds());
        vertShaderConstData->vCaustics = XMVectorSet(0.05f, 0.05f, sinf(totalSeconds) / 8, cosf(totalSeconds) / 10);

        XMVECTOR vDeterminant;
        XMMATRIX matDolphin = dolphin.GetWorld();
        XMMATRIX matDolphinInv = XMMatrixInverse(&vDeterminant, matDolphin);
        vertShaderConstData->vLightDolphinSpace = XMVector4Normalize(XMVector4Transform(vertShaderConstData->vLight, matDolphinInv));

        // Vertex shader operations use transposed matrices
        XMMATRIX matCamera = XMMatrixMultiply(matDolphin, m_matView);
        XMMATRIX mat = XMMatrixMultiply(matCamera, m_matProj);
        vertShaderConstData->matTranspose = XMMatrixTranspose(mat);
        vertShaderConstData->matCameraTranspose = XMMatrixTranspose(matCamera);
        vertShaderConstData->matViewTranspose = XMMatrixTranspose(m_matView);
        vertShaderConstData->matProjTranspose = XMMatrixTranspose(m_matProj);

    }
    context->Unmap(m_VSConstantBuffer.Get(), 0);

    context->VSSetConstantBuffers(0, 1, m_VSConstantBuffer.GetAddressOf());
    dolphin.Render(context, m_pixelShader.Get(), m_causticTextureViews[m_currentCausticTextureView].Get());
}
