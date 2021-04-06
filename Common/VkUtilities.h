#ifndef __VK_UTILITIES_H__
#define __VK_UTILITIES_H__

#include <vulkan/vulkan.h>

#include "Common.h"

typedef long VKHRESULT;

struct VMAHandle_T {};

typedef VMAHandle_T *VMAHandle;

#ifndef VK_SUCCEEDED
#define VK_SUCCEEDED(hr) ((hr) == VK_SUCCESS)
#endif /*VK_SUCCEEDED*/

#ifndef VK_FAILED
#define VK_FAILED(hr) ((hr) != VK_SUCCESS)
#endif /*VK_FAILED*/

#define VK_TRACE(...)  vkUtilsTrace(__VA_ARGS__)

void vkUtilsTrace(const char *fmt, ...);

#if defined(DEBUG) || defined(_DEBUG)
#ifndef V
#define V(x)           { hr = (x); if(hr != VK_SUCCESS) { VK_TRACE(#x), _ASSERT( 0 ); } }
#endif
#ifndef V_RETURN
#define V_RETURN(x)    { hr = (x); if( hr != VK_SUCCESS ) { VK_TRACE(#x), _ASSERT( 0 ); return hr; } }
#endif
#else
#ifndef V
#define V(x)           { hr = (x); }
#endif
#ifndef V_RETURN
#define V_RETURN(x)    { hr = (x); if( hr != VK_SUCCESS ) { return hr; } }
#endif
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(p)       { if (p) { delete (p);     (p) = nullptr; } }
#endif
#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if (p) { delete[] (p);   (p) = nullptr; } }
#endif
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p) = nullptr; } }
#endif

#ifndef SAFE_ADDREF
#define SAFE_ADDREF(p) { (p) ? (p)->AddRef() : (ULONG)0; }
#endif

extern
VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pCallback
    );

extern
void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT callback,
    const VkAllocationCallbacks *pAllocator
    );

extern
VkShaderModule CreateShaderModule(
  VkDevice pDevice,
  const uint32_t *pBytes,
  size_t bytesLength
);

extern
VkShaderModule CreateShaderModuleFromSPIRVFile(
  VkDevice pDevice,
  const wchar_t *pFileName
);

extern uint32_t GetVulkanApiVersion();

extern
VKHRESULT InitializeVmaAllocator(
  VkInstance pInstance,
  VkPhysicalDevice pPhysicalDevice,
  VkDevice pDevice
);

extern
void DestroyVmaAllocator();

extern
VMAHandle GetVmaAllocator();

extern void DestroyVmaBuffer(
  VkBuffer pBuffer,
  VMAHandle pMem
);

extern void DestroyVmaImage(
  VkImage pBuffer,
  VMAHandle pMem
);

extern
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
);

extern uint32_t CalcUniformBufferByteSize(uint32_t uByteSize);

///
/// Before call this function, call `CalcUniformBufferByteSize` to compute the
/// aligment size of the buffers.
///
extern
VKHRESULT CreateUploadBuffer(
  VkDevice pDevice,
  size_t uByteSize,
  VkBufferUsageFlags bufferUsage,
  VkBuffer *ppUploadBuffer,
  VMAHandle *ppUploadMem,
  _Out_opt_ void **ppMappedData
);

extern
VKHRESULT CreateDefaultTexture(
  VkDevice pDevice,
  VkImageCreateInfo *pCreateInfo,
  VkImage *ppTexture,
  VMAHandle *ppTextureMem
);


#endif/* __VK_UTILITIES_H__ */
