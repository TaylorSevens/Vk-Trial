#include "VulkanRenderContext.hpp"
#include "VkUtilities.h"
#include <vector>
#include <algorithm>
#ifdef _WIN32
#include <windows.h>
#include <vulkan/vk_sdk_platform.h>
#include <vulkan/vulkan_win32.h>
#endif
#include <sstream>

#undef min
#undef max

static VKAPI_ATTR VkBool32 VKAPI_CALL
DebugMessageCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                     VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                     const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData);

static const char *s_aValidationLayerNames[] = {"VK_LAYER_LUNARG_standard_validation"};

static const char *const s_aDeviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

VulkanRenderContext::VulkanRenderContext()
    : m_iClientWidth(800), m_iClientHeight(600), m_pVkInstance(VK_NULL_HANDLE),
      m_pDebugMessenger(VK_NULL_HANDLE), m_pPhysicalDevice(VK_NULL_HANDLE),
      m_pDevice(VK_NULL_HANDLE), m_iGraphicQueueFamilyIndex(-1), m_iPresentQueueFamilyIndex(-1),
      m_pGraphicQueue(VK_NULL_HANDLE), m_pPresentQueue(VK_NULL_HANDLE),
      m_pCommandPool(VK_NULL_HANDLE), m_iCurrRendererItem(0), m_iCurrSwapChainItem(0),
      m_pWndSurface(VK_NULL_HANDLE), m_iSwapChainImageCount(0),
      m_pSwapChainFBsCompatibleRenderPass(VK_NULL_HANDLE), m_pMsaaColorBuffer(VK_NULL_HANDLE),
      m_pMsaaColorBufferMem(VK_NULL_HANDLE), m_pMsaaColorView(VK_NULL_HANDLE),
      m_pDepthStencilImage(VK_NULL_HANDLE), m_pDepthStencilBufferMem(VK_NULL_HANDLE),
      m_pDepthStencilImageView(VK_NULL_HANDLE) {
  m_aDeviceConfig.VsyncEnabled = (FALSE);
  m_aDeviceConfig.MsaaSampleCount = 1;
  m_aDeviceConfig.MsaaQaulityLevel = 0;
  m_aDeviceConfig.MsaaEnabled = FALSE;

  memset(m_aRendererItemCtx, 0, sizeof(m_aRendererItemCtx));
  memset(m_aSwapChainItemCtx, 0, sizeof(m_aSwapChainItemCtx));
}

VulkanRenderContext::~VulkanRenderContext() {}

VKHRESULT VulkanRenderContext::Initialize() {
  VKHRESULT hr;

  V_RETURN(InitVulkan());

  return hr;
}

VKHRESULT VulkanRenderContext::Destroy() {
  vkDeviceWaitIdle(m_pDevice);
  Cleanup();
  return 0;
}

VKHRESULT VulkanRenderContext::InitVulkan() {

  VKHRESULT hr;

  //#ifdef _DEBUG
  //  V_RETURN(SetDebugCallback());
  //#endif

  V_RETURN(PickPhysicalDevice());

  V_RETURN(CreateLogicalDevice());

  V_RETURN(InitializeVmaAllocator(m_pVkInstance, m_pPhysicalDevice, m_pDevice));

  V_RETURN(CreateGraphicsQueueCommandPool());

  V_RETURN(CreateGraphicsQueueCommandBuffers());

  m_iSwapChainImageCount = CalcSwapChainBackBufferCount();

  V_RETURN(CreateSwapChain());

  V_RETURN(CreateSwapChainSyncObjects());

  return hr;
}

VKHRESULT VulkanRenderContext::CreateVkInstance(const char *pAppName) {
  VKHRESULT hr;

  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = pAppName;
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "No Engine";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = GetVulkanApiVersion();

  VkInstanceCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;

  uint32_t extCount = 0;
  std::vector<VkExtensionProperties> extensions;
  std::vector<const char *> extensionNames;

  V_RETURN(vkEnumerateInstanceExtensionProperties(NULL, &extCount, NULL));
  extensions.resize(extCount);
  V_RETURN(vkEnumerateInstanceExtensionProperties(NULL, &extCount, extensions.data()));
  for (auto &ext : extensions) {
    extensionNames.push_back(ext.extensionName);
  }

#ifdef _DEBUG
  extensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

  createInfo.enabledExtensionCount = (uint32_t)extensionNames.size();
  createInfo.ppEnabledExtensionNames = extensionNames.data();
#else
  createInfo.enabledExtensionCount = (uint32_t)extensionNames.size();
  createInfo.ppEnabledExtensionNames = extensionNames.data();
#endif

#ifdef _DEBUG
  hr = CheckValidationLayerSupport(s_aValidationLayerNames, _countof(s_aValidationLayerNames));
  if (VK_SUCCEEDED(hr)) {
    createInfo.enabledLayerCount = _countof(s_aValidationLayerNames);
    createInfo.ppEnabledLayerNames = s_aValidationLayerNames;
  } else {
    VK_TRACE("WARNING: Failed to find validation layer: %s\n", s_aValidationLayerNames[0]);
  }

  VkDebugUtilsMessengerCreateInfoEXT debugLayerInfo = {};

  debugLayerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  debugLayerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  debugLayerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  debugLayerInfo.pfnUserCallback = DebugMessageCallback;
  debugLayerInfo.pUserData = nullptr;

  createInfo.pNext = &debugLayerInfo;

#else
  createInfo.enabledLayerCount = 0;
#endif

  V_RETURN(vkCreateInstance(&createInfo, nullptr, &m_pVkInstance));

  return hr;
}

