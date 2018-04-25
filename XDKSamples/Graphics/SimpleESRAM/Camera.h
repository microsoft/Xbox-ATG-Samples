//--------------------------------------------------------------------------------------
// Camera.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <GamePad.h>
#include <Keyboard.h>
#include <Mouse.h>

namespace ATG
{
    class Camera
    {
    public:
        Camera();
        Camera(
            float theta, 
            float phi, 
            float distance,
            float znear,
            float zfar,
            float aspect);

        Camera(const Camera&) = default;
        Camera& operator=(const Camera&) = default;

        Camera(Camera&&) = default;
        Camera& operator=(Camera&&) = default;

        ~Camera() = default;

        void XM_CALLCONV SetLookAt(DirectX::XMVECTOR point);

        void SetMoveRate(float rate);
        void SetRotateRate(float rate);

        void SetRadialConstraints(float minRadius, float maxRadius);

        void SetPerspective(float aspect, float znear, float zfar);
        void SetAspect(float aspect);

        void Update(float deltaTime, const DirectX::GamePad::State& pad);
        void Update(float deltaTime, const DirectX::Mouse& mouse, DirectX::Keyboard& kb);

        DirectX::XMMATRIX XM_CALLCONV GetView() const;
        DirectX::XMMATRIX XM_CALLCONV GetProjection() const;

    private:
        // Orientation
        DirectX::XMFLOAT4 m_lookat;

        // Position
        float m_theta;
        float m_phi;
        float m_radius;

        // Movement
        float m_rotateRate;
        float m_moveRate;

        // Constraints
        float m_minRadius;
        float m_maxRadius;

        // Projection
        float m_near;
        float m_far;
        float m_aspect;
    };
}
