//--------------------------------------------------------------------------------------
// MouseCursor.cpp
//
// MouseCursor UWP sample
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "MouseCursor.h"

extern void ExitSample();

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

namespace
{
    const float c_RotationGain = 0.004f; // sensitivity adjustment
}

Sample::Sample() :
    m_isAbsolute(true),
    m_isRelative(false),
    m_isClipCursor(false),
    m_highlightFPS(false),
    m_highlightRTS(false),
    m_eyeFPS( 0.f, 20.f, -20.f ),
    m_targetFPS( 0.f, 20.f, 0.f ),
    m_eyeRTS( 0.f, 300.f, 0.f ),
    m_targetRTS( 0.01f, 300.1f, 0.01f ),
    m_eye( 0.f, 20.f, 0.f ),
    m_target( 0.01f, 20.1f, 0.01f ),
    m_pitch( 0 ),
    m_yaw( 0 )	
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
    m_deviceResources->RegisterDeviceNotify(this);
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize( IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation )
{
    m_deviceResources->SetWindow(window, width, height, rotation);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

}

#pragma region Frame Update
// Executes basic render loop.
void Sample::Tick()
{
    m_timer.Tick( [ & ] ()
    {
        Update( m_timer );
    } );

    Render();
}

// Updates the world.
void Sample::Update( DX::StepTimer const&)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    // In clip cursor mode implement screen scrolling when the mouse is near the edge of the screen
    if (m_isClipCursor)
    {
        if (m_screenLocation.X < 20.f)
            MoveRight(-25.f);

        else if (m_screenLocation.X > m_deviceResources->GetLogicalOutputSize().right - m_deviceResources->GetLogicalOutputSize().left - 20.f)
            MoveRight(25.f);

        if (m_screenLocation.Y < 20.f)
            MoveForward(25.f);
        else if (m_screenLocation.Y > m_deviceResources->GetLogicalOutputSize().bottom - m_deviceResources->GetLogicalOutputSize().top - 20.f)
            MoveForward(-25.f);
    }

    PIXEndEvent();
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Sample::Render()
{

    // Don't try to render anything before the first Update.
    if ( m_timer.GetFrameCount() == 0 )
    {
        return;
    }

    Clear();

    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Render");

    m_spriteBatch->Begin();

    std::wstring text = L"+";
    
    const wchar_t* output = text.c_str();
    Vector2 origin = m_font->MeasureString( output ) / 2.f;

    m_fontPos.x = m_screenLocation.X;
    m_fontPos.y = m_screenLocation.Y;

    if ( m_isAbsolute )
    {
        m_spriteBatch->Draw( m_texture_background.Get(), m_fullscreenRect );
        m_spriteBatch->Draw( m_texture_tile.Get(), m_FPStile );
        m_spriteBatch->Draw( m_texture_tile.Get(), m_RTStile );
        
        if ( m_highlightFPS )
        {
            m_spriteBatch->Draw( m_texture_tile_border.Get(), m_FPStile );
        }
        else if ( m_highlightRTS )
        {
            m_spriteBatch->Draw( m_texture_tile_border.Get(), m_RTStile );
        }

        std::wstring text1 = L"Mouse Cursor Sample: ";

        switch (this->m_deviceResources->GetRotation())
        {
        case DXGI_MODE_ROTATION_IDENTITY:
            text1 += L"0";
            break;
        case DXGI_MODE_ROTATION_ROTATE90:
            text1 += L"90";
            break;
        case DXGI_MODE_ROTATION_ROTATE180:
            text1 += L"180";
            break;
        case DXGI_MODE_ROTATION_ROTATE270:
            text1 += L"270";
            break;
        }

        const wchar_t* outputTitle = text1.c_str();
        Vector2 originTitle = m_font64->MeasureString( outputTitle ) / 2.f;
        std::wstring text2 = L"Choose a game mode";
        const wchar_t* outputSubtitle = text2.c_str();
        Vector2 originSubtitle = m_font32->MeasureString( outputSubtitle ) / 2.f;

        std::wstring text3 = L"First-person \n   Shooter";
        const wchar_t* outputFPS = text3.c_str();
        Vector2 originFPS = m_font28->MeasureString( outputFPS ) / 2.f;
        std::wstring text4 = L"Real-time \n Strategy";
        const wchar_t* outputRTS = text4.c_str();
        Vector2 originRTS = m_font28->MeasureString( outputRTS ) / 2.f;

        m_font64->DrawString( m_spriteBatch.get(), outputTitle, m_fontPosTitle, Colors::White, 0.f, originTitle );
        m_font32->DrawString( m_spriteBatch.get(), outputSubtitle, m_fontPosSubtitle, Colors::White, 0.f, originSubtitle );
        m_font28->DrawString( m_spriteBatch.get(), outputFPS, m_fontPosFPS, Colors::White, 0.f, originFPS );
        m_font28->DrawString( m_spriteBatch.get(), outputRTS, m_fontPosRTS, Colors::White, 0.f, originRTS );

    }
    else
    {
        m_font->DrawString( m_spriteBatch.get(), output, m_fontPos, Colors::White, 0.f, origin );
        
        if ( m_isRelative )
        {
            m_modelFPS->Draw( m_deviceResources->GetD3DDeviceContext(), *m_states, m_world, m_view, m_proj );
        }
        else if ( m_isClipCursor )
        {
            m_modelRTS->Draw( m_deviceResources->GetD3DDeviceContext(), *m_states, m_world, m_view, m_proj );
        }
    }

    m_spriteBatch->End();

    PIXEndEvent(context);

    // Show the new frame.
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    PIXEndEvent();
}

