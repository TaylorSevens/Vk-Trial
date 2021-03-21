#pragma once
#include <string>
#include "VkUtilities.h"
#include "GameTimer.h"

struct SwapChainItemContext {
  VkImage pImage;
  VkImageView pImageView;
  VkFramebuffer pFrameBuffer;

  VkSemaphore pImageAvailableSem;
};

struct RendererItemContext {
  VkCommandBuffer pCommandBuffer;
  VkSemaphore pRenderFinishedSem;
  VkFence pCmdBufferInFightFence;

  void *pUserContext;
};

class VulkanApp
{
public:
	VulkanApp(HINSTANCE hInstance);
	virtual ~VulkanApp();

	virtual VKHRESULT Initialize();

  VKHRESULT Run();

  /// Clean up render context.
  virtual void Cleanup();

protected:
  virtual void Update(float fTime, float fElapsedTime);
  virtual void RenderFrame(float fTime, float fElapsedTime);

  static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT uMsg, WPARAM wp, LPARAM lp);
  LRESULT CALLBACK MsgProc(HWND hwnd, UINT uMsg, WPARAM wp, LPARAM lp);
  virtual LRESULT OnResize();
  virtual LRESULT OnMouseEvent(UINT uMsg, WPARAM wParam, int x, int y);
  virtual LRESULT OnKeyEvent(WPARAM wParam, LPARAM lParam);

	VKHRESULT InitWindow();
	VKHRESULT InitVulkan();

  VKHRESULT Set4xMsaaEnabled(BOOL bEnabled);

	VKHRESULT CreateVkInstance();
	VKHRESULT CheckValidationLayerSupport(const char **ppValidationLayers, int nValidationLayerCount);
  VKHRESULT SetDebugCallback();
	VKHRESULT PickPhysicalDevice();
  VKHRESULT CreateLogicalDevice();
  uint32_t CalcSwapChainBackBufferCount();
  VKHRESULT CreateGraphicsQueueCommandPool();
  VKHRESULT CreateWin32Surfaces();
  VKHRESULT CreateSwapChain();
  VKHRESULT CreateSwapChainImageViews();
  VKHRESULT CreateSwapChainFBsCompatibleRenderPass();
  VKHRESULT CreateSwapChainFrameBuffers();
  VKHRESULT CreateGraphicsQueueCommandBuffers();
  VKHRESULT CreateSwapChainSyncObjects();
  VKHRESULT RecreateSwapChain();
  void CleanupSwapChain();
  VKHRESULT CreateDepthStencilBuffer();

	bool IsDeviceSuitable(VkPhysicalDevice device);

  bool CheckMultisampleSupport(VkPhysicalDevice, uint32_t *puMaxMsaaQualityLevel);

  VKHRESULT CreateMsaaColorBuffer();

  VKHRESULT PrepareNextFrame(_Inout_opt_ SwapChainItemContext **ppSwapchainContext);
  VKHRESULT WaitForPreviousGraphicsCommandBufferFence(_In_ RendererItemContext *pRendererContext);
  VKHRESULT Present();

  float GetAspectRatio() const;

  void CalcFrameStats();

  uint32_t FindMemoryTypeIndex(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

  bool IsMsaaEnabled() const;

  struct DeviceFeatureConfig {
    BOOL MsaaEnabled;      /// Enable MSAA.
    UINT MsaaQaulityLevel; /// MSAA Quality level.
    BOOL VsyncEnabled; /// Enable Vsynchronization

    BOOL RaytracingEnabled; /// Enable Ray Tracing
  };

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

  DeviceFeatureConfig m_aDeviceConfig;

  /// Vulkan staffs.
  VkViewport m_Viewport;
  VkRect2D m_ScissorRect;

	VkInstance m_pVkInstance;
  VkDebugUtilsMessengerEXT m_pDebugMessenger;
	VkPhysicalDevice m_pPhysicalDevice;
  VkDevice m_pDevice;
  int m_iGraphicQueueFamilyIndex;
  int m_iPresentQueueFamilyIndex;
  VkQueue m_pGraphicQueue;
  VkQueue m_pPresentQueue; /// USed for presentation.

  VkCommandPool m_pCommandPool;

  /// Renderer command bufferss.
  RendererItemContext m_aRendererItemCtx[2];
  uint32_t m_iCurrRendererItem;

  uint32_t m_iCurrSwapChainItem;

  /// MSAA resolve texture.
  VkImage m_pMsaaColorBuffer;
  VMAHandle m_pMsaaColorBufferMem;
  VkImageView m_pMsaaColorView;

  /// Depth and stencil buffer.
  VkImage m_pDepthStencilImage;
  VMAHandle  m_pDepthStencilBufferMem;
  VkImageView m_pDepthStencilImageView;

  /// Surface.
  VkSurfaceKHR m_pWin32Surface;
  /// Swap chain.
  VkSwapchainKHR m_pSwapChain;
  uint32_t m_iSwapChainImageCount;
  SwapChainItemContext m_aSwapChainItemCtx[2];
  VkFormat m_aSwapChainImageFormat;
  VkFormat m_aDepthStencilFormat;
  VkExtent2D m_aSwapChainExtent;
  VkRenderPass m_pSwapChainFBsCompatibleRenderPass;

  /// Timer used for caculate FPS.
  GameTimer m_GameTimer;
};

