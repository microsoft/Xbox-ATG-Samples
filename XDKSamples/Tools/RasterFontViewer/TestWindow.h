//--------------------------------------------------------------------------------------
// TestWindow.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <functional>
#include <Windows.h>

namespace ATG
{
    // Window class for displaying and testing GDI graphics
    class TestWindow
    {
    public:
        using PaintCallback = std::function<void(HDC)>;
        TestWindow(const PaintCallback &onPaint);
    
    private:
        static LRESULT CALLBACK s_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
        LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
        void OnPaint(HWND hWnd);

        PaintCallback m_onPaint;
    };

} // namespace ATG