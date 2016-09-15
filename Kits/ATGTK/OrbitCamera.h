//--------------------------------------------------------------------------------------
// File: OrbitCamera.h
//
// Helper for implementing Model-View camera
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright(c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#include <memory.h>
#include "GamePad.h"


namespace DirectX
{
    class Keyboard;
    class Mouse;

    struct BoundingSphere;
    struct BoundingBox;
}

namespace DX
{
    class OrbitCamera
    {
    public:
        OrbitCamera();

        OrbitCamera(OrbitCamera&& moveFrom);
        OrbitCamera& operator= (OrbitCamera&& moveFrom);

        OrbitCamera(OrbitCamera const&) = delete;
        OrbitCamera& operator=(OrbitCamera const&) = delete;

        virtual ~OrbitCamera();

        // Perform per-frame update using gamepad controls
        //
        //           Right Thumbstick: Orbit Right, Left, Up, Down
        //          Left Thumbstick X: Roll Z
        //          Left Thumbstick Y: Increase/decrease orbit radius
        //     Left Thumbstick Button: Return to default focus/radius (frame extents)
        //     Right Thumstick Button: Reset camera
        // Left/Right Shoulder Button: Increase/decrease/reset translation sensitivity
        //                       DPad: Translate X/Y
        //
        void Update(float elapsedTime, const DirectX::GamePad::State& pad);

        // Perform per-frame update using keyboard & mouse controls
        //
        //          WASD & Arrow Keys: Translate X/Y
        //          PageUp / PageDown: Translate Z
        //                        End: Return to default focus/radius (frame extents)
        //                       Home: Reset camera
        //
        //         Hold Left Mouse Button: Orbit X/Y
        //        Hold Right Mouse Button: Translate X/Y (Shift Translates Z)
        //                   Scroll wheel: Increase/decrease orbit radius
        //
        void Update(float elapsedTime, DirectX::Mouse& mouse, DirectX::Keyboard& kb);

        // Reset camera to default view
        void Reset();

        // Set projection window (pixels)
        void SetWindow(int width, int height);

        // Set projection parameters
        void SetProjectionParameters(float fov, float nearDistance, float farDistance, bool lhcoords = false);

        // Set model-view radius and radius limits
        void SetRadius(float defaultRadius, float minRadius = 1.f, float maxRadius = FLT_MAX);

        // Set translation sensitivity and limits
        void SetSensitivity(float defaultSensitivity, float minSensitivity, float maxSensitivity, float stepSensitivity);

        // Set rotation rate
        void SetRotationRate(float rotRate);

        // Set radius rate
        void SetRadiusRate(float radiusRate);

        // Set focus bounds (manually set properties are assumed to be in bounds)
        void SetBoundingBox(const DirectX::BoundingBox& box);

        // Manually set initial/default focus/rotation
        void XM_CALLCONV SetFocus(DirectX::FXMVECTOR focus);
        void XM_CALLCONV SetRotation(DirectX::FXMVECTOR rotation);

        // Sets initial/default focus and radius to view a bounding volume
        void SetFrameExtents(const DirectX::BoundingSphere& sphere);
        void SetFrameExtents(const DirectX::BoundingBox& box);

        // Behavior control flags
        static const unsigned int c_FlagsDisableTranslation = 0x1;          // Disables all translation controls
        static const unsigned int c_FlagsDisableRollZ = 0x2;                // Disable roll in Z
        static const unsigned int c_FlagsArrowKeys_XZ = 0x4;                // WASD: Instead of translate X/Y, do translate in X/Z
        static const unsigned int c_FlagsArrowKeysOrbit = 0x8;              // WASD: Orbit X/Y instead of translate, Q/E roll in Z
        static const unsigned int c_FlagsDisableRadiusControl = 0x10;       // Disable radius controls
        static const unsigned int c_FlagsDisableSensitivityControl = 0x20;  // Disable sensitivity controls
        static const unsigned int c_FlagsDisableFrameExtentsReset = 0x40;   // Disable frame extents controls

        void SetFlags(unsigned int flags);

        // Returns view and projection matrices for camera
        DirectX::XMMATRIX GetView() const;
        DirectX::XMMATRIX GetProjection() const;

        // Returns the current focus point
        DirectX::XMVECTOR GetFocus() const;

        // Returns the current camera position
        DirectX::XMVECTOR GetPosition() const;

        // Returns current behavior control flags
        unsigned int GetFlags() const;

    private:
        // Private implementation.
        class Impl;

        struct aligned_deleter { void operator()(void* p) { _aligned_free(p); } };

        std::unique_ptr<Impl, aligned_deleter> pImpl;
    };
}