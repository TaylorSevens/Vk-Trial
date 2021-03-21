#include "VulkanApp.h"
#include "VkUtilities.h"
#include <vector>
#include <algorithm>
#include <WindowsX.h>
#include <vulkan/vk_sdk_platform.h>
#include <vulkan/vulkan_win32.h>
#include <sstream>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessageCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
    void*                                            pUserData
);

static VulkanApp *s_pVkApp;

static const char * s_aValidationLayerNames[] = {
  "VK_LAYER_LUNARG_standard_validation"
};

static const char *const s_aDeviceExtensions[] = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

VulkanApp::VulkanApp(HINSTANCE hInstance)
  : m_hAppInst(hInstance)
  , m_hMainWnd(nullptr)
  , m_iClientWidth(800)
  , m_iClientHeight(600)
  , m_MainWndCaption(L"Vulkan Application")
  , m_uWndSizeState(SIZE_RESTORED)
  , m_bAppPaused(false)
  , m_bFullScreen(false)
  , m_pVkInstance(VK_NULL_HANDLE)
  , m_pDebugMessenger(VK_NULL_HANDLE)
  , m_pPhysicalDevice(VK_NULL_HANDLE)
  , m_pDevice(VK_NULL_HANDLE)
  , m_iGraphicQueueFamilyIndex(-1)
  , m_iPresentQueueFamilyIndex(-1)
  , m_pGraphicQueue(VK_NULL_HANDLE)
  , m_pPresentQueue(VK_NULL_HANDLE)
  , m_pCommandPool(VK_NULL_HANDLE)
  , m_iCurrRendererItem(0)
  , m_iCurrSwapChainItem(0)
  , m_pWin32Surface(VK_NULL_HANDLE)
  , m_iSwapChainImageCount(0)
  , m_pSwapChainFBsCompatibleRenderPass(VK_NULL_HANDLE)
  , m_pMsaaColorBuffer(VK_NULL_HANDLE)
  , m_pMsaaColorBufferMem(VK_NULL_HANDLE)
  , m_pMsaaColorView(VK_NULL_HANDLE)
  , m_pDepthStencilImage(VK_NULL_HANDLE)
  , m_pDepthStencilBufferMem(VK_NULL_HANDLE)
  , m_pDepthStencilImageView(VK_NULL_HANDLE)
{
  m_aDeviceConfig.VsyncEnabled = (FALSE);
  m_aDeviceConfig.MsaaQaulityLevel = (1);
  m_aDeviceConfig.MsaaEnabled = FALSE;

  memset(m_aRendererItemCtx, 0, sizeof(m_aRendererItemCtx));
  memset(m_aSwapChainItemCtx, 0, sizeof(m_aSwapChainItemCtx));

  s_pVkApp = this;
}

VulkanApp::~VulkanApp()
{
}

VKHRESULT VulkanApp::Initialize() {
	VKHRESULT hr;

	V_RETURN(InitWindow());

	V_RETURN(InitVulkan());

  PostMessage(m_hMainWnd, WM_SIZE, (WPARAM)SIZE_RESTORED, MAKEWPARAM(m_iClientWidth, m_iClientHeight));

  /// You are responsible to  end the command buffer
  /// and flush the command queue at some later point.

	return hr;
}

VKHRESULT VulkanApp::Run() {

  MSG msg = { 0 };
  float fTime, fElapsed;

  m_GameTimer.Reset();

  while (msg.message != WM_QUIT) {
    // If there are Window messsages, then process them
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      m_GameTimer.Tick();
      fElapsed = m_GameTimer.TotalTime();
      fTime = m_GameTimer.DeltaTime();

      if (!m_bAppPaused) {
        CalcFrameStats();
        Update(fTime, fElapsed);
        RenderFrame(fTime, fElapsed);
      }
    }
  }

  vkDeviceWaitIdle(m_pDevice);

  Cleanup();

  return msg.message == WM_QUIT ? S_OK : -1;
}