VKHRESULT VulkanRenderContext::CheckValidationLayerSupport(const char **ppValidationLayers,
                                                           int nValidationLayerCount) {
  uint32_t uLayerCount = 0;
  std::vector<VkLayerProperties> aAvailableLayers;
  VKHRESULT hr;
  const char **pp;
  int i;
  bool bLayerFound;

  V(vkEnumerateInstanceLayerProperties(&uLayerCount, nullptr));
  aAvailableLayers.resize(uLayerCount);
  V(vkEnumerateInstanceLayerProperties(&uLayerCount, aAvailableLayers.data()));

  pp = ppValidationLayers;
  for (i = 0; i < nValidationLayerCount; ++i, ++pp) {
    bLayerFound = false;
    for (auto aLayerDesc : aAvailableLayers) {
      if (strcmp(*pp, aLayerDesc.layerName) == 0) {
        bLayerFound = true;
        break;
      }
    }
    if (!bLayerFound)
      return -1;
  }

  return 0;
}

void VulkanRenderContext::Cleanup() {

  HRESULT hr;
  PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR = nullptr;
  uint32_t i;

  for (i = 0; i < m_iSwapChainImageCount; ++i) {
    vkDestroySemaphore(m_pDevice, m_aSwapChainItemCtx[i].pImageAvailableSem, nullptr);
  }

  CleanupSwapChain();

  for (i = 0; i < _countof(m_aRendererItemCtx); ++i) {
    vkDestroySemaphore(m_pDevice, m_aRendererItemCtx[i].pRenderFinishedSem, nullptr);
    vkDestroyFence(m_pDevice, m_aRendererItemCtx[i].pCmdBufferInFightFence, nullptr);
  }

  vkDestroyCommandPool(m_pDevice, m_pCommandPool, nullptr);

  vkDestroySurfaceKHR =
      (PFN_vkDestroySurfaceKHR)vkGetInstanceProcAddr(m_pVkInstance, "vkDestroySurfaceKHR");
  if (vkDestroySurfaceKHR) {
    vkDestroySurfaceKHR(m_pVkInstance, m_pWndSurface, nullptr);
  } else {
    V(-1);
  }

  //#ifdef _DEBUG
  //  DestroyDebugUtilsMessengerEXT(m_pVkInstance, m_pDebugMessenger, nullptr);
  //#endif

  DestroyVmaAllocator();

  vkDestroyDevice(m_pDevice, nullptr);

  vkDestroyInstance(m_pVkInstance, nullptr);
}

VKHRESULT VulkanRenderContext::SetDebugCallback() {
  VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
  VKHRESULT hr;

  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  createInfo.pfnUserCallback = DebugMessageCallback;
  createInfo.pUserData = nullptr;

  V_RETURN(CreateDebugUtilsMessengerEXT(m_pVkInstance, &createInfo, nullptr, &m_pDebugMessenger));
  return hr;
}

VKAPI_ATTR VkBool32 VKAPI_CALL
DebugMessageCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                     VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                     const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData) {
  if (messageTypes >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    VK_TRACE("Severe level: error;\n-->%s\n", pCallbackData->pMessage);
  } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    VK_TRACE("Severe level: info;\n-->%s\n", pCallbackData->pMessage);
  } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    VK_TRACE("Severe level: warning;\n-->%s\n", pCallbackData->pMessage);
  }
  return VK_FALSE;
}

VKHRESULT VulkanRenderContext::PickPhysicalDevice() {

  VKHRESULT hr;
  uint32_t deviceCount = 0;
  uint32_t uMaxMsaaQuality;

  V_RETURN(vkEnumeratePhysicalDevices(m_pVkInstance, &deviceCount, nullptr));
  if (deviceCount == 0) {
    V_RETURN(-1);
  }

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(m_pVkInstance, &deviceCount, devices.data());

  for (auto &device : devices) {
    if (IsDeviceSuitable(device)) {
      m_pPhysicalDevice = device;

      CheckMultisampleSupport(device, &uMaxMsaaQuality);
      m_aDeviceConfig.MsaaSampleCount = uMaxMsaaQuality;
      m_aDeviceConfig.MsaaQaulityLevel =
          std::min(uMaxMsaaQuality, m_aDeviceConfig.MsaaQaulityLevel);

      break;
    }
  }
  if (m_pPhysicalDevice == VK_NULL_HANDLE) {
    V_RETURN(-1);
  }

  return hr;
}

