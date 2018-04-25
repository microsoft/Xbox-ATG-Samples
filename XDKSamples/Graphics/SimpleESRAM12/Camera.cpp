//--------------------------------------------------------------------------------------
// Camera.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Camera.h"

using namespace ATG;
using namespace DirectX;

Camera::Camera()
    : m_lookat(0.0f, 0.0f, 0.0f, 0.0f)
    , m_theta(0.0f)
    , m_phi(XM_2PI / 6.0f)
    , m_radius(3.3f)
    , m_rotateRate(XM_PI)
    , m_moveRate(5.0f)
    , m_minRadius(1.0f)
    , m_maxRadius(10.0f)
    , m_near(0.05f)
    , m_far(100.0f)
    , m_aspect(16.0f/9.0f)
{ }

Camera::Camera(float theta, float phi, float distance, float znear, float zfar, float aspect)
    : m_theta(theta)
    , m_phi(phi)
    , m_radius(distance)
    , m_near(znear)
    , m_far(zfar)
    , m_aspect(aspect)
{ }

void Camera::SetPerspective(float aspect, float znear, float zfar)
{
    m_aspect = aspect;
    m_near = znear;
    m_far = zfar;
}

void Camera::SetAspect(float aspect)
{
    m_aspect = aspect;
}

void Camera::SetLookAt(DirectX::XMVECTOR point)
{
    XMStoreFloat4(&m_lookat, point);
}

void Camera::SetMoveRate(float rate)
{
    m_moveRate = std::max(0.1f, rate);
}

void Camera::SetRotateRate(float rate)
{
    m_rotateRate = std::max(0.1f, rate);
}

void Camera::SetRadialConstraints(float minRadius, float maxRadius)
{
    m_minRadius = std::max(1e-2f, minRadius);
    m_maxRadius = std::max(m_minRadius, maxRadius);
}

void Camera::Update(float deltaTime, const DirectX::GamePad::State& pad)
{
    m_theta += pad.thumbSticks.rightX * m_rotateRate * deltaTime;
    m_phi = std::max(1e-2f, std::min(XM_PIDIV2, m_phi - pad.thumbSticks.rightY * m_rotateRate * deltaTime));
    m_radius = std::max(m_minRadius, std::min(m_maxRadius, m_radius - pad.thumbSticks.leftY * m_moveRate * deltaTime));
}

void Camera::Update(float deltaTime, const DirectX::Mouse& mouse, DirectX::Keyboard& kb)
{
    Keyboard::State kbState = kb.GetState();
    Mouse::State mouseState = mouse.GetState();

    float deltaTheta = -1.0f * float(kbState.A) + 1.0f * float(kbState.D);
    float deltaPhi = -1.0f * float(kbState.S) + 1.0f * float(kbState.W);
    float deltaRadius = mouseState.scrollWheelValue < 0 ? -1.0f : mouseState.scrollWheelValue > 0 ? 1.0f : 0.0f;

    m_theta += deltaTheta * m_rotateRate * deltaTime;
    m_phi = std::max(1e-2f, std::min(XM_PIDIV2, m_phi - deltaPhi * m_rotateRate * deltaTime));
    m_radius = std::max(m_minRadius, std::min(m_maxRadius, m_radius - deltaRadius * m_moveRate * deltaTime));
}

DirectX::XMMATRIX Camera::GetView() const
{
    DirectX::XMVECTOR pos = XMVectorSet(
        m_radius * sinf(m_phi) * cosf(m_theta), 
        m_radius * cosf(m_phi), 
        m_radius * sinf(m_phi) * sinf(m_theta), 
        0);

    return XMMatrixLookAtLH(pos, XMLoadFloat4(&m_lookat), XMVectorSet(0, 1, 0, 0));
}

DirectX::XMMATRIX Camera::GetProjection() const
{
    return XMMatrixPerspectiveFovLH(XM_PIDIV4, m_aspect, m_near, m_far);
}