VKHRESULT VulkanApp::InitWindow() {
  WNDCLASSEX wcx = { sizeof(WNDCLASSEX) };
  RECT rect;
  int width, height;
  VKHRESULT hr;
  HICON hIcon = NULL;
  WCHAR szResourcePath[MAX_PATH];

  if(FindDemoMediaFileAbsPath(L"Media/Icons/vulkan.ico", _countof(szResourcePath), szResourcePath) == 0) {
    hIcon = (HICON)LoadImage(m_hAppInst, szResourcePath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
  }

  wcx.style = CS_HREDRAW | CS_VREDRAW;
  wcx.lpfnWndProc = VulkanApp::MainWndProc;
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
    V_RETURN(-1);
  }

  rect = { 0, 0, (LONG)m_iClientWidth, (LONG)m_iClientHeight };
  AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);
  width = rect.right - rect.left;
  height = rect.bottom - rect.top;

  m_hMainWnd = CreateWindow(wcx.lpszClassName, m_MainWndCaption.c_str(),
    WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height,
    NULL, NULL, wcx.hInstance, 0);
  if (!m_hMainWnd) {
    V_RETURN(-1);
  }

  ShowWindow(m_hMainWnd, SW_SHOW);
  UpdateWindow(m_hMainWnd);

  return VK_SUCCESS;
}

VKHRESULT VulkanApp::InitVulkan() {

	VKHRESULT hr;

	V_RETURN(CreateVkInstance());

//#ifdef _DEBUG
//  V_RETURN(SetDebugCallback());
//#endif

  V_RETURN(CreateWin32Surfaces());

	V_RETURN(PickPhysicalDevice());

  V_RETURN(CreateLogicalDevice());

  V_RETURN(InitializeVmaAllocator(
    m_pVkInstance,
    m_pPhysicalDevice,
    m_pDevice
  ));

  V_RETURN(CreateGraphicsQueueCommandPool());

  V_RETURN(CreateGraphicsQueueCommandBuffers());

  m_iSwapChainImageCount = CalcSwapChainBackBufferCount();

  V_RETURN(CreateSwapChain());

  V_RETURN(CreateSwapChainSyncObjects());

	return hr;
}

VKHRESULT VulkanApp::CreateVkInstance() {
	VKHRESULT hr;
  size_t mbslen;
  std::string strCaption;

  wcstombs_s(&mbslen, nullptr, 0, m_MainWndCaption.c_str(), 0);
  strCaption.resize(mbslen);
  wcstombs_s(nullptr, &strCaption[0], mbslen, m_MainWndCaption.c_str(), mbslen-1);

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = strCaption.c_str();
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
  debugLayerInfo.messageSeverity =
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  debugLayerInfo.messageType =
    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
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

VKHRESULT VulkanApp::CheckValidationLayerSupport(const char **ppValidationLayers, int nValidationLayerCount) {
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

void VulkanApp::Cleanup() {

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


  vkDestroySurfaceKHR = (PFN_vkDestroySurfaceKHR)vkGetInstanceProcAddr(m_pVkInstance, "vkDestroySurfaceKHR");
  if (vkDestroySurfaceKHR) {
    vkDestroySurfaceKHR(m_pVkInstance, m_pWin32Surface, nullptr);
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

VKHRESULT VulkanApp::SetDebugCallback() {
    VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
    VKHRESULT hr;

    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = DebugMessageCallback;
    createInfo.pUserData = nullptr;

    V_RETURN(CreateDebugUtilsMessengerEXT(m_pVkInstance, &createInfo, nullptr, &m_pDebugMessenger));
    return hr;
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessageCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
  VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
  const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
  void*                                            pUserData
) {
  if (messageTypes >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    VK_TRACE("Severe level: error;\n-->%s\n", pCallbackData->pMessage);
  } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    VK_TRACE("Severe level: info;\n-->%s\n", pCallbackData->pMessage);
  } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    VK_TRACE("Severe level: warning;\n-->%s\n", pCallbackData->pMessage);
  }
  return VK_FALSE;
}

VKHRESULT VulkanApp::PickPhysicalDevice() {

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
      m_aDeviceConfig.MsaaQaulityLevel = std::min((UINT)uMaxMsaaQuality, m_aDeviceConfig.MsaaQaulityLevel);

			break;
		}
	}
	if (m_pPhysicalDevice == VK_NULL_HANDLE) {
		V_RETURN(-1);
	}

	return hr;
}