bool VulkanRenderContext::IsDeviceSuitable(VkPhysicalDevice device) {
  HRESULT hr;
  // VkPhysicalDeviceRayTracingPropertiesNV rtxProperties = {
  //     VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV};
  // VkPhysicalDeviceProperties2 deviceProperties = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
  //                                                 &rtxProperties};
  VkPhysicalDeviceProperties2 deviceProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, nullptr };
  VkPhysicalDeviceFeatures deviceFeatures;
  std::vector<VkQueueFamilyProperties> queueFamilyProperties;
  uint32_t queueFamilyCount = 0;
  int i;
  VkBool32 presentSupport = false;
  std::vector<VkExtensionProperties> extensions;
  uint32_t extensionCount = 0;
  bool reqExtExist = false;

  deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  vkGetPhysicalDeviceProperties2(device, &deviceProperties);
  vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

  /// Check queue capabilites.
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, NULL);
  queueFamilyProperties.resize(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyProperties.data());

  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
  extensions.resize(extensionCount);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

  for (auto &reqExt : s_aDeviceExtensions) {
    reqExtExist = false;
    for (auto &extension : extensions) {
      if (_stricmp(extension.extensionName, reqExt) == 0) {
        reqExtExist = true;
        break;
      }
    }

    if (!reqExtExist) {
      return false;
    }
  }

  i = 0;
  for (auto &queueProperty : queueFamilyProperties) {
    if (m_iGraphicQueueFamilyIndex < 0) {
      if (queueProperty.queueCount > 0 && (queueProperty.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
        m_iGraphicQueueFamilyIndex = i;
      }
    }
    if (m_iPresentQueueFamilyIndex < 0) {
      /// Check surface capacity.
      V(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_pWndSurface, &presentSupport));
      if (presentSupport) {
        m_iPresentQueueFamilyIndex = i;
      }
    }
    ++i;
  }

  return deviceProperties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
         deviceFeatures.geometryShader && (m_iGraphicQueueFamilyIndex >= 0) &&
         (m_iPresentQueueFamilyIndex >= 0);
}

bool VulkanRenderContext::CheckMultisampleSupport(VkPhysicalDevice,
                                                  uint32_t *puMaxMsaaQualityLevel) {

  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(m_pPhysicalDevice, &properties);

  VkSampleCountFlags counts = std::min(properties.limits.framebufferColorSampleCounts,
                                       properties.limits.framebufferDepthSampleCounts);

  if (puMaxMsaaQualityLevel)
    *puMaxMsaaQualityLevel = (uint32_t)counts;

  return (uint32_t)counts > 1;
}

