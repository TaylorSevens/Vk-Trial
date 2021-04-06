#pragma once
#include <string>
#include "VkUtilities.h"
#include "GameTimer.hpp"

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

class VulkanRenderContext {
public:
  VulkanRenderContext();
  virtual ~VulkanRenderContext();

  VKHRESULT CreateVkInstance(const char *pAppName);
  VKHRESULT CreateWindowSurface(void *pOpacHandle);

  virtual VKHRESULT Initialize();

  VKHRESULT Destroy();

  virtual VKHRESULT Resize(int cx, int cy);
  virtual VKHRESULT FrameMoved(int xdelta, int ydelta, void *userData) = 0;
  virtual VKHRESULT FrameZoomed(int xdelta, int ydelta, void *userData) = 0;

  virtual void Update(float fTime, float fElapsedTime);
  virtual void RenderFrame(float fTime, float fElapsedTime);

  VKHRESULT SetMsaaEnabled(bool bEnabled);
  bool IsMsaaEnabled() const;

protected:
  /// Pick a properiate device
  virtual bool IsDeviceSuitable(VkPhysicalDevice device);

  /// Clean up render context.
  virtual void Cleanup();

  VKHRESULT InitVulkan();

  VKHRESULT CheckValidationLayerSupport(const char **ppValidationLayers, int nValidationLayerCount);
  VKHRESULT SetDebugCallback();
  VKHRESULT PickPhysicalDevice();
  VKHRESULT CreateLogicalDevice();
  uint32_t CalcSwapChainBackBufferCount();
  VKHRESULT CreateGraphicsQueueCommandPool();

  VKHRESULT CreateSwapChain();
  VKHRESULT CreateSwapChainImageViews();
  VKHRESULT CreateSwapChainFBsCompatibleRenderPass();
  VKHRESULT CreateSwapChainFrameBuffers();
  VKHRESULT CreateGraphicsQueueCommandBuffers();
  VKHRESULT CreateSwapChainSyncObjects();
  VKHRESULT RecreateSwapChain();
  void CleanupSwapChain();
  VKHRESULT CreateDepthStencilBuffer();

  bool CheckMultisampleSupport(VkPhysicalDevice, uint32_t *puMaxMsaaQualityLevel);

  VKHRESULT CreateMsaaColorBuffer();

  VKHRESULT PrepareNextFrame(_Inout_opt_ SwapChainItemContext **ppSwapchainContext);
  VKHRESULT WaitForPreviousGraphicsCommandBufferFence(_In_ RendererItemContext *pRendererContext);
  VKHRESULT Present();

  float GetAspectRatio() const;

  void CalcFrameStats();

  uint32_t FindMemoryTypeIndex(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

  struct DeviceFeatureConfig {
    bool MsaaEnabled;      /// Enable MSAA.
    uint32_t MsaaSampleCount;
    uint32_t MsaaQaulityLevel; /// MSAA Quality level.
    bool VsyncEnabled;     /// Enable Vsynchronization
    bool RaytracingEnabled; /// Enable Ray Tracing
  };

  uint32_t m_iClientWidth;
  uint32_t m_iClientHeight;

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
  VkQueue m_pPresentQueue; /// Used for presentation.

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
  VMAHandle m_pDepthStencilBufferMem;
  VkImageView m_pDepthStencilImageView;

  /// Surface.
  VkSurfaceKHR m_pWndSurface;
  /// Swap chain.
  VkSwapchainKHR m_pSwapChain;
  uint32_t m_iSwapChainImageCount;
  SwapChainItemContext m_aSwapChainItemCtx[2];
  VkFormat m_aSwapChainImageFormat;
  VkFormat m_aDepthStencilFormat;
  VkExtent2D m_aSwapChainExtent;
  VkRenderPass m_pSwapChainFBsCompatibleRenderPass;
};