bool VulkanApp::IsDeviceSuitable(VkPhysicalDevice device) {
  HRESULT hr;
  VkPhysicalDeviceRayTracingPropertiesNV rtxProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV };
  VkPhysicalDeviceProperties2 deviceProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &rtxProperties };
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
      V(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_pWin32Surface, &presentSupport));
      if (presentSupport) {
        m_iPresentQueueFamilyIndex = i;
      }
    }
    ++i;
  }


	return deviceProperties.properties.deviceType ==
		VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
		deviceFeatures.geometryShader && 
    (m_iGraphicQueueFamilyIndex >= 0) && 
    (m_iPresentQueueFamilyIndex >= 0);
}

bool VulkanApp::CheckMultisampleSupport(VkPhysicalDevice, uint32_t *puMaxMsaaQualityLevel) {

  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(m_pPhysicalDevice, &properties);

  VkSampleCountFlags counts = std::min(properties.limits.framebufferColorSampleCounts,
    properties.limits.framebufferDepthSampleCounts);

  if (puMaxMsaaQualityLevel) *puMaxMsaaQualityLevel = (uint32_t)counts;

  return (uint32_t)counts > 1;
}

VKHRESULT VulkanApp::CreateLogicalDevice() {
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

VKHRESULT VulkanApp::CreateWin32Surfaces() {

  HRESULT hr;
  VkWin32SurfaceCreateInfoKHR createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  createInfo.hwnd = m_hMainWnd;
  createInfo.hinstance = m_hAppInst;
  PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR = nullptr;

  vkCreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(m_pVkInstance,
    "vkCreateWin32SurfaceKHR");

  if (vkCreateWin32SurfaceKHR) {
    V_RETURN(vkCreateWin32SurfaceKHR(m_pVkInstance, &createInfo, nullptr, &m_pWin32Surface));
  } else {
    V_RETURN(-1 || (VKHRESULT)(uintptr_t)(void *)("Can not find vkCreateWin32SurfaceKHR"));
  }

  return hr;
}

uint32_t VulkanApp::CalcSwapChainBackBufferCount() {
  VKHRESULT hr;
  VkSurfaceCapabilitiesKHR capabilities;
  uint32_t swapChainImageCount;

  V(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_pPhysicalDevice, m_pWin32Surface, &capabilities));

  swapChainImageCount = capabilities.minImageCount + 1;
  if (capabilities.maxImageCount > 0 && capabilities.maxImageCount < swapChainImageCount)
    swapChainImageCount = capabilities.maxImageCount;

  if (swapChainImageCount < m_iSwapChainImageCount)
    swapChainImageCount = m_iSwapChainImageCount;

  if (swapChainImageCount > _countof(m_aSwapChainItemCtx))
    swapChainImageCount = _countof(m_aSwapChainItemCtx);

  return swapChainImageCount;
}