// Helper method to clear the back buffers.
void Sample::Clear()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views
    auto renderTarget = m_deviceResources->GetRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    context->ClearRenderTargetView(renderTarget, Colors::CornflowerBlue);
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
void Sample::OnActivated()
{
}

void Sample::OnDeactivated()
{
}

void Sample::OnSuspending()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    context->ClearState();

    m_deviceResources->Trim();
}

void Sample::OnResuming()
{
    m_timer.ResetElapsedTime();
}

void Sample::OnWindowSizeChanged( int width, int height, DXGI_MODE_ROTATION rotation )
{
    if ( !m_deviceResources->WindowSizeChanged( width, height, rotation ) )
        return;

    CreateWindowSizeDependentResources();
}

void Sample::ValidateDevice()
{
    m_deviceResources->ValidateDevice();
}

// Properties
void Sample::GetDefaultSize( int& width, int& height ) const
{
    width = 1280;
    height = 720;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Sample::CreateDeviceDependentResources()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto device = m_deviceResources->GetD3DDevice();

    // Device dependent object initialization ( independent of window size )
    m_font = std::make_unique<SpriteFont>(device, L"Courier_36.spritefont");
    m_font64 = std::make_unique<SpriteFont>(device, L"SegoeUI_34.spritefont");
    m_font32 = std::make_unique<SpriteFont>(device, L"SegoeUI_24.spritefont");
    m_font28 = std::make_unique<SpriteFont>(device, L"SegoeUI_22.spritefont");
    m_spriteBatch = std::make_unique<SpriteBatch>(context);

    m_states = std::make_unique<CommonStates>(device);
    m_fxFactory = std::make_unique<EffectFactory>(device);

    m_modelFPS = Model::CreateFromSDKMESH( device, L"FPSRoom.sdkmesh", *m_fxFactory, true );

    // Note that this model uses 32-bit index buffers so it can't be used w/ Feature Level 9.1
    m_modelRTS = Model::CreateFromSDKMESH(device, L"3DRTSMap.sdkmesh", *m_fxFactory, true );

    DX::ThrowIfFailed( 
        CreateWICTextureFromFile(device, L"Assets//background_flat.png",
            nullptr, m_texture_background.ReleaseAndGetAddressOf() ) );
    DX::ThrowIfFailed( 
        CreateWICTextureFromFile(device, L"Assets//green_tile.png", nullptr,
            m_texture_tile.ReleaseAndGetAddressOf() ) );
    DX::ThrowIfFailed( 
        CreateWICTextureFromFile(device, L"Assets//green_tile_border.png", nullptr,
            m_texture_tile_border.ReleaseAndGetAddressOf() ) );

    m_world = Matrix::Identity;
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    // Initialization of background image
    m_fullscreenRect = m_deviceResources->GetLogicalOutputSize();

    UINT backBufferWidth = static_cast<UINT>(m_fullscreenRect.right - m_fullscreenRect.left);
    UINT backBufferHeight = static_cast<UINT>(m_fullscreenRect.bottom - m_fullscreenRect.top);

    // Update pointer location
    if (m_isRelative)
    {
        m_screenLocation.X = backBufferWidth / 2.f;
        m_screenLocation.Y = backBufferHeight / 2.f;
    }
   
    // Initialize UI tiles and font locations
    m_FPStile.left = ( LONG ) ( 0.325f*backBufferWidth );
    m_FPStile.top =  ( LONG ) ( 0.44f*backBufferHeight );
    m_FPStile.right = ( LONG ) ( 0.495f*backBufferWidth );
    m_FPStile.bottom = ( LONG ) ( 0.66f*backBufferHeight );
    m_FPStile.bottom = std::max( m_FPStile.bottom, m_FPStile.top + 150 );
    m_FPStile.left = std::min( m_FPStile.left, ( LONG ) ( m_FPStile.right - ( ( m_FPStile.bottom - m_FPStile.top ) * 4 / 3.f ) ) );	

    m_RTStile.left = ( LONG ) ( 0.505f*backBufferWidth );
    m_RTStile.top = m_FPStile.top;
    m_RTStile.right = ( LONG ) ( 0.675f*backBufferWidth );
    m_RTStile.bottom = m_FPStile.bottom;
    m_RTStile.right = std::max( m_RTStile.right, ( LONG )( m_RTStile.left + ( ( m_RTStile.bottom - m_RTStile.top ) * 4 / 3.f ) ) );

    m_fontPos.x = backBufferWidth / 2.f;
    m_fontPos.y = backBufferHeight / 2.f;

    m_fontPosTitle.x = backBufferWidth / 2.f;
    m_fontPosTitle.y = backBufferHeight * 0.27f;

    m_fontPosSubtitle.x = backBufferWidth / 2.f;
    m_fontPosSubtitle.y = backBufferHeight * 0.36f;

    m_fontPosFPS.x = m_FPStile.left + ( m_FPStile.right - m_FPStile.left )/2.f;
    m_fontPosFPS.y = m_FPStile.top + ( m_FPStile.bottom - m_FPStile.top ) / 2.f;

    m_fontPosRTS.x = m_RTStile.left + ( m_RTStile.right - m_RTStile.left )/2.f;
    m_fontPosRTS.y = m_fontPosFPS.y; 

    m_spriteBatch->SetRotation( m_deviceResources->GetRotation() );
}

