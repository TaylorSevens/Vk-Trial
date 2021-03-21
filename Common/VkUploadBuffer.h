#pragma once
#include "VkUtilities.h"

class VkUploadBuffer
{
public:
  VkUploadBuffer();
  ~VkUploadBuffer();

  VKHRESULT CreateBuffer(
    VkDevice pDevice,
    UINT uElementCount,
    UINT cbElement,
    BOOL bIsConstant
  );

  VOID FreeBuffer();

  VOID CopyData(const void *pBuffer, UINT cbBuffer, UINT iIndex);

  VkBuffer GetResource() const;

  UINT GetBufferSize() const;

private:
  VkBuffer        m_pUploadBuffer;
  VMAHandle       m_pUploadBufferMem;
  BYTE            *m_pMappedData;
  UINT            m_cbPerElement;
  UINT            m_uElementCount;
  UINT            m_cbElementStride;
};