VKHRESULT VulkanApp::CreateSwapChain() {
  VKHRESULT hr;
  /// Query for swap chain informatins.
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR>presentModes;
  VkSurfaceCapabilitiesKHR capabilities;
  uint32_t formatCount = 0,
          presentModeCount = 0;
  VkSurfaceFormatKHR surfaceFormat = { VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
  VkPresentModeKHR surfacePresentMode;
  VkExtent2D currExtent;
  VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
  uint32_t swapChainImageCount = m_iSwapChainImageCount;
  uint32_t i;
  VkImage aSwapChainImages[3];

  vkGetPhysicalDeviceSurfaceFormatsKHR(m_pPhysicalDevice, m_pWin32Surface, &formatCount, nullptr);
  formats.resize(formatCount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(m_pPhysicalDevice, m_pWin32Surface, &formatCount, formats.data());

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_pPhysicalDevice, m_pWin32Surface, &capabilities);

  vkGetPhysicalDeviceSurfacePresentModesKHR(m_pPhysicalDevice, m_pWin32Surface, &presentModeCount, nullptr);
  presentModes.resize(presentModeCount);
  vkGetPhysicalDeviceSurfacePresentModesKHR(m_pPhysicalDevice, m_pWin32Surface, &presentModeCount, presentModes.data());

  if (formats.empty() || presentModes.empty()) {
    V_RETURN(-1);
  }

  /// Choose the format.
  if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
    surfaceFormat = { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
  } else {
    surfaceFormat = formats.front();
    for (auto &format : formats) {
      if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace ==
        VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
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
    } if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
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
  swapChainCreateInfo.surface = m_pWin32Surface;
  swapChainCreateInfo.minImageCount = swapChainImageCount;
  swapChainCreateInfo.imageFormat = surfaceFormat.format;
  swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
  swapChainCreateInfo.imageExtent = currExtent;
  swapChainCreateInfo.imageArrayLayers = 1;
  swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  uint32_t queueFamilyIndices[] = {
    (uint32_t)m_iGraphicQueueFamilyIndex,
    (uint32_t)m_iPresentQueueFamilyIndex
  };

  if (queueFamilyIndices[0] != queueFamilyIndices[1]) {
    swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    swapChainCreateInfo.queueFamilyIndexCount = 2;
    swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapChainCreateInfo.queueFamilyIndexCount = 0; /// Optional
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
  V_RETURN(vkGetSwapchainImagesKHR(m_pDevice, m_pSwapChain, &swapChainImageCount, aSwapChainImages));
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

VKHRESULT VulkanApp::CreateMsaaColorBuffer() {

  VKHRESULT hr;
  VkImageCreateInfo createInfo = {
    VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // sType;
    nullptr, // pNext;
    0, // flags;
    VK_IMAGE_TYPE_2D, // imageType;
    m_aSwapChainImageFormat, // format;
    { m_aSwapChainExtent.width, m_aSwapChainExtent.height, 1 }, // extent;
    1, // mipLevels;
    1, // arrayLayers;
    (VkSampleCountFlagBits)m_aDeviceConfig.MsaaQaulityLevel, // samples;
    VK_IMAGE_TILING_OPTIMAL, // tiling;
    VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,// usage;
    VK_SHARING_MODE_EXCLUSIVE,// sharingMode;
    0,// queueFamilyIndexCount;
    nullptr,// pQueueFamilyIndices;
    VK_IMAGE_LAYOUT_UNDEFINED// initialLayout;
  };

  V_RETURN(CreateDefaultTexture(m_pDevice, &createInfo, &m_pMsaaColorBuffer, &m_pMsaaColorBufferMem));

  VkImageViewCreateInfo viewInfo = {
    VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // sType;
    nullptr, // pNext;
    0, // flags;
    m_pMsaaColorBuffer, // image;
    VK_IMAGE_VIEW_TYPE_2D, // viewType;
    createInfo.format, // format;
    {
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY
    },// components;
    {
      VK_IMAGE_ASPECT_COLOR_BIT, // aspectMask;
      0, // baseMipLevel;
      1, // levelCount;
      0, // baseArrayLayer;
      1, // layerCount;
    }// subresourceRange;
  };
  V_RETURN(vkCreateImageView(m_pDevice, &viewInfo, nullptr, &m_pMsaaColorView));

  return hr;
}

VKHRESULT VulkanApp::RecreateSwapChain() {
  VKHRESULT hr;
  CleanupSwapChain();
  V(CreateSwapChain());
  return hr;
}

void VulkanApp::CleanupSwapChain() {

  uint32_t i;

  for (i = 0; i < m_iSwapChainImageCount; ++i) {
    vkDestroyFramebuffer(m_pDevice, m_aSwapChainItemCtx[i].pFrameBuffer, nullptr);
    m_aSwapChainItemCtx[i].pFrameBuffer = nullptr;
  }

  DestroyVmaImage(m_pMsaaColorBuffer, m_pMsaaColorBufferMem);
  vkDestroyImageView(m_pDevice, m_pMsaaColorView, nullptr);

  vkDestroyRenderPass(m_pDevice, m_pSwapChainFBsCompatibleRenderPass, nullptr);
  m_pSwapChainFBsCompatibleRenderPass = nullptr;

  DestroyVmaImage(m_pDepthStencilImage, m_pDepthStencilBufferMem);
  vkDestroyImageView(m_pDevice, m_pDepthStencilImageView, nullptr); m_pDepthStencilImageView = nullptr;

  for (i = 0; i < m_iSwapChainImageCount; ++i) {
    vkDestroyImageView(m_pDevice, m_aSwapChainItemCtx[i].pImageView, nullptr);
    m_aSwapChainItemCtx[i].pImageView = nullptr;
  }

  vkDestroySwapchainKHR(m_pDevice, m_pSwapChain, nullptr);
  m_pSwapChain = nullptr;
}

VKHRESULT VulkanApp::CreateDepthStencilBuffer() {

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
      bFormatFound = TRUE;
      break;
    }
  }

  V_RETURN(!(bFormatFound && !!("Can not find the depth-stencil format that device supported!")));

  VkImageCreateInfo imageInfo = {
    VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    nullptr,
    0,
    VK_IMAGE_TYPE_2D,
    depthStencilFormat,
    { m_aSwapChainExtent.width, m_aSwapChainExtent.height, 1 },
    1,
    1,
    IsMsaaEnabled() ? ((VkSampleCountFlagBits)m_aDeviceConfig.MsaaQaulityLevel):VK_SAMPLE_COUNT_1_BIT,
    tiling,
    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    VK_SHARING_MODE_EXCLUSIVE,
    0,
    nullptr,
    VK_IMAGE_LAYOUT_UNDEFINED
  };

  V_RETURN(CreateDefaultTexture(m_pDevice, &imageInfo,
    &m_pDepthStencilImage, &m_pDepthStencilBufferMem));
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

  if (depthStencilFormat == VK_FORMAT_D24_UNORM_S8_UINT || depthStencilFormat == VK_FORMAT_D32_SFLOAT_S8_UINT)
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
  barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = m_pDepthStencilImage;
  barrier.subresourceRange = imageViewInfo.subresourceRange;

  vkCmdPipelineBarrier(
    pCmdBuffer,
    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
    0,
    0,
    nullptr,
    0,
    nullptr,
    1,
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

uint32_t VulkanApp::FindMemoryTypeIndex(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {

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

bool VulkanApp::IsMsaaEnabled() const {
  return (m_aDeviceConfig.MsaaEnabled && m_aDeviceConfig.MsaaQaulityLevel > 1);
}

VKHRESULT VulkanApp::CreateSwapChainImageViews() {

  HRESULT hr;
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

    V_RETURN(vkCreateImageView(m_pDevice, &createInfo, nullptr, &m_aSwapChainItemCtx[i].pImageView));
  }

  return hr;
}

VKHRESULT VulkanApp::CreateSwapChainFBsCompatibleRenderPass() {

  VKHRESULT hr;
  /// Create frame buffer compatible render pass
  /// 1 render pass, 1 subpass
  VkAttachmentDescription colorAttachment{};
  colorAttachment.format = m_aSwapChainImageFormat;
  colorAttachment.samples = IsMsaaEnabled() ? (VkSampleCountFlagBits)m_aDeviceConfig.MsaaQaulityLevel:VK_SAMPLE_COUNT_1_BIT;
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
  depthStencilAttachment.samples = IsMsaaEnabled() ? (VkSampleCountFlagBits)m_aDeviceConfig.MsaaQaulityLevel : VK_SAMPLE_COUNT_1_BIT;
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
  if(IsMsaaEnabled())
    subpass.pResolveAttachments = &colorAttachmentRefResolve;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkAttachmentDescription attachments[] = { colorAttachment, depthStencilAttachment, colorAttachmentResolve };

  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = IsMsaaEnabled() ? 3 : 2;
  renderPassInfo.pAttachments = attachments;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies = &dependency;

  V_RETURN(vkCreateRenderPass(m_pDevice, &renderPassInfo, nullptr, &m_pSwapChainFBsCompatibleRenderPass));
  return hr;
}

VKHRESULT VulkanApp::CreateSwapChainFrameBuffers() {

  HRESULT hr = 0;
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

    V_RETURN(vkCreateFramebuffer(m_pDevice, &fbInfo, nullptr, &m_aSwapChainItemCtx[i].pFrameBuffer));
  }

  return hr;
}

VKHRESULT VulkanApp::CreateGraphicsQueueCommandPool() {
  VKHRESULT hr;
  VkCommandPoolCreateInfo createInfo = {};

  createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  createInfo.queueFamilyIndex = m_iGraphicQueueFamilyIndex;
  createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  V_RETURN(vkCreateCommandPool(m_pDevice, &createInfo, nullptr, &m_pCommandPool));
  return hr;
}

VKHRESULT VulkanApp::CreateGraphicsQueueCommandBuffers() {

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
    V_RETURN(vkCreateSemaphore(m_pDevice, &semInfo, nullptr, &m_aRendererItemCtx[i].pRenderFinishedSem));
    V_RETURN(vkCreateFence(m_pDevice, &fenceInfo, nullptr, &m_aRendererItemCtx[i].pCmdBufferInFightFence));
  }

  return hr;
}

VKHRESULT VulkanApp::CreateSwapChainSyncObjects() {

  VKHRESULT hr = 0;
  VkSemaphoreCreateInfo semInfo = {};
  uint32_t i;

  semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semInfo.flags = 0;

  for (i = 0; i < m_iSwapChainImageCount; ++i) {
    V_RETURN(vkCreateSemaphore(m_pDevice, &semInfo, nullptr, &m_aSwapChainItemCtx[i].pImageAvailableSem));
  }

  return hr;
}

VKHRESULT VulkanApp::PrepareNextFrame(_Inout_opt_ SwapChainItemContext **ppSwapchainContext) {
  VKHRESULT hr;
  uint32_t imageIndex = 0;

  V(vkAcquireNextImageKHR(m_pDevice, m_pSwapChain, UINT64_MAX,
    m_aSwapChainItemCtx[m_iCurrSwapChainItem].pImageAvailableSem,
    VK_NULL_HANDLE, &imageIndex));
  if (m_iCurrSwapChainItem != imageIndex) {
    std::swap(m_aSwapChainItemCtx[imageIndex].pImageAvailableSem, m_aSwapChainItemCtx[m_iCurrSwapChainItem].pImageAvailableSem);
    m_iCurrSwapChainItem = imageIndex;
  }

  if (ppSwapchainContext)
    *ppSwapchainContext = (hr == VK_SUCCESS) ? &m_aSwapChainItemCtx[m_iCurrSwapChainItem] : nullptr;

  return hr;
}

VKHRESULT VulkanApp::WaitForPreviousGraphicsCommandBufferFence(_In_ RendererItemContext *pRendererContext) {

  HRESULT hr;

  V(!(pRendererContext && !!"Previous command buffer is not synchronized!"));
  V(vkWaitForFences(m_pDevice, 1,
    &pRendererContext->pCmdBufferInFightFence, FALSE, UINT64_MAX));
  vkResetFences(m_pDevice, 1, &pRendererContext->pCmdBufferInFightFence);

  return hr;
}

VKHRESULT VulkanApp::Present() {

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

float VulkanApp::GetAspectRatio() const {
  return (float)m_iClientWidth / m_iClientHeight;
}

VKHRESULT VulkanApp::Set4xMsaaEnabled(BOOL state) {
  VKHRESULT hr = 0;
  if (m_aDeviceConfig.MsaaEnabled != state) {
    m_aDeviceConfig.MsaaEnabled = state;

    V(vkDeviceWaitIdle(m_pDevice));
    V(RecreateSwapChain());

  }
  return hr;
}

void VulkanApp::Update(float /*fTime*/, float /*fElapsedTime*/) {

}

void VulkanApp::RenderFrame(float /*fTime*/, float /*fElaspedTime*/) {

}


LRESULT VulkanApp::OnResize() {

  VKHRESULT hr;

  V(vkDeviceWaitIdle(m_pDevice));
  V(RecreateSwapChain());

  return (LRESULT)hr;
}

LRESULT VulkanApp::OnMouseEvent(UINT uMsg, WPARAM wParam, int x, int y) {
  return S_OK;
}

LRESULT VulkanApp::OnKeyEvent(WPARAM wParam, LPARAM lParam) {
  return S_OK;
}

void VulkanApp::CalcFrameStats() {
  // Code computes the average frames per second, and also the 
  // average time it takes to render one frame.  These stats 
  // are appended to the window caption bar.

  static int frameCnt = 0;
  static float timeElapsed = 0.0f;
  float timeInterval;

  frameCnt++;

  // Compute averages over one second period.
  if ((timeInterval = m_GameTimer.TotalTime() - timeElapsed) >= 1.0f) {
    float fps = (float)frameCnt;
    float mfps = timeInterval * 1000.0f / fps;
    wchar_t buff[256];

    swprintf_s(buff, _countof(buff), L"%s  fps: %.0f mspf:%.3f", m_MainWndCaption.c_str(), fps, mfps);

    SetWindowText(m_hMainWnd, buff);

    // Reset for next average
    frameCnt = 0;
    timeElapsed = m_GameTimer.TotalTime();
  }
}

LRESULT VulkanApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {

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


LRESULT CALLBACK VulkanApp::MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  return s_pVkApp->MsgProc(hwnd, msg, wp, lp);
}