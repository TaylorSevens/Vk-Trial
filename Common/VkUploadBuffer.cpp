#include "VkUploadBuffer.h"

VkUploadBuffer::VkUploadBuffer() {
  m_pUploadBuffer = nullptr;
  m_pUploadBufferMem = nullptr;
  m_pMappedData = nullptr;
  m_cbPerElement = 0;
  m_uElementCount = 0;
  m_cbElementStride = 0;
}

VKHRESULT VkUploadBuffer::CreateBuffer(
  VkDevice pDevice,
  size_t uElementCount,
  size_t cbElement,
  bool bIsConstant
) {
  VKHRESULT hr;
  size_t cbElementStride = cbElement;

  if (bIsConstant)
    cbElementStride = CalcUniformBufferByteSize((uint32_t)cbElement);

  V_RETURN(CreateUploadBuffer(pDevice, cbElementStride * uElementCount,
    bIsConstant ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    &m_pUploadBuffer, &m_pUploadBufferMem, (void **)&m_pMappedData));

  m_cbPerElement = cbElement;
  m_cbElementStride = cbElementStride;
  m_uElementCount = uElementCount;

  return hr;
}

VkUploadBuffer::~VkUploadBuffer() {
  FreeBuffer();
}

void VkUploadBuffer::FreeBuffer() {
  if (m_pUploadBufferMem) {
    DestroyVmaBuffer(m_pUploadBuffer, m_pUploadBufferMem);
    m_pUploadBuffer = nullptr;
    m_pUploadBufferMem = nullptr;
  }
}

void VkUploadBuffer::CopyData(const void *pBuffer, size_t cbBuffer, int32_t iIndex) {
  VKHRESULT hr;
  V(cbBuffer >= m_cbPerElement && iIndex < m_uElementCount ? VK_SUCCESS : -1);
  if (cbBuffer >= m_cbPerElement && iIndex < m_uElementCount) {
    memcpy(m_pMappedData + iIndex * m_cbElementStride, pBuffer, m_cbPerElement);
  }
}

VkBuffer VkUploadBuffer::GetResource() const {
  return m_pUploadBuffer;
}

size_t VkUploadBuffer::GetBufferSize() const {
  return m_cbElementStride * m_uElementCount;
}