VKHRESULT VulkanRenderContext::CreateLogicalDevice() {
  VkDeviceQueueCreateInfo queueCreateInfo[2] = {};
  uint32_t queueFamilyCount = _countof(queueCreateInfo);
  VKHRESULT hr;
  float priority = 1.0f;
  VkPhysicalDeviceFeatures physicalDeviceFeatures = {};
  VkDeviceCreateInfo createInfo = {};

  if (m_iGraphicQueueFamilyIndex == m_iPresentQueueFamilyIndex)
    queueFamilyCount = 1;

  /// Graphic Pipeline Queue.
  queueCreateInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueCreateInfo[0].queueFamilyIndex = m_iGraphicQueueFamilyIndex;
  queueCreateInfo[0].queueCount = 1;
  queueCreateInfo[0].pQueuePriorities = &priority;

  if (queueFamilyCount == 2) {
    queueCreateInfo[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo[1].queueFamilyIndex = m_iPresentQueueFamilyIndex;
    queueCreateInfo[1].queueCount = 1;
    queueCreateInfo[1].pQueuePriorities = &priority;
  }

  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.pQueueCreateInfos = queueCreateInfo;
  createInfo.queueCreateInfoCount = queueFamilyCount;
  createInfo.pEnabledFeatures = &physicalDeviceFeatures;
  createInfo.ppEnabledExtensionNames = s_aDeviceExtensions;
  createInfo.enabledExtensionCount = _countof(s_aDeviceExtensions);

#ifdef _DEBUG
  createInfo.enabledLayerCount = _countof(s_aValidationLayerNames);
  createInfo.ppEnabledLayerNames = s_aValidationLayerNames;
#endif

  V_RETURN(vkCreateDevice(m_pPhysicalDevice, &createInfo, nullptr, &m_pDevice));

  vkGetDeviceQueue(m_pDevice, m_iGraphicQueueFamilyIndex, 0, &m_pGraphicQueue);
  vkGetDeviceQueue(m_pDevice, m_iPresentQueueFamilyIndex, 0, &m_pPresentQueue);

  return hr;
}

VKHRESULT VulkanRenderContext::CreateWindowSurface(void *pOpacHandle) {

  VKHRESULT hr;
#ifdef _WIN32
  VkWin32SurfaceCreateInfoKHR createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  createInfo.hwnd = (HWND)pOpacHandle;
  createInfo.hinstance = (HINSTANCE)GetModuleHandleW(NULL);
  PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR = nullptr;

  vkCreateWin32SurfaceKHR =
      (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(m_pVkInstance, "vkCreateWin32SurfaceKHR");

  if (vkCreateWin32SurfaceKHR) {
    V_RETURN(vkCreateWin32SurfaceKHR(m_pVkInstance, &createInfo, nullptr, &m_pWndSurface));
  } else {
    V_RETURN(-1 || (VKHRESULT)(uintptr_t)(void *)("Can not find vkCreateWin32SurfaceKHR"));
  }
#else
  static_assert(0, "Unsupported yet");
#endif

  return hr;
}

uint32_t VulkanRenderContext::CalcSwapChainBackBufferCount() {
  VKHRESULT hr;
  VkSurfaceCapabilitiesKHR capabilities;
  uint32_t swapChainImageCount;

  V(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_pPhysicalDevice, m_pWndSurface, &capabilities));

  swapChainImageCount = capabilities.minImageCount + 1;
  if (capabilities.maxImageCount > 0 && capabilities.maxImageCount < swapChainImageCount)
    swapChainImageCount = capabilities.maxImageCount;

  if (swapChainImageCount < m_iSwapChainImageCount)
    swapChainImageCount = m_iSwapChainImageCount;

  if (swapChainImageCount > _countof(m_aSwapChainItemCtx))
    swapChainImageCount = _countof(m_aSwapChainItemCtx);

  return swapChainImageCount;
}

VKHRESULT VulkanRenderContext::CreateSwapChain() {
  VKHRESULT hr;
  /// Query for swap chain informatins.
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
  VkSurfaceCapabilitiesKHR capabilities;
  uint32_t formatCount = 0, presentModeCount = 0;
  VkSurfaceFormatKHR surfaceFormat = {VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
  VkPresentModeKHR surfacePresentMode;
  VkExtent2D currExtent;
  VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
  uint32_t swapChainImageCount = m_iSwapChainImageCount;
  uint32_t i;
  VkImage aSwapChainImages[3];

  vkGetPhysicalDeviceSurfaceFormatsKHR(m_pPhysicalDevice, m_pWndSurface, &formatCount, nullptr);
  formats.resize(formatCount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(m_pPhysicalDevice, m_pWndSurface, &formatCount,
                                       formats.data());

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_pPhysicalDevice, m_pWndSurface, &capabilities);

  vkGetPhysicalDeviceSurfacePresentModesKHR(m_pPhysicalDevice, m_pWndSurface, &presentModeCount,
                                            nullptr);
  presentModes.resize(presentModeCount);
  vkGetPhysicalDeviceSurfacePresentModesKHR(m_pPhysicalDevice, m_pWndSurface, &presentModeCount,
                                            presentModes.data());

  if (formats.empty() || presentModes.empty()) {
    V_RETURN(-1);
  }

  /// Choose the format.
  if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
    surfaceFormat = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
  } else {
    surfaceFormat = formats.front();
    for (auto &format : formats) {
      if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
          format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        surfaceFormat = format;
        break;
      }
    }
  }

  /// Choose the present mode.
  surfacePresentMode = VK_PRESENT_MODE_FIFO_KHR;
  for (auto &presentMode : presentModes) {
    if (presentMode == VK_PRESENT_MODE_FIFO_KHR && m_aDeviceConfig.VsyncEnabled) {
      surfacePresentMode = presentMode;
      break;
    }
    if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
      surfacePresentMode = presentMode;
      break;
    } else if (presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
      surfacePresentMode = presentMode;
      break;
    }
  }

  /// Choose swap chain extent.
  if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
    currExtent = capabilities.currentExtent;
  } else {
    currExtent.width = m_iClientWidth;
    currExtent.height = m_iClientHeight;

    currExtent.width = std::max(capabilities.minImageExtent.width,
                                std::min(capabilities.maxImageExtent.width, currExtent.width));

    currExtent.height = std::max(capabilities.minImageExtent.height,
                                 std::max(capabilities.maxImageExtent.height, currExtent.height));
  }

  /// Create swap chain.
  swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapChainCreateInfo.surface = m_pWndSurface;
  swapChainCreateInfo.minImageCount = swapChainImageCount;
  swapChainCreateInfo.imageFormat = surfaceFormat.format;
  swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
  swapChainCreateInfo.imageExtent = currExtent;
  swapChainCreateInfo.imageArrayLayers = 1;
  swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  uint32_t queueFamilyIndices[] = {(uint32_t)m_iGraphicQueueFamilyIndex,
                                   (uint32_t)m_iPresentQueueFamilyIndex};

  if (queueFamilyIndices[0] != queueFamilyIndices[1]) {
    swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    swapChainCreateInfo.queueFamilyIndexCount = 2;
    swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapChainCreateInfo.queueFamilyIndexCount = 0;     /// Optional
    swapChainCreateInfo.pQueueFamilyIndices = nullptr; /// Optional
  }

  swapChainCreateInfo.preTransform = capabilities.currentTransform;
  swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapChainCreateInfo.presentMode = surfacePresentMode;
  swapChainCreateInfo.clipped = VK_TRUE;
  swapChainCreateInfo.oldSwapchain = nullptr;

  V_RETURN(vkCreateSwapchainKHR(m_pDevice, &swapChainCreateInfo, nullptr, &m_pSwapChain));

  /// Store some immmediate context for later usage.
  m_aSwapChainImageFormat = surfaceFormat.format;
  m_aSwapChainExtent = currExtent;

  /// Query images form the swap chain.
  swapChainImageCount = 0;
  V_RETURN(vkGetSwapchainImagesKHR(m_pDevice, m_pSwapChain, &swapChainImageCount, nullptr));
  V(!(m_iSwapChainImageCount == swapChainImageCount) && !!("Swap chain image count error!"));
  V_RETURN(
      vkGetSwapchainImagesKHR(m_pDevice, m_pSwapChain, &swapChainImageCount, aSwapChainImages));
  for (i = 0; i < swapChainImageCount; ++i)
    m_aSwapChainItemCtx[i].pImage = aSwapChainImages[i];

  if (IsMsaaEnabled()) {
    V_RETURN(CreateMsaaColorBuffer());
  }

  V_RETURN(this->CreateSwapChainImageViews());
  V_RETURN(this->CreateDepthStencilBuffer());
  V_RETURN(this->CreateSwapChainFBsCompatibleRenderPass());
  V_RETURN(this->CreateSwapChainFrameBuffers());

  /// Viewport and Scissor Rect Settings.
  m_Viewport.x = .0f;
  m_Viewport.y = .0f;
  m_Viewport.width = (float)m_iClientWidth;
  m_Viewport.height = (float)m_iClientHeight;
  m_Viewport.minDepth = .0f;
  m_Viewport.maxDepth = 1.0f;

  m_ScissorRect.offset.x = 0;
  m_ScissorRect.offset.y = 0;
  m_ScissorRect.extent = m_aSwapChainExtent;

  return hr;
}

