#include <VulkanRenderContext.hpp>
#include <string>
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#else
#error Unsupport yet
#endif
#include <GLFW/glfw3native.h>
#include <stdio.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

static struct UIState {
  std::string Title;
  int LastMousePos[2];
  bool Captured;

  GameTimer Timer;
  double FrameStatLastTimeStamp;
  uint32_t FrameStatLastFrameCount;
  uint64_t FrameStatTotalFrameCount;
} g_UIState;

static void ProcessKeyStrokesInput(GLFWwindow *window);
static void ReportFrameStats(GLFWwindow *window);
static void OnResizeWindow(GLFWwindow *window, int cx, int cy);
static void OnCursorPosChanged(GLFWwindow *window, double xpos, double ypos);
static void OnMouseButtonEvent(GLFWwindow *window, int button, int action, int mods);
static void OnMouseScroll(GLFWwindow *window, double xoffset, double yoffset);

int RunSample(const char *pTitle, int width, int height, VulkanRenderContext *pRenderContext) {

  VKHRESULT hr;

  GLFWwindow *window;
  std::wstring iconPath;
  float fTime, fElapsed;

  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  g_UIState.Title = pTitle;
  window = glfwCreateWindow(width, height, pTitle, nullptr, nullptr);
  if (!window) {
    return -1;
  }
  glfwSetWindowUserPointer(window, pRenderContext);

  glfwSetFramebufferSizeCallback(window, OnResizeWindow);
  glfwSetCursorPosCallback(window, OnCursorPosChanged);
  glfwSetMouseButtonCallback(window, OnMouseButtonEvent);
  glfwSetScrollCallback(window, OnMouseScroll);

  pRenderContext->CreateVkInstance(pTitle);
#if _WIN32
  pRenderContext->CreateWindowSurface((void *)glfwGetWin32Window(window));
#else
#error Unsupport yet.
#endif
  V(pRenderContext->Initialize());
  if (VK_FAILED(hr)) {
    return -1;
  }
  // Call it for the first time.
  pRenderContext->Resize(width, height);

  g_UIState.Timer.Resume();

  while (!glfwWindowShouldClose(window)) {
    ProcessKeyStrokesInput(window);
    ReportFrameStats(window);

    g_UIState.Timer.Tick();

    fElapsed = (float)g_UIState.Timer.TotalElapsed();
    fTime = (float)g_UIState.Timer.DeltaElasped();

    pRenderContext->Update(fTime, fElapsed);
    pRenderContext->RenderFrame(fTime, fElapsed);

    glfwPollEvents();
  }

  pRenderContext->Destroy();

  glfwTerminate();

  return hr;
}

void ProcessKeyStrokesInput(GLFWwindow *window) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, 1);
  else if(glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS) {
    auto pRenderContext = reinterpret_cast<VulkanRenderContext *>(glfwGetWindowUserPointer(window));
    pRenderContext->SetMsaaEnabled(!pRenderContext->IsMsaaEnabled());
  }
}

void ReportFrameStats(GLFWwindow *window) {
  double timeInterval = 0.0;

  g_UIState.FrameStatLastFrameCount += 1;
  g_UIState.FrameStatTotalFrameCount += 1;

  if ((timeInterval = (g_UIState.Timer.TotalElapsed() - g_UIState.FrameStatLastTimeStamp)) >= 1.0) {
    char buff[128];
    snprintf(buff, _countof(buff), "%s, FPS:%3.1f, MSPF:%.3f", g_UIState.Title.c_str(),
                 (float)(g_UIState.FrameStatLastFrameCount / timeInterval),
                 (float)(timeInterval / g_UIState.FrameStatLastFrameCount));
    g_UIState.FrameStatLastTimeStamp = g_UIState.Timer.TotalElapsed();
    g_UIState.FrameStatLastFrameCount = 0;
    glfwSetWindowTitle(window, buff);
  }
}

void OnResizeWindow(GLFWwindow *window, int cx, int cy) {

  auto pRenderContext = reinterpret_cast<VulkanRenderContext *>(glfwGetWindowUserPointer(window));
  pRenderContext->Resize(cx, cy);
}

void OnCursorPosChanged(GLFWwindow *window, double xpos, double ypos) {
  if (g_UIState.Captured) {
    auto pRenderContext = reinterpret_cast<VulkanRenderContext *>(glfwGetWindowUserPointer(window));

    int dx = (int)xpos - g_UIState.LastMousePos[0];
    int dy = (int)ypos - g_UIState.LastMousePos[1];
    pRenderContext->FrameMoved(dx, dy, nullptr);

    g_UIState.LastMousePos[0] = (int)xpos;
    g_UIState.LastMousePos[1] = (int)ypos;
  }
}

void OnMouseButtonEvent(GLFWwindow *window, int button, int action, int mods) {
  if (button == GLFW_MOUSE_BUTTON_LEFT) {
    g_UIState.Captured = action == GLFW_PRESS;
    if (g_UIState.Captured) {
      double xpos, ypos;
      glfwGetCursorPos(window, &xpos, &ypos);
      g_UIState.LastMousePos[0] = (int)xpos;
      g_UIState.LastMousePos[1] = (int)ypos;
    }
  }
}

void OnMouseScroll(GLFWwindow *window, double xoffset, double yoffset) {
  auto pRenderContext = reinterpret_cast<VulkanRenderContext *>(glfwGetWindowUserPointer(window));
  pRenderContext->FrameZoomed((int)xoffset, (int)yoffset, nullptr);
}