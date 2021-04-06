#include "VkUtilities.h"
#include <string.h>
#include <algorithm>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#include <cstdarg>

#ifdef _WIN32
#include <windows.h>
#endif

#pragma warning(disable: 4098)

VmaAllocator g_pVmaAllocator;

struct VulkanResoureBindingConfig {
  uint32_t MinUniformBufferOffsetAlignment;
} g_aResourceBindingConfig;

#ifdef _WIN32
void vkUtilsTrace(const char* fmt, ...) {
  char buff[1024];

  va_list vlist;
  va_start(vlist, fmt);
  vsnprintf(buff, _countof(buff), fmt, vlist);
  va_end(vlist);

  OutputDebugStringA(buff);
}
#else
void vkUtilsTrace(const char *fmt, ...) {
    va_list vlist;
    va_start(vlist, fmt);
    vprintf(fmt, vlist);
}
#endif

#define IMPLEMENT_VK_EXT_FUNC(funcptr_type, func, instance, ...) \
    funcptr_type ext_func = (funcptr_type)vkGetInstanceProcAddr(instance, \
    "vk" #func); \
    if(ext_func) \
        return ext_func(instance, __VA_ARGS__); \
    else \
        return VK_ERROR_EXTENSION_NOT_PRESENT;

#define IMPLEMENT_VK_EXT_FUNC2(funcptr_type, func, instance, ...) \
    funcptr_type ext_func = (funcptr_type)vkGetInstanceProcAddr(instance, \
    "vk" #func); \
    if(ext_func) \
        return ext_func(instance, __VA_ARGS__); \
    else \
        return;

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pCallback
) {
    IMPLEMENT_VK_EXT_FUNC(PFN_vkCreateDebugUtilsMessengerEXT,
        CreateDebugUtilsMessengerEXT, instance, pCreateInfo, pAllocator, pCallback)
}

void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT callback,
    const VkAllocationCallbacks *pAllocator
) {
    IMPLEMENT_VK_EXT_FUNC2(PFN_vkDestroyDebugUtilsMessengerEXT,
        DestroyDebugUtilsMessengerEXT, instance, callback, pAllocator)
}

VkShaderModule CreateShaderModule(
  VkDevice pDevice,
  const uint32_t *pBytes,
  size_t bytesLength
) {
  VkShaderModuleCreateInfo createInfo = {0};
  VKHRESULT hr;
  VkShaderModule pShaderModule;

  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = bytesLength;
  createInfo.pCode = pBytes;

  V(vkCreateShaderModule(pDevice, &createInfo, NULL, &pShaderModule));

  return hr == VK_SUCCESS ? pShaderModule : VK_NULL_HANDLE;
}

VkShaderModule CreateShaderModuleFromSPIRVFile(
  VkDevice pDevice,
  const wchar_t *pFileName
) {
  VKHRESULT hr;
  FILE *fd;
  unsigned char *pBuffer;
  size_t nlen, idx, nleft, count;
  VkShaderModule shaderModule = VK_NULL_HANDLE;
  WCHAR szFilePath[MAX_PATH];

  if(FindDemoMediaFileAbsPath(pFileName, MAX_PATH, szFilePath))
    return VK_NULL_HANDLE;

  if (_wfopen_s(&fd, szFilePath, L"rb")) {
    V(-1 && (VKHRESULT)(uintptr_t)(void*)"Can not find the shader File");
    return VK_NULL_HANDLE;
  }

  fseek(fd, 0, SEEK_END);
  nlen = ftell(fd);
  pBuffer = new unsigned char[nlen];
  if (!pBuffer) {
    V((VKHRESULT)(uintptr_t)(void*)"Can not allocate memory");
    fclose(fd);
    return VK_NULL_HANDLE;
  }

  fseek(fd, 0, SEEK_SET);
  idx = 0;
  nleft = nlen;
  do {
    count = fread_s(pBuffer + idx, nleft, 1, nleft, fd);
    idx += count;
    nleft -= count;
  } while (!count);
  if(!nleft)
    shaderModule = CreateShaderModule(pDevice, (uint32_t *)pBuffer, nlen);
  else
    V((VKHRESULT)(uintptr_t)(void*)"Can not read file context");

  fclose(fd);
  delete[] pBuffer;

  return shaderModule;
}

// $(VK_SDK_PATH)\Bin\glslangValidator -V %(Filename)%(Extension) -o $(IntDir)%(Filename)%(Extension).spv
// Vulkan SPIR-V tools generating vender-independent binary.
// $(OutDir)%(Filename)%(Extension).spv

uint32_t GetVulkanApiVersion() {

#if VMA_VULKAN_VERSION == 1002000
  return VK_API_VERSION_1_2;
#elif VMA_VULKAN_VERSION == 1001000
  return VK_API_VERSION_1_1;
#elif VMA_VULKAN_VERSION == 1000000
  return VK_API_VERSION_1_0;
#else
#error Invalid VMA_VULKAN_VERSION.
  return UINT32_MAX;
#endif
}

VKHRESULT InitializeVmaAllocator(
  VkInstance pInstance,
  VkPhysicalDevice pPhysicalDevice,
  VkDevice pDevice
) {

  VKHRESULT hr;
  VmaAllocatorCreateInfo createInfo = {};

  createInfo.physicalDevice = pPhysicalDevice;
  createInfo.device = pDevice;
  createInfo.instance = pInstance;
  createInfo.vulkanApiVersion = GetVulkanApiVersion();
  createInfo.flags = 0;
  V_RETURN(vmaCreateAllocator(&createInfo, &g_pVmaAllocator));

  VkPhysicalDeviceProperties properties;

  vkGetPhysicalDeviceProperties(pPhysicalDevice, &properties);

  g_aResourceBindingConfig.MinUniformBufferOffsetAlignment = (UINT)properties.limits.minUniformBufferOffsetAlignment;

  return hr;
}

void DestroyVmaAllocator() {
  if (g_pVmaAllocator) {
    vmaDestroyAllocator(g_pVmaAllocator);
    g_pVmaAllocator = nullptr;
  }
}

void DestroyVmaBuffer(
  VkBuffer pBuffer,
  VMAHandle pMem
) {
  vmaDestroyBuffer(g_pVmaAllocator, pBuffer, (VmaAllocation)pMem);
}

void DestroyVmaImage(
  VkImage pBuffer,
  VMAHandle pMem
) {
  vmaDestroyImage(g_pVmaAllocator, pBuffer, (VmaAllocation)pMem);
}

VMAHandle GetVmaAllocator() {
  return (VMAHandle)g_pVmaAllocator;
}

VKHRESULT CreateDefaultBuffer(
  VkDevice pDevice,
  VkCommandBuffer pCmdBuffer,
  const void *pInitData,
  size_t uByteSize,
  VkBufferUsageFlags bufferUsage,
  VkBuffer *ppUploadBuffer,
  VMAHandle *ppUploadMem,
  VkBuffer *ppDefaultBuffer,
  VMAHandle *ppDefaultMem
) {
  VKHRESULT hr;
  VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
  bufferInfo.size = uByteSize;
  bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
  allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

  VmaAllocationInfo mappedInfo = {};
  V(vmaCreateBuffer(g_pVmaAllocator, &bufferInfo, &allocInfo, ppUploadBuffer, (VmaAllocation*)ppUploadMem, &mappedInfo));
  if (VK_FAILED(hr))
    return hr;
  memcpy(mappedInfo.pMappedData, pInitData, uByteSize);

  bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | bufferUsage;
  allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  allocInfo.flags = 0;

  V_RETURN(vmaCreateBuffer(g_pVmaAllocator, &bufferInfo, &allocInfo, ppDefaultBuffer, (VmaAllocation*)ppDefaultMem, nullptr));
  if (VK_FAILED(hr)) {
    vmaDestroyBuffer(g_pVmaAllocator, *ppUploadBuffer, (VmaAllocation)*ppUploadMem);
    *ppUploadBuffer = NULL;
    ppUploadMem = NULL;
    return hr;
  }

  VkBufferCopy copyRegion = {
    0,
    0,
    uByteSize
  };
  vkCmdCopyBuffer(pCmdBuffer, *ppUploadBuffer, *ppDefaultBuffer, 1, &copyRegion);

  return hr;
}

uint32_t CalcUniformBufferByteSize(uint32_t uByteSize) {
  return (uByteSize + g_aResourceBindingConfig.MinUniformBufferOffsetAlignment - 1) &
    (~(g_aResourceBindingConfig.MinUniformBufferOffsetAlignment - 1));
}

VKHRESULT CreateUploadBuffer(
  VkDevice pDevice,
  size_t uByteSize,
  VkBufferUsageFlags bufferUsage,
  VkBuffer *ppUploadBuffer,
  VMAHandle *ppUploadMem,
  _Out_opt_ void ** ppMappedData
) {

  VKHRESULT hr;

  VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
  bufferInfo.size = uByteSize;
  bufferInfo.usage = bufferUsage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
  allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

  VmaAllocationInfo mappedInfo = {};
  V(vmaCreateBuffer(g_pVmaAllocator, &bufferInfo, &allocInfo, ppUploadBuffer, (VmaAllocation*)ppUploadMem, &mappedInfo));
  if (VK_FAILED(hr))
    return hr;

  if (ppMappedData)
    *ppMappedData = mappedInfo.pMappedData;

  return hr;
}

VKHRESULT CreateDefaultTexture(
  VkDevice pDevice,
  VkImageCreateInfo *pCreateInfo,
  VkImage *ppTexture,
  VMAHandle *ppTextureMem
) {
  VKHRESULT hr;

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  allocInfo.flags = 0;

  V_RETURN(vmaCreateImage(g_pVmaAllocator, pCreateInfo, &allocInfo,
    ppTexture, (VmaAllocation *)ppTextureMem, nullptr));

  return hr;
}