VKHRESULT VulkanRenderContext::CreateMsaaColorBuffer() {

  VKHRESULT hr;
  VkImageCreateInfo createInfo = {
      VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                                           // sType;
      nullptr,                                                                       // pNext;
      0,                                                                             // flags;
      VK_IMAGE_TYPE_2D,                                                              // imageType;
      m_aSwapChainImageFormat,                                                       // format;
      {m_aSwapChainExtent.width, m_aSwapChainExtent.height, 1},                      // extent;
      1,                                                                             // mipLevels;
      1,                                                                             // arrayLayers;
      (VkSampleCountFlagBits)m_aDeviceConfig.MsaaQaulityLevel,                       // samples;
      VK_IMAGE_TILING_OPTIMAL,                                                       // tiling;
      VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // usage;
      VK_SHARING_MODE_EXCLUSIVE,                                                     // sharingMode;
      0,                        // queueFamilyIndexCount;
      nullptr,                  // pQueueFamilyIndices;
      VK_IMAGE_LAYOUT_UNDEFINED // initialLayout;
  };

  V_RETURN(
      CreateDefaultTexture(m_pDevice, &createInfo, &m_pMsaaColorBuffer, &m_pMsaaColorBufferMem));

  VkImageViewCreateInfo viewInfo = {
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // sType;
      nullptr,                                  // pNext;
      0,                                        // flags;
      m_pMsaaColorBuffer,                       // image;
      VK_IMAGE_VIEW_TYPE_2D,                    // viewType;
      createInfo.format,                        // format;
      {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
       VK_COMPONENT_SWIZZLE_IDENTITY}, // components;
      {
          VK_IMAGE_ASPECT_COLOR_BIT, // aspectMask;
          0,                         // baseMipLevel;
          1,                         // levelCount;
          0,                         // baseArrayLayer;
          1,                         // layerCount;
      }                              // subresourceRange;
  };
  V_RETURN(vkCreateImageView(m_pDevice, &viewInfo, nullptr, &m_pMsaaColorView));

  return hr;
}

VKHRESULT VulkanRenderContext::RecreateSwapChain() {
  VKHRESULT hr;
  CleanupSwapChain();
  V(CreateSwapChain());
  return hr;
}

