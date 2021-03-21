#pragma once
#include <Windows.h>
#include <string>

class RenderSample;

class Win32Application {
public:
  int Run(RenderSample *render, HINSTANCE hInstance, UINT nCmdShow);

private:
int InitWindow();
static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT uMsg, WPARAM wp, LPARAM lp);
LRESULT CALLBACK MsgProc(HWND hwnd, UINT uMsg, WPARAM wp, LPARAM lp);
virtual LRESULT OnResize();
virtual LRESULT OnMouseEvent(UINT uMsg, WPARAM wParam, int x, int y);
virtual LRESULT OnKeyEvent(WPARAM wParam, LPARAM lParam);

// App handle
HINSTANCE m_hAppInst;
// Windows misc
HWND m_hMainWnd;
UINT m_iClientWidth;
UINT m_iClientHeight;
std::wstring m_MainWndCaption;

UINT m_uWndSizeState;       /* Window sizing state */
bool m_bAppPaused;          /* Is window paused */
bool m_bFullScreen;         /* Is window full screen displaying */
};