#pragma once
#include "VkUtilities.h"

class VkUploadBuffer
{
public:
  VkUploadBuffer();
  ~VkUploadBuffer();

  VKHRESULT CreateBuffer(
    VkDevice pDevice,
    size_t uElementCount,
    size_t cbElement,
    bool bIsConstant
  );

  void FreeBuffer();

  void CopyData(const void *pBuffer, size_t cbBuffer, int32_t iIndex);

  VkBuffer GetResource() const;

  size_t GetBufferSize() const;

private:
  VkBuffer        m_pUploadBuffer;
  VMAHandle       m_pUploadBufferMem;
  char            *m_pMappedData;
  size_t          m_cbPerElement;
  size_t          m_uElementCount;
  size_t          m_cbElementStride;
};