void VulkanRenderContext::CleanupSwapChain() {

  uint32_t i;

  for (i = 0; i < m_iSwapChainImageCount; ++i) {
    vkDestroyFramebuffer(m_pDevice, m_aSwapChainItemCtx[i].pFrameBuffer, nullptr);
    m_aSwapChainItemCtx[i].pFrameBuffer = nullptr;
  }

  vkDestroyImageView(m_pDevice, m_pMsaaColorView, nullptr);
  m_pMsaaColorView = nullptr;
  DestroyVmaImage(m_pMsaaColorBuffer, m_pMsaaColorBufferMem);
  m_pMsaaColorBuffer = nullptr;
  m_pMsaaColorBufferMem = nullptr;

  vkDestroyRenderPass(m_pDevice, m_pSwapChainFBsCompatibleRenderPass, nullptr);
  m_pSwapChainFBsCompatibleRenderPass = nullptr;

  DestroyVmaImage(m_pDepthStencilImage, m_pDepthStencilBufferMem);
  m_pDepthStencilImage = nullptr;
  m_pDepthStencilBufferMem = nullptr;
  vkDestroyImageView(m_pDevice, m_pDepthStencilImageView, nullptr);
  m_pDepthStencilImageView = nullptr;

  for (i = 0; i < m_iSwapChainImageCount; ++i) {
    vkDestroyImageView(m_pDevice, m_aSwapChainItemCtx[i].pImageView, nullptr);
    m_aSwapChainItemCtx[i].pImageView = nullptr;
  }

  vkDestroySwapchainKHR(m_pDevice, m_pSwapChain, nullptr);
  m_pSwapChain = nullptr;
}

VKHRESULT VulkanRenderContext::CreateDepthStencilBuffer() {

  VKHRESULT hr;
  VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL; /// VK_IMAGE_TILING_LINEAR
  VkFormatFeatureFlags features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
  VkFormat candidates[] = {
      VK_FORMAT_D24_UNORM_S8_UINT,
      VK_FORMAT_D32_SFLOAT_S8_UINT,
      VK_FORMAT_D32_SFLOAT,
  };
  VkFormatProperties props;
  VkBool32 bFormatFound = VK_FALSE;
  VkFormat depthStencilFormat;

  for (auto &format : candidates) {
    vkGetPhysicalDeviceFormatProperties(m_pPhysicalDevice, format, &props);

    if ((props.optimalTilingFeatures & features) == features) {
      depthStencilFormat = format;
      bFormatFound = true;
      break;
    }
  }

  V_RETURN(!(bFormatFound && !!("Can not find the depth-stencil format that device supported!")));

  VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                 nullptr,
                                 0,
                                 VK_IMAGE_TYPE_2D,
                                 depthStencilFormat,
                                 {m_aSwapChainExtent.width, m_aSwapChainExtent.height, 1},
                                 1,
                                 1,
                                 IsMsaaEnabled()
                                     ? ((VkSampleCountFlagBits)m_aDeviceConfig.MsaaQaulityLevel)
                                     : VK_SAMPLE_COUNT_1_BIT,
                                 tiling,
                                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                 VK_SHARING_MODE_EXCLUSIVE,
                                 0,
                                 nullptr,
                                 VK_IMAGE_LAYOUT_UNDEFINED};

  V_RETURN(CreateDefaultTexture(m_pDevice, &imageInfo, &m_pDepthStencilImage,
                                &m_pDepthStencilBufferMem));
  m_aDepthStencilFormat = depthStencilFormat;

  /// Create Depth-Stencil Image View.
  VkImageViewCreateInfo imageViewInfo = {};
  imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  imageViewInfo.image = m_pDepthStencilImage;
  imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  imageViewInfo.format = depthStencilFormat;
  imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  imageViewInfo.subresourceRange.baseMipLevel = 0;
  imageViewInfo.subresourceRange.levelCount = 1;
  imageViewInfo.subresourceRange.baseArrayLayer = 0;
  imageViewInfo.subresourceRange.layerCount = 1;

  if (depthStencilFormat == VK_FORMAT_D24_UNORM_S8_UINT ||
      depthStencilFormat == VK_FORMAT_D32_SFLOAT_S8_UINT)
    imageViewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
  V_RETURN(vkCreateImageView(m_pDevice, &imageViewInfo, nullptr, &m_pDepthStencilImageView));

  /// Start record intialize command buffer.
  auto pCmdBuffer = m_aRendererItemCtx[m_iCurrRendererItem].pCommandBuffer;
  VkCommandBufferBeginInfo cmdBegInfo = {
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      nullptr,
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      nullptr,
  };

  V(vkBeginCommandBuffer(pCmdBuffer, &cmdBegInfo));

  /// Record a command to transfer layout state,
  VkImageMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.image = m_pDepthStencilImage;
  barrier.srcAccessMask = 0;
  barrier.dstAccessMask =
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = m_pDepthStencilImage;
  barrier.subresourceRange = imageViewInfo.subresourceRange;

  vkCmdPipelineBarrier(pCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, nullptr, 0, nullptr, 1,
                       &barrier);

  vkEndCommandBuffer(pCmdBuffer);

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &pCmdBuffer;
  vkQueueSubmit(m_pGraphicQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(m_pGraphicQueue);

  return hr;
}

