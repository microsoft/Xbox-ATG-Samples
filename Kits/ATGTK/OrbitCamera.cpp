//--------------------------------------------------------------------------------------
// File: OrbitCamera.cpp
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright(c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include "pch.h"
#include "OrbitCamera.h"

#include "GamePad.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "SimpleMath.h"

using namespace DirectX;
using namespace DX;

#pragma warning(disable : 4324)

namespace
{
    // Ken Shoemake, "Arcball Rotation Control", Graphics Gems IV, pg 176 - 192  
    class __declspec(align(16)) ArcBall
    {
    public:
        ArcBall() :
            m_width(800.f),
            m_height(400.f),
            m_radius(1.f),
            m_drag(false)
        {
            Reset();
        }

        void Reset()
        {
            m_qdown = m_qnow = XMQuaternionIdentity();
        }

        void OnBegin(int x, int y, DirectX::FXMVECTOR quat)
        {
            m_drag = true;
            m_qdown = quat;
            m_downPoint = ScreenToVector(float(x), float(y));
        }

        void OnMove(int x, int y)
        {
            using namespace DirectX;
            if (m_drag)
            {
                XMVECTOR curr = ScreenToVector(float(x), float(y));

                m_qnow = XMQuaternionMultiply(m_qdown, QuatFromBallPoints(m_downPoint, curr));
                m_qnow = XMQuaternionNormalize(m_qnow);
            }
        }

        void OnEnd()
        {
            m_drag = false;
        }
        void SetWindow(int width, int height)
        {
            m_width = float(width);
            m_height = float(height);
        }

        void SetRadius(float radius)
        {
            m_radius = radius;
        }

        XMVECTOR GetQuat() const { return m_qnow; }

        bool IsDragging() const { return m_drag; }

    private:
        float                           m_width;
        float                           m_height;
        float                           m_radius;
        XMVECTOR                        m_qdown;
        XMVECTOR                        m_qnow;
        XMVECTOR                        m_downPoint;
        bool                            m_drag;

        DirectX::XMVECTOR ScreenToVector(float screenx, float screeny)
        {
            float x = -(screenx - m_width / 2.f) / (m_radius * m_width / 2.f);
            float y = (screeny - m_height / 2.f) / (m_radius * m_height / 2.f);

            float z = 0.0f;
            float mag = x * x + y * y;

            if (mag > 1.0f)
            {
                float scale = 1.0f / sqrtf(mag);
                x *= scale;
                y *= scale;
            }
            else
                z = sqrtf(1.0f - mag);

            return DirectX::XMVectorSet(x, y, z, 0);
        }

        static DirectX::XMVECTOR QuatFromBallPoints(DirectX::FXMVECTOR vFrom, DirectX::FXMVECTOR vTo)
        {
            using namespace DirectX;
            XMVECTOR dot = XMVector3Dot(vFrom, vTo);
            XMVECTOR vPart = XMVector3Cross(vFrom, vTo);
            return XMVectorSelect(dot, vPart, g_XMSelect1110);
        }
    };
}


class __declspec(align(16)) OrbitCamera::Impl
{
public:
    Impl() :
        m_nearDistance(0.1f),
        m_farDistance(10000.f),
        m_fov(XM_PI / 4.f),
        m_defaultSensitivity(1.f),
        m_minSensitivity(0.01f),
        m_maxSensitivity(10.f),
        m_stepSensitivity(0.01f),
        m_defaultRadius(5.f),
        m_minRadius(1.f),
        m_maxRadius(FLT_MAX),
        m_rotRate(1.f),
        m_radiusRate(1.f),
        m_flags(0),
        m_lhcoords(false),
#ifdef _XBOX_ONE
        m_width(1920),
        m_height(1080)
#else
        m_width(1280),
        m_height(720)
#endif
    {
        m_view = m_projection = XMMatrixIdentity();
        m_cameraPosition = m_homeFocus = g_XMZero;
        m_homeRotation = g_XMIdentityR3;

        m_bounds.Center.x = m_bounds.Center.y = m_bounds.Center.z = 0.f;
        m_bounds.Extents.x = m_bounds.Extents.y = m_bounds.Extents.z = FLT_MAX;

        Reset();
    }

    void Update(float elapsedTime, const GamePad::State& pad)
    {
        using namespace DirectX::SimpleMath;

        float handed = (m_lhcoords) ? 1.f : -1.f;

        Matrix im = XMMatrixInverse(nullptr, GetView());

        if (!(m_flags & c_FlagsDisableTranslation))
        {
            // Translate camera
            Vector3 move = Vector3::Zero;

            if (pad.IsDPadUpPressed())
            {
                move.y += 1.f;
            }
            else if (pad.IsDPadDownPressed())
            {
                move.y -= 1.f;
            }

            if (pad.IsDPadLeftPressed())
            {
                move.x -= 1.f;
            }
            else if (pad.IsDPadRightPressed())
            {
                move.x += 1.f;
            }

            if (move.x != 0 || move.y != 0)
            {
                move = Vector3::TransformNormal(move, im);

                m_focus += move * elapsedTime * m_sensitivity;

                Vector3 minBound = m_bounds.Center - m_bounds.Extents;
                Vector3 maxBound = m_bounds.Center + m_bounds.Extents;
                m_focus = XMVectorMax(minBound, XMVectorMin(maxBound, m_focus));

                m_viewDirty = true;
            }
        }
        
        // Rotate camera
        Vector3 orbit(pad.thumbSticks.rightX, pad.thumbSticks.rightY, pad.thumbSticks.leftX);
        orbit *= elapsedTime * m_rotRate;

        if (orbit.x != 0 || orbit.y != 0 || orbit.z != 0)
        {
            m_cameraRotation = XMQuaternionMultiply(m_cameraRotation, XMQuaternionRotationAxis(im.Right(), orbit.y * handed));
            m_cameraRotation = XMQuaternionMultiply(m_cameraRotation, XMQuaternionRotationAxis(im.Up(), -orbit.x * handed));

            if (!(m_flags & c_FlagsDisableRollZ))
            {
                m_cameraRotation = XMQuaternionMultiply(m_cameraRotation, XMQuaternionRotationAxis(im.Forward(), orbit.z));
            }

            m_cameraRotation = XMQuaternionNormalize(m_cameraRotation);
            m_viewDirty = true;
        }

        if (!(m_flags & c_FlagsDisableRadiusControl))
        {
            if (pad.thumbSticks.leftY != 0)
            {
                m_radius -= pad.thumbSticks.leftY * elapsedTime * m_radiusRate;
                m_radius = std::max(m_minRadius, std::min(m_maxRadius, m_radius));
                m_viewDirty = true;
            }
        }

        // Other controls
        if (pad.IsLeftShoulderPressed() && pad.IsRightShoulderPressed())
        {
            m_sensitivity = m_defaultSensitivity;

#ifdef _DEBUG
            (void)GetView();

            char buff[128] = {};
            Vector4 tmp = m_cameraPosition;
            sprintf_s(buff, "cameraPosition = { %2.2ff, %2.2ff, %2.2ff, 0.f};\n", tmp.x, tmp.y, tmp.z);
            OutputDebugStringA(buff);

            tmp = m_cameraRotation;
            sprintf_s(buff, "cameraRotation = { %2.2ff, %2.2ff, %2.2ff, %2.2ff};\n", tmp.x, tmp.y, tmp.z, tmp.w );
            OutputDebugStringA(buff);
#endif
        }
        else if (!(m_flags & c_FlagsDisableSensitivityControl))
        {
            if (pad.IsRightShoulderPressed())
            {
                m_sensitivity += m_stepSensitivity;
                if (m_sensitivity > m_maxSensitivity)
                    m_sensitivity = m_maxSensitivity;
            }
            else if (pad.IsLeftShoulderPressed())
            {
                m_sensitivity -= m_stepSensitivity;
                if (m_sensitivity < m_minSensitivity)
                    m_sensitivity = m_minSensitivity;
            }
        }

        if (pad.IsRightStickPressed())
        {
            Reset();
        }

        if (pad.IsLeftStickPressed() && !(m_flags & c_FlagsDisableFrameExtentsReset))
        {
            m_radius = m_defaultRadius;
            m_focus = m_homeFocus;
            m_viewDirty = true;
        }
    }

    void Update(float elapsedTime, Mouse& mouse, Keyboard& kb)
    {
        using namespace DirectX::SimpleMath;

        float handed = (m_lhcoords) ? 1.f : -1.f;

        Matrix im = XMMatrixInverse(nullptr, GetView());

        auto mstate = mouse.GetState();
        auto kbstate = kb.GetState();

        if ((mstate.positionMode != Mouse::MODE_RELATIVE) && !m_arcBall.IsDragging())
        {
            // Keyboard controls
            if (m_flags & c_FlagsArrowKeysOrbit)
            {
                Vector3 orbit = Vector3::Zero;

                if (kbstate.Up || kbstate.W)
                    orbit.y = 1.f;

                if (kbstate.Down || kbstate.S)
                    orbit.y = -1.f;

                if (kbstate.Right || kbstate.D)
                    orbit.x = 1.f;

                if (kbstate.Left || kbstate.A)
                    orbit.x = -1.f;

                if (kbstate.Q)
                    orbit.z = -1.f;

                if (kbstate.E)
                    orbit.z = 1.f;

                if (orbit.x != 0 || orbit.y != 0 || orbit.z != 0)
                {
                    orbit *= elapsedTime * m_rotRate;

                    m_cameraRotation = XMQuaternionMultiply(m_cameraRotation, XMQuaternionRotationAxis(im.Right(), orbit.y * handed));
                    m_cameraRotation = XMQuaternionMultiply(m_cameraRotation, XMQuaternionRotationAxis(im.Up(), -orbit.x * handed));

                    if (!(m_flags & c_FlagsDisableRollZ))
                    {
                        m_cameraRotation = XMQuaternionMultiply(m_cameraRotation, XMQuaternionRotationAxis(im.Forward(), orbit.z));
                    }

                    m_cameraRotation = XMQuaternionNormalize(m_cameraRotation);
                    m_viewDirty = true;
                }
            }
            else if (!(m_flags & c_FlagsDisableTranslation))
            {
                // Arrow keys & WASD control translation of camera focus
                Vector3 move = Vector3::Zero;

                float scale = m_radius;
                if (kbstate.LeftShift || kbstate.RightShift)
                    scale *= 0.5f;

                if (m_flags & c_FlagsArrowKeys_XZ)
                {
                    if (kbstate.PageUp)
                        move.y += scale;

                    if (kbstate.PageDown)
                        move.y -= scale;

                    if (kbstate.Up || kbstate.W)
                        move.z += scale * handed;

                    if (kbstate.Down || kbstate.S)
                        move.z -= scale * handed;
                }
                else
                {
                    if (kbstate.Up || kbstate.W)
                        move.y += scale;

                    if (kbstate.Down || kbstate.S)
                        move.y -= scale;

                    if (kbstate.PageUp)
                        move.z += scale * handed;

                    if (kbstate.PageDown)
                        move.z -= scale * handed;
                }

                if (kbstate.Right || kbstate.D)
                    move.x += scale;

                if (kbstate.Left || kbstate.A)
                    move.x -= scale;

                if (move.x != 0 || move.y != 0 || move.z != 0)
                {
                    move = Vector3::TransformNormal(move, im);

                    m_focus += move * elapsedTime;

                    Vector3 minBound = m_bounds.Center - m_bounds.Extents;
                    Vector3 maxBound = m_bounds.Center + m_bounds.Extents;
                    m_focus = XMVectorMax(minBound, XMVectorMin(maxBound, m_focus));

                    m_viewDirty = true;
                }
            }

            if (kbstate.Home)
            {
                Reset();
            }
            else if (kbstate.End && !(m_flags & c_FlagsDisableFrameExtentsReset))
            {
                m_radius = m_defaultRadius;
                m_focus = m_homeFocus;
                m_viewDirty = true;
            }
        }

        // Mouse controls
        if (mstate.positionMode == Mouse::MODE_RELATIVE)
        {
            if (!(m_flags & c_FlagsDisableTranslation))
            {
                // Translate camera
                Vector3 delta;
                if (kbstate.LeftShift || kbstate.RightShift)
                {
                    delta = Vector3(0.f, 0.f, -float(mstate.y) * handed) * m_radius * elapsedTime;
                }
                else
                {
                    delta = Vector3(-float(mstate.x), float(mstate.y), 0.f) * m_radius * elapsedTime;
                }

                delta = Vector3::TransformNormal(delta, im);

                m_focus += delta * elapsedTime * m_sensitivity;

                Vector3 minBound = m_bounds.Center - m_bounds.Extents;
                Vector3 maxBound = m_bounds.Center + m_bounds.Extents;
                m_focus = XMVectorMax(minBound, XMVectorMin(maxBound, m_focus));

                m_viewDirty = true;
            }
        }
        else if (m_arcBall.IsDragging())
        {
            // Rotate camera
            m_arcBall.OnMove(mstate.x, mstate.y);
            m_cameraRotation = XMQuaternionInverse(m_arcBall.GetQuat());
            m_viewDirty = true;
        }
        else if (!(m_flags & c_FlagsDisableRadiusControl))
        {
            // Radius with scroll wheel
            m_radius = m_defaultRadius - ((float(mstate.scrollWheelValue) / 120.f) * m_radiusRate);
            m_radius = std::max(m_minRadius, std::min(m_maxRadius, m_radius));
            m_viewDirty = true;
        }

        if (!m_arcBall.IsDragging())
        {
            if (mstate.rightButton && mstate.positionMode == Mouse::MODE_ABSOLUTE)
                mouse.SetMode(Mouse::MODE_RELATIVE);
            else if (!mstate.rightButton && mstate.positionMode == Mouse::MODE_RELATIVE)
                mouse.SetMode(Mouse::MODE_ABSOLUTE);

            if (mstate.leftButton)
            {
                m_arcBall.OnBegin(mstate.x, mstate.y, XMQuaternionInverse(m_cameraRotation));
            }
        }
        else if (!mstate.leftButton)
        {
            m_arcBall.OnEnd();
        }
    }

    void Reset()
    {
        m_focus = m_homeFocus;
        m_radius = m_defaultRadius;
        m_cameraRotation = m_homeRotation;
        m_sensitivity = m_defaultSensitivity;
        m_viewDirty = m_projDirty = true;
        m_arcBall.Reset();
        m_arcBall.OnEnd();
    }

    mutable XMMATRIX        m_view;
    mutable XMMATRIX        m_projection;
    mutable XMVECTOR        m_cameraPosition;

    XMVECTOR                m_focus;
    XMVECTOR                m_homeFocus;

    XMVECTOR                m_cameraRotation;
    XMVECTOR                m_homeRotation;

    float                   m_nearDistance;
    float                   m_farDistance;
    float                   m_fov;
    float                   m_sensitivity;
    float                   m_defaultSensitivity;
    float                   m_minSensitivity;
    float                   m_maxSensitivity;
    float                   m_stepSensitivity;
    float                   m_radius;
    float                   m_defaultRadius;
    float                   m_minRadius;
    float                   m_maxRadius;
    float                   m_rotRate;
    float                   m_radiusRate;
    unsigned int            m_flags;

    bool                    m_lhcoords;
    mutable bool            m_viewDirty;
    mutable bool            m_projDirty;

    int                     m_width;
    int                     m_height;

    DirectX::BoundingBox    m_bounds;

    ArcBall                 m_arcBall;

    XMMATRIX GetView() const
    {
        m_viewDirty = false;

        XMVECTOR dir = XMVector3Rotate((m_lhcoords) ? g_XMNegIdentityR2 : g_XMIdentityR2, m_cameraRotation);
        XMVECTOR up = XMVector3Rotate(g_XMIdentityR1, m_cameraRotation);

        m_cameraPosition = m_focus + m_radius * dir;

        if (m_lhcoords)
        {
            m_view = XMMatrixLookAtLH(m_cameraPosition, m_focus, up);
        }
        else
        {
            m_view = XMMatrixLookAtRH(m_cameraPosition, m_focus, up);
        }

        return m_view;
    }

    XMMATRIX GetProjection() const
    {
        m_projDirty = false;

        float aspectRatio = (m_height > 0.f) ? (float(m_width) / float(m_height)) : 1.f;

        if (m_lhcoords)
        {
            m_projection = XMMatrixPerspectiveFovLH(m_fov, aspectRatio, m_nearDistance, m_farDistance);
        }
        else
        {
            m_projection = XMMatrixPerspectiveFovRH(m_fov, aspectRatio, m_nearDistance, m_farDistance);
        }

        return m_projection;
    }
};


// Public constructor.
OrbitCamera::OrbitCamera()
{
    auto ptr = reinterpret_cast<Impl*>(_aligned_malloc(sizeof(Impl), 16));
    pImpl.reset(new (ptr) Impl);
    pImpl->Reset();
}


// Move constructor.
OrbitCamera::OrbitCamera(OrbitCamera&& moveFrom)
    : pImpl(std::move(moveFrom.pImpl))
{
}


// Move assignment.
OrbitCamera& OrbitCamera::operator= (OrbitCamera&& moveFrom)
{
    pImpl = std::move(moveFrom.pImpl);
    return *this;
}


// Public destructor.
OrbitCamera::~OrbitCamera()
{
}


// Public methods.
void OrbitCamera::Update(float elapsedTime, const GamePad::State& pad)
{
    pImpl->Update(elapsedTime, pad);
}

void OrbitCamera::Update(float elapsedTime, Mouse& mouse, Keyboard& kb)
{
    pImpl->Update(elapsedTime, mouse, kb);
}

void OrbitCamera::Reset()
{
    pImpl->Reset();
}

void OrbitCamera::SetWindow(int width, int height)
{
    pImpl->m_projDirty = true;
    pImpl->m_width = width;
    pImpl->m_height = height;
    pImpl->m_arcBall.SetWindow(width, height);
}

void OrbitCamera::SetProjectionParameters(float fov, float nearDistance, float farDistance, bool lhcoords)
{
    pImpl->m_projDirty = true;
    pImpl->m_fov = fov;
    pImpl->m_nearDistance = nearDistance;
    pImpl->m_farDistance = farDistance;
    pImpl->m_lhcoords = lhcoords;
}

void OrbitCamera::SetFlags(unsigned int flags)
{
    pImpl->m_flags = flags;
}

void OrbitCamera::SetRadius(float defaultRadius, float minRadius, float maxRadius)
{
    pImpl->m_viewDirty = true;
    pImpl->m_radius = pImpl->m_defaultRadius = defaultRadius;
    pImpl->m_minRadius = minRadius;
    pImpl->m_maxRadius = maxRadius;
}

void OrbitCamera::SetSensitivity(float defaultSensitivity, float minSensitivity, float maxSensitivity, float stepSensitivity)
{
    pImpl->m_sensitivity = pImpl->m_defaultSensitivity = defaultSensitivity;
    pImpl->m_minSensitivity = minSensitivity;
    pImpl->m_maxSensitivity = maxSensitivity;
    pImpl->m_stepSensitivity = stepSensitivity;
}

void OrbitCamera::SetRotationRate(float rotRate)
{
    pImpl->m_rotRate = rotRate;
}

void OrbitCamera::SetRadiusRate(float radiusRate)
{
    pImpl->m_radiusRate = radiusRate;
}

void OrbitCamera::SetBoundingBox(const DirectX::BoundingBox& box)
{
    pImpl->m_bounds = box;
}

void XM_CALLCONV OrbitCamera::SetFocus(FXMVECTOR focus)
{
    pImpl->m_viewDirty = true;
    pImpl->m_focus = pImpl->m_homeFocus = focus;
}

void XM_CALLCONV OrbitCamera::SetRotation(FXMVECTOR rotation)
{
    XMVECTOR nr = XMQuaternionNormalize(rotation);

    pImpl->m_viewDirty = true;
    pImpl->m_cameraRotation = pImpl->m_homeRotation = nr;
}

void OrbitCamera::SetFrameExtents(const DirectX::BoundingSphere& sphere)
{
    pImpl->m_viewDirty = true;

    pImpl->m_radius = pImpl->m_defaultRadius = sphere.Radius * 2.f;

    XMVECTOR v = XMLoadFloat3(&sphere.Center);
    pImpl->m_focus = pImpl->m_homeFocus = v;
}

void OrbitCamera::SetFrameExtents(const DirectX::BoundingBox& box)
{
    pImpl->m_viewDirty = true;

    BoundingSphere sphere;
    sphere.Center = box.Center;
    sphere.Radius = std::max(box.Extents.x, std::max(box.Extents.y, box.Extents.z));

    pImpl->m_radius = pImpl->m_defaultRadius = sphere.Radius * 2.f;

    XMVECTOR v = XMLoadFloat3(&sphere.Center);
    pImpl->m_focus = pImpl->m_homeFocus = v;
}


// Public accessors.
XMMATRIX OrbitCamera::GetView() const
{
    if (pImpl->m_viewDirty)
    {
        return pImpl->GetView();
    }
    else
    {
        return pImpl->m_view;
    }
}

XMMATRIX OrbitCamera::GetProjection() const
{
    if (pImpl->m_projDirty)
    {
        return pImpl->GetProjection();
    }
    else
    {
        return pImpl->m_projection;
    }
}

XMVECTOR OrbitCamera::GetFocus() const
{
    return pImpl->m_focus;
}

XMVECTOR OrbitCamera::GetPosition() const
{
    if (pImpl->m_viewDirty)
    {
        (void)pImpl->GetView();
    }
    return pImpl->m_cameraPosition;
}

unsigned int OrbitCamera::GetFlags() const
{
    return pImpl->m_flags;
}