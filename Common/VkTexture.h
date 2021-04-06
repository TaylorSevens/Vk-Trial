#pragma once
#include "VkUtilities.h"

class VkTexture
{
public:
  VkTexture();
  ~VkTexture();

  VKHRESULT LoadFromDDSFile(
    _In_ VkDevice pDevice,
    _In_ VkCommandBuffer pCmdBuffer,
    _In_z_ const wchar_t *pszFileName,
    _In_ VkAccessFlags accessFlags,
    _In_ VkImageLayout destLayout,
    _In_ VkPipelineStageFlags destPipelineStage
  );

  void DisposeUploaders();

  void DisposeFinally(_In_ VkDevice pDevice);

  const VkImageView& GetResourceView() const;

private:
  VkImage m_pDefaultBuffer;
  VMAHandle m_pDefaultBufferMem;
  VkBuffer m_pUploadBuffer;
  VMAHandle m_pUploadBufferMem;

  VkImageView m_pTextureView;

  /// Format, Dimension, MipLevels, Layer Count.
  VkFormat m_Format;
  VkImageType m_Dimension;
  uint32_t m_uMipLevels;
  uint32_t m_uLayerCount;

  bool m_bIsCubeMap;
  bool m_bIsVolumeMap;
};