uint32_t VulkanRenderContext::FindMemoryTypeIndex(uint32_t typeFilter,
                                                  VkMemoryPropertyFlags properties) const {

  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(m_pPhysicalDevice, &memProperties);
  uint32_t typeIndex = (uint32_t)-1;
  uint32_t i;
  VKHRESULT hr;

  for (i = 0; i < memProperties.memoryTypeCount; ++i) {
    if ((typeFilter & (1 << i)) &&
        (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
      typeIndex = i;
      break;
    }
  }

  V(!(typeIndex >= 0 && !!("Can not find memory type index!")));
  return typeIndex;
}

VKHRESULT VulkanRenderContext::CreateSwapChainImageViews() {

  VKHRESULT hr;
  VkImageViewCreateInfo createInfo = {};
  uint32_t i;

  createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  createInfo.format = m_aSwapChainImageFormat;
  createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
  createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  createInfo.subresourceRange.baseMipLevel = 0;
  createInfo.subresourceRange.levelCount = 1;
  createInfo.subresourceRange.baseArrayLayer = 0;
  createInfo.subresourceRange.layerCount = 1;

  hr = VK_SUCCESS;
  for (i = 0; i < m_iSwapChainImageCount; ++i) {
    createInfo.image = m_aSwapChainItemCtx[i].pImage;

    V_RETURN(
        vkCreateImageView(m_pDevice, &createInfo, nullptr, &m_aSwapChainItemCtx[i].pImageView));
  }

  return hr;
}

VKHRESULT VulkanRenderContext::CreateSwapChainFBsCompatibleRenderPass() {

  VKHRESULT hr;
  /// Create frame buffer compatible render pass
  /// 1 render pass, 1 subpass
  VkAttachmentDescription colorAttachment{};
  colorAttachment.format = m_aSwapChainImageFormat;
  colorAttachment.samples = IsMsaaEnabled()
                                ? (VkSampleCountFlagBits)m_aDeviceConfig.MsaaQaulityLevel
                                : VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  if (IsMsaaEnabled()) {
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  } else
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentDescription colorAttachmentResolve = {};
  colorAttachmentResolve.format = m_aSwapChainImageFormat;
  colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentDescription depthStencilAttachment = {};
  depthStencilAttachment.format = m_aDepthStencilFormat;
  depthStencilAttachment.samples = IsMsaaEnabled()
                                       ? (VkSampleCountFlagBits)m_aDeviceConfig.MsaaQaulityLevel
                                       : VK_SAMPLE_COUNT_1_BIT;
  depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depthStencilAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthStencilAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
  depthStencilAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthStencilAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference colorAttachmentRef{};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference colorAttachmentRefResolve;
  colorAttachmentRefResolve.attachment = 2;
  colorAttachmentRefResolve.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthStencilAttachmentRef = {};
  depthStencilAttachmentRef.attachment = 1;
  depthStencilAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;
  subpass.pDepthStencilAttachment = &depthStencilAttachmentRef;
  if (IsMsaaEnabled())
    subpass.pResolveAttachments = &colorAttachmentRefResolve;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkAttachmentDescription attachments[] = {colorAttachment, depthStencilAttachment,
                                           colorAttachmentResolve};

  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = IsMsaaEnabled() ? 3 : 2;
  renderPassInfo.pAttachments = attachments;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies = &dependency;

  V_RETURN(vkCreateRenderPass(m_pDevice, &renderPassInfo, nullptr,
                              &m_pSwapChainFBsCompatibleRenderPass));
  return hr;
}

VKHRESULT VulkanRenderContext::CreateSwapChainFrameBuffers() {

  VKHRESULT hr = 0;
  /// Create frame buffers.
  uint32_t i;
  VkFramebufferCreateInfo fbInfo = {};
  VkImageView aAttachmentViews[3] = {};

  fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  fbInfo.renderPass = m_pSwapChainFBsCompatibleRenderPass;

  for (i = 0; i < m_iSwapChainImageCount; ++i) {

    if (IsMsaaEnabled()) {
      aAttachmentViews[0] = m_pMsaaColorView;
      aAttachmentViews[1] = m_pDepthStencilImageView;
      aAttachmentViews[2] = m_aSwapChainItemCtx[i].pImageView;
      fbInfo.attachmentCount = 3;
    } else {
      aAttachmentViews[0] = m_aSwapChainItemCtx[i].pImageView;
      aAttachmentViews[1] = m_pDepthStencilImageView;
      fbInfo.attachmentCount = 2;
    }

    fbInfo.pAttachments = aAttachmentViews;
    fbInfo.width = m_aSwapChainExtent.width;
    fbInfo.height = m_aSwapChainExtent.height;
    fbInfo.layers = 1;

    V_RETURN(
        vkCreateFramebuffer(m_pDevice, &fbInfo, nullptr, &m_aSwapChainItemCtx[i].pFrameBuffer));
  }

  return hr;
}

VKHRESULT VulkanRenderContext::CreateGraphicsQueueCommandPool() {
  VKHRESULT hr;
  VkCommandPoolCreateInfo createInfo = {};

  createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  createInfo.queueFamilyIndex = m_iGraphicQueueFamilyIndex;
  createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  V_RETURN(vkCreateCommandPool(m_pDevice, &createInfo, nullptr, &m_pCommandPool));
  return hr;
}

VKHRESULT VulkanRenderContext::CreateGraphicsQueueCommandBuffers() {

  VKHRESULT hr;
  VkCommandBufferAllocateInfo allocInfo = {};
  VkCommandBuffer aCmdBuffers[3];
  uint32_t i;
  VkSemaphoreCreateInfo semInfo = {};
  VkFenceCreateInfo fenceInfo = {};

  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = m_pCommandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = _countof(m_aRendererItemCtx);

  V_RETURN(vkAllocateCommandBuffers(m_pDevice, &allocInfo, aCmdBuffers));

  semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semInfo.flags = 0;

  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (i = 0; i < _countof(m_aRendererItemCtx); ++i) {
    m_aRendererItemCtx[i].pCommandBuffer = aCmdBuffers[i];

    /// Create sychronizing objects.
    V_RETURN(
        vkCreateSemaphore(m_pDevice, &semInfo, nullptr, &m_aRendererItemCtx[i].pRenderFinishedSem));
    V_RETURN(vkCreateFence(m_pDevice, &fenceInfo, nullptr,
                           &m_aRendererItemCtx[i].pCmdBufferInFightFence));
  }

  return hr;
}

VKHRESULT VulkanRenderContext::CreateSwapChainSyncObjects() {

  VKHRESULT hr = 0;
  VkSemaphoreCreateInfo semInfo = {};
  uint32_t i;

  semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semInfo.flags = 0;

  for (i = 0; i < m_iSwapChainImageCount; ++i) {
    V_RETURN(vkCreateSemaphore(m_pDevice, &semInfo, nullptr,
                               &m_aSwapChainItemCtx[i].pImageAvailableSem));
  }

  return hr;
}

VKHRESULT
VulkanRenderContext::PrepareNextFrame(_Inout_opt_ SwapChainItemContext **ppSwapchainContext) {
  VKHRESULT hr;
  uint32_t imageIndex = 0;

  V(vkAcquireNextImageKHR(m_pDevice, m_pSwapChain, UINT64_MAX,
                          m_aSwapChainItemCtx[m_iCurrSwapChainItem].pImageAvailableSem,
                          VK_NULL_HANDLE, &imageIndex));
  if (m_iCurrSwapChainItem != imageIndex) {
    std::swap(m_aSwapChainItemCtx[imageIndex].pImageAvailableSem,
              m_aSwapChainItemCtx[m_iCurrSwapChainItem].pImageAvailableSem);
    m_iCurrSwapChainItem = imageIndex;
  }

  if (ppSwapchainContext)
    *ppSwapchainContext = (hr == VK_SUCCESS) ? &m_aSwapChainItemCtx[m_iCurrSwapChainItem] : nullptr;

  return hr;
}

VKHRESULT
VulkanRenderContext::WaitForPreviousGraphicsCommandBufferFence(
    _In_ RendererItemContext *pRendererContext) {

  VKHRESULT hr;

  V(!(pRendererContext && !!"Previous command buffer is not synchronized!"));
  V(vkWaitForFences(m_pDevice, 1, &pRendererContext->pCmdBufferInFightFence, FALSE, UINT64_MAX));
  vkResetFences(m_pDevice, 1, &pRendererContext->pCmdBufferInFightFence);

  return hr;
}

VKHRESULT VulkanRenderContext::Present() {

  VKHRESULT hr;
  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &m_aRendererItemCtx[m_iCurrRendererItem].pRenderFinishedSem;
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &m_pSwapChain;
  presentInfo.pImageIndices = &m_iCurrSwapChainItem;
  presentInfo.pResults = nullptr;

  V(vkQueuePresentKHR(m_pPresentQueue, &presentInfo));

  m_iCurrSwapChainItem = (m_iCurrSwapChainItem + 1) % m_iSwapChainImageCount;
  m_iCurrRendererItem = (m_iCurrRendererItem + 1) % _countof(m_aRendererItemCtx);

  return hr;
}

float VulkanRenderContext::GetAspectRatio() const {
  return (float)m_iClientWidth / m_iClientHeight;
}

VKHRESULT VulkanRenderContext::SetMsaaEnabled(bool state) {
  VKHRESULT hr = 0;
  if (m_aDeviceConfig.MsaaEnabled != state) {
    m_aDeviceConfig.MsaaEnabled = state;

    V(vkDeviceWaitIdle(m_pDevice));
    V(RecreateSwapChain());
  }
  return hr;
}

bool VulkanRenderContext::IsMsaaEnabled() const {
  return (m_aDeviceConfig.MsaaEnabled && m_aDeviceConfig.MsaaQaulityLevel > 1);
}

void VulkanRenderContext::Update(float /*fTime*/, float /*fElapsedTime*/) {}

void VulkanRenderContext::RenderFrame(float /*fTime*/, float /*fElaspedTime*/) {}

VKHRESULT VulkanRenderContext::Resize(int cx, int cy) {

  VKHRESULT hr;

  m_iClientWidth = cx;
  m_iClientHeight = cy;

  V(vkDeviceWaitIdle(m_pDevice));
  V(RecreateSwapChain());

  return hr;
}