void Sample::OnDeviceLost()
{
    m_font.reset();
    m_font64.reset();
    m_font32.reset();
    m_font28.reset();
    m_spriteBatch.reset();
    m_states.reset();
    m_fxFactory.reset();
    m_modelFPS.reset();
    m_modelRTS.reset();
    m_texture_background.Reset();
    m_texture_tile.Reset();
    m_texture_tile_border.Reset();
}

void Sample::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion

// Update the pointer location in clip cursor mode
void Sample::UpdatePointer( Windows::Foundation::Point screen )
{
    m_screenLocation = screen;
}

// Change the target value based on the mouse movement for move-look/relative mouse mode
void Sample::UpdateCamera( Vector3 movement )
{
    // Adjust pitch and yaw based on the mouse movement
    Vector3 rotationDelta = movement * c_RotationGain;
    m_pitch += rotationDelta.y;
    m_yaw += rotationDelta.x;

    // Limit to avoid looking directly up or down
    const float limit = XM_PI / 2.0f - 0.01f;
    m_pitch = std::max(-limit, std::min(+limit, m_pitch));

    if ( m_yaw > XM_PI )
    {
        m_yaw -= XM_PI * 2.f;
    }
    else if ( m_yaw < -XM_PI )
    {
        m_yaw += XM_PI * 2.f;
    }

    float y = sinf( m_pitch );
    float r = cosf( m_pitch );
    float z = r*cosf( m_yaw );
    float x = r*sinf( m_yaw );

    m_target = m_eye + Vector3( x, y, z );

    SetView();
}

// Move the camera forward or backward
void Sample::MoveForward( float amount )
{
    Vector3 movement = m_target - m_eye;
    movement.y = 0.f;
    Vector3 m_eye_temp = m_eye - amount*movement;

    if ( ( m_eye_temp.z < -1*m_eye_temp.x + 400 ) && ( m_eye_temp.z < m_eye_temp.x + 800 ) 
        && ( m_eye_temp.z > -1 * m_eye_temp.x - 300 ) && ( m_eye_temp.z > m_eye_temp.x - 800 ) )
    {
        m_eye = m_eye_temp;
        m_target = m_target - amount*movement;

        SetView();
    }
}

// Move the camera to the right or left 
void Sample::MoveRight( float amount )
{
    Vector3 movement1 = m_target - m_eye;
    movement1.y = 0.f;
    Vector3 movement;
    movement.x = -movement1.z;
    movement.y = 0.f;
    movement.z = movement1.x;


    Vector3 m_eye_temp = m_eye + amount*movement;

    if ( ( m_eye_temp.z < ( -1 * m_eye_temp.x + 400 ) ) && ( m_eye_temp.z < ( m_eye_temp.x + 800 ) )
        && ( m_eye_temp.z > ( -1 * m_eye_temp.x - 300 ) ) && ( m_eye_temp.z > ( m_eye_temp.x - 800 ) ) )
    {
        m_eye = m_eye_temp;
        m_target = m_target + amount*movement;

        SetView();
    }
}

