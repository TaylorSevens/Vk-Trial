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
  UINT uElementCount,
  UINT cbElement,
  BOOL bIsConstant
) {
  VKHRESULT hr;
  UINT cbElementStride = cbElement;

  if (bIsConstant)
    cbElementStride = CalcUniformBufferByteSize(cbElement);

  V_RETURN(CreateUploadBuffer(pDevice, cbElementStride * uElementCount,
    bIsConstant ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    &m_pUploadBuffer, &m_pUploadBufferMem, (VOID **)&m_pMappedData));

  m_cbPerElement = cbElement;
  m_cbElementStride = cbElementStride;
  m_uElementCount = uElementCount;

  return hr;
}

VkUploadBuffer::~VkUploadBuffer() {
  FreeBuffer();
}

VOID VkUploadBuffer::FreeBuffer() {
  if (m_pUploadBufferMem) {
    DestroyVmaBuffer(m_pUploadBuffer, m_pUploadBufferMem);
    m_pUploadBuffer = nullptr;
    m_pUploadBufferMem = nullptr;
  }
}

VOID VkUploadBuffer::CopyData(const void *pBuffer, UINT cbBuffer, UINT iIndex) {
  HRESULT hr;
  V(cbBuffer >= m_cbPerElement && iIndex < m_uElementCount ? S_OK : E_INVALIDARG);
  if (cbBuffer >= m_cbPerElement && iIndex < m_uElementCount) {
    memcpy(m_pMappedData + iIndex * m_cbElementStride, pBuffer, m_cbPerElement);
  }
}

VkBuffer VkUploadBuffer::GetResource() const {
  return m_pUploadBuffer;
}

UINT VkUploadBuffer::GetBufferSize() const {
  return m_cbElementStride * m_uElementCount;
}

