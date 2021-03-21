#include "Win32Application.h"
#include "Common.h"


int Win32Application::InitWindow() {
  int ret;
  WNDCLASSEX wcx = { sizeof(WNDCLASSEX) };
  RECT rect;
  int width, height;
  HICON hIcon = NULL;
  WCHAR szResourcePath[MAX_PATH];

  if(FindDemoMediaFileAbsPath(L"Media/Icons/vulkan.ico", _countof(szResourcePath), szResourcePath) == 0) {
    hIcon = (HICON)LoadImage(m_hAppInst, szResourcePath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
  }

  wcx.style = CS_HREDRAW | CS_VREDRAW;
  wcx.lpfnWndProc = Win32Application::MainWndProc;
  wcx.cbClsExtra = 0;
  wcx.cbWndExtra = 0;
  wcx.hInstance = m_hAppInst;
  wcx.hIcon = hIcon;
  wcx.hIconSm = hIcon;
  wcx.hCursor = (HCURSOR)LoadCursor(NULL, IDC_ARROW);
  wcx.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
  wcx.lpszClassName = NULL;
  wcx.lpszClassName = L"VulkanMainWnd";

  if (!RegisterClassEx(&wcx)) {
    return -1;
  }

  rect = { 0, 0, (LONG)m_iClientWidth, (LONG)m_iClientHeight };
  AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);
  width = rect.right - rect.left;
  height = rect.bottom - rect.top;

  m_hMainWnd = CreateWindow(wcx.lpszClassName, m_MainWndCaption.c_str(),
    WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height,
    NULL, NULL, wcx.hInstance, (LPVOID)this);
  if (!m_hMainWnd) {
    return -1;
  }

  ShowWindow(m_hMainWnd, SW_SHOW);
  UpdateWindow(m_hMainWnd);

  return 0;
}

LRESULT CALLBACK Win32Application::MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {

  switch(msg) (

  )

  return s_pVkApp->MsgProc(hwnd, msg, wp, lp);
}

LRESULT Win32Application::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {

  switch (msg) {
    // WM_ACTIVATE is sent when the window is activated or deactivated.  
    // We pause the game when the window is deactivated and unpause it 
    // when it becomes active.  
  case WM_ACTIVATE:
    if (LOWORD(wParam) == WA_INACTIVE) {
      m_bAppPaused = true;
      m_GameTimer.Stop();
    } else {
      m_bAppPaused = false;
      m_GameTimer.Start();
    }
    return 0;

    // WM_SIZE is sent when the user resizes the window.  
  case WM_SIZE:
    // Save the new client area dimensions.
    m_iClientWidth = LOWORD(lParam);
    m_iClientHeight = HIWORD(lParam);
    if (m_pDevice) {
      if (wParam == SIZE_MINIMIZED) {
        m_bAppPaused = true;
        m_uWndSizeState = SIZE_MINIMIZED;
      } else if (wParam == SIZE_MAXIMIZED) {
        m_bAppPaused = false;
        m_uWndSizeState = SIZE_MAXIMIZED;
        OnResize();
      } else if (wParam == SIZE_RESTORED) {

        // Restoring from minimized state?
        if (m_uWndSizeState == SIZE_MINIMIZED) {
          m_bAppPaused = false;
          m_uWndSizeState = SIZE_RESTORED;
          OnResize();
        }

        // Restoring from maximized state?
        else if (m_uWndSizeState == SIZE_MAXIMIZED) {
          m_bAppPaused = false;
          m_uWndSizeState = SIZE_RESTORED;
          OnResize();
        } else if (m_uWndSizeState & 0x10) {
          // If user is dragging the resize bars, we do not resize 
          // the buffers here because as the user continuously 
          // drags the resize bars, a stream of WM_SIZE messages are
          // sent to the window, and it would be pointless (and slow)
          // to resize for each WM_SIZE message received from dragging
          // the resize bars.  So instead, we reset after the user is 
          // done resizing the window and releases the resize bars, which 
          // sends a WM_EXITSIZEMOVE message.
        } else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
        {
          OnResize();
        }
      }
    }
    return 0;

    // WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
  case WM_ENTERSIZEMOVE:
    m_bAppPaused = true;
    m_uWndSizeState = 0x10;
    m_GameTimer.Stop();
    return 0;

    // WM_EXITSIZEMOVE is sent when the user releases the resize bars.
    // Here we reset everything based on the new window dimensions.
  case WM_EXITSIZEMOVE:
    m_bAppPaused = false;
    m_uWndSizeState = SIZE_RESTORED;
    m_GameTimer.Start();
    OnResize();
    return 0;

    // WM_DESTROY is sent when the window is being destroyed.
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;

    // The WM_MENUCHAR message is sent when a menu is active and the user presses 
    // a key that does not correspond to any mnemonic or accelerator key. 
  case WM_MENUCHAR:
    // Don't beep when we alt-enter.
    return MAKELRESULT(0, MNC_CLOSE);

    // Catch this message so to prevent the window from becoming too small.
  case WM_GETMINMAXINFO:
    ((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
    ((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
    return 0;

  case WM_LBUTTONDOWN:
  case WM_MBUTTONDOWN:
  case WM_RBUTTONDOWN:
    OnMouseEvent(msg, wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
    return 0;
  case WM_LBUTTONUP:
  case WM_MBUTTONUP:
  case WM_RBUTTONUP:
    OnMouseEvent(msg, wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
    return 0;
  case WM_MOUSEMOVE:
    OnMouseEvent(msg, wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
    return 0;
  case WM_IME_KEYDOWN:
  case WM_KEYDOWN:
    OnKeyEvent(wParam, lParam);
    return 0;
  case WM_KEYUP:
    if (wParam == VK_ESCAPE) {
      PostQuitMessage(0);
    } else if ((int)wParam == VK_F2)
      Set4xMsaaEnabled(!m_aDeviceConfig.MsaaEnabled);

    return 0;
  }

  return DefWindowProc(hwnd, msg, wParam, lParam);
}