// Update the viewport based on the updated eye and target values
void Sample::SetView()
{
    UINT backBufferWidth = static_cast<UINT>( m_deviceResources->GetLogicalOutputSize().right - m_deviceResources->GetLogicalOutputSize().left );
    UINT backBufferHeight = static_cast<UINT>( m_deviceResources->GetLogicalOutputSize().bottom - m_deviceResources->GetLogicalOutputSize().top );

    m_view = Matrix::CreateLookAt( m_eye, m_target, Vector3::UnitY );
    Matrix proj = XMMatrixPerspectiveFovLH( XM_PI / 4.f, float( backBufferWidth ) / float( backBufferHeight ), 0.1f, 10000.f );

    m_proj = proj * m_deviceResources->GetOrientationTransform3D();
}

// Set mode to relative ( 1 ), absolute ( 0 ), or clip cursor ( 2 )
Sample::MouseMode Sample::SetMode( Windows::Foundation::Point mouseLocation )
{
    // If entering FPS or relative mode
    if ( mouseLocation.X < m_FPStile.right && mouseLocation.X > m_FPStile.left
        && mouseLocation.Y > m_FPStile.top && mouseLocation.Y < m_FPStile.bottom )
    {
        UINT backBufferWidth = static_cast<UINT>( m_deviceResources->GetLogicalOutputSize().right - m_deviceResources->GetLogicalOutputSize().left );
        UINT backBufferHeight = static_cast<UINT>( m_deviceResources->GetLogicalOutputSize().bottom - m_deviceResources->GetLogicalOutputSize().top );

        m_screenLocation.X = backBufferWidth / 2.f;
        m_screenLocation.Y = backBufferHeight / 2.f;

        m_isRelative = true;
        m_isAbsolute = false;
        m_isClipCursor = false;
        m_highlightFPS = false;
        m_highlightRTS = false;
        m_world = Matrix::CreateRotationX( XM_PI / 2.f )*Matrix::CreateRotationY( XM_PI );
        m_eye = m_eyeFPS;
        m_target = m_targetFPS;
        UpdateCamera( Vector3::Zero );
        SetView();
        return RELATIVE_MOUSE;
    }
    // If entering RTS or clipCursor mode
    else if ( mouseLocation.X < m_RTStile.right && mouseLocation.X > m_RTStile.left
        && mouseLocation.Y > m_RTStile.top && mouseLocation.Y < m_RTStile.bottom )
    {
        m_isRelative = false;
        m_isAbsolute = false;
        m_isClipCursor = true;
        m_highlightFPS = false;
        m_highlightRTS = false;
        m_world = Matrix::CreateRotationX( XM_PI / 2.f )*Matrix::CreateRotationY( 5 * XM_PI / 4 );
        m_eye = m_eyeRTS;
        m_target = m_targetRTS;
        SetView();
        return CLIPCURSOR_MOUSE;
    }
    // Entering absolute mode
    else
    {
        if ( m_isClipCursor )
        {
            m_eyeRTS = m_eye;
            m_targetRTS = m_target;
        }
        m_isRelative = false;
        m_isAbsolute = true;
        m_isClipCursor = false;
        return ABSOLUTE_MOUSE;
    }
}

// When the mouse moves, check to see if it is on top of the FPS or RTS selection tiles
void Sample::CheckLocation( Windows::Foundation::Point mouseLocation )
{
    if ( m_isAbsolute )
    {
        // If hovering over FPS or relative selection
        if ( mouseLocation.X < m_FPStile.right && mouseLocation.X > m_FPStile.left
            && mouseLocation.Y > m_FPStile.top && mouseLocation.Y < m_FPStile.bottom )
        {
            m_highlightFPS = true;
            m_highlightRTS = false;
        }
        // If hovering over RTS or clipCursor selection
        else if ( mouseLocation.X < m_RTStile.right && mouseLocation.X > m_RTStile.left
            && mouseLocation.Y > m_RTStile.top && mouseLocation.Y < m_RTStile.bottom )
        {
            m_highlightRTS = true;
            m_highlightFPS = false;
        }
        else
        {
            m_highlightFPS = false;
            m_highlightRTS = false;
        }
    }
}
