#include "VkTexture.h"
#include <dxgiformat.h>
#include <DirectXTex.h>

VkTexture::VkTexture() {

  m_pDefaultBuffer = VK_NULL_HANDLE;
  m_pDefaultBufferMem = VK_NULL_HANDLE;
  m_pUploadBuffer = VK_NULL_HANDLE;
  m_pUploadBufferMem = VK_NULL_HANDLE;

  m_Format = VK_FORMAT_UNDEFINED;
  m_Dimension = VK_IMAGE_TYPE_1D;
  m_uMipLevels = 0;
  m_uLayerCount = 0;

  m_bIsCubeMap = FALSE;
  m_bIsVolumeMap = FALSE;
}

VkTexture::~VkTexture() {
  _ASSERT(!m_pUploadBufferMem && !m_pDefaultBufferMem && !m_pTextureView && "Clean up not possible on ctor!");
}

void VkTexture::DisposeUploaders() {
  DestroyVmaBuffer(m_pUploadBuffer, m_pUploadBufferMem);
  m_pUploadBuffer = nullptr; m_pUploadBufferMem = nullptr;
}

void VkTexture::DisposeFinally(_In_ VkDevice pDevice) {
  DestroyVmaBuffer(m_pUploadBuffer, m_pUploadBufferMem);
  m_pUploadBuffer = nullptr; m_pUploadBufferMem = nullptr;
  DestroyVmaImage(m_pDefaultBuffer, m_pDefaultBufferMem);
  m_pDefaultBuffer = nullptr; m_pDefaultBufferMem = nullptr;
  vkDestroyImageView(pDevice, m_pTextureView, nullptr);
  m_pTextureView = nullptr;
}

const VkImageView& VkTexture::GetResourceView() const {
  return m_pTextureView;
}

static VkFormat TranslateDxgiFormatIntoVulkans(DXGI_FORMAT format)
{
  switch (format) {
  case DXGI_FORMAT_B8G8R8A8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM;
  case DXGI_FORMAT_R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
  case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
  case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;
  case DXGI_FORMAT_BC1_UNORM: return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
  case DXGI_FORMAT_BC2_UNORM: return VK_FORMAT_BC2_UNORM_BLOCK;
  case DXGI_FORMAT_BC3_UNORM: return VK_FORMAT_BC3_UNORM_BLOCK;
  case DXGI_FORMAT_BC4_UNORM: return VK_FORMAT_BC4_UNORM_BLOCK;
  case DXGI_FORMAT_BC4_SNORM: return VK_FORMAT_BC4_UNORM_BLOCK;
  case DXGI_FORMAT_BC5_UNORM: return VK_FORMAT_BC5_UNORM_BLOCK;
  case DXGI_FORMAT_BC5_SNORM: return VK_FORMAT_BC5_UNORM_BLOCK;
  case DXGI_FORMAT_BC1_UNORM_SRGB: return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
  case DXGI_FORMAT_BC2_UNORM_SRGB: return VK_FORMAT_BC2_SRGB_BLOCK;
  case DXGI_FORMAT_BC3_UNORM_SRGB: return VK_FORMAT_BC3_SRGB_BLOCK;
  case DXGI_FORMAT_R10G10B10A2_UNORM: return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
  case DXGI_FORMAT_R16G16B16A16_FLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;
  default: _ASSERT(false);  return VK_FORMAT_UNDEFINED;
  }
}

VKHRESULT VkTexture::LoadFromDDSFile(
  _In_ VkDevice pDevice,
  _In_ VkCommandBuffer pCmdBuffer,
  _In_z_ const wchar_t *pszFileName,
  _In_ VkAccessFlags accessFlags,
  _In_ VkImageLayout destLayout,
  _In_ VkPipelineStageFlags destPipelineStage
) {
  VKHRESULT hr;

  DirectX::ScratchImage images;
  DirectX::TexMetadata metaData;
  WCHAR szPath[MAX_PATH];

  if(FindDemoMediaFileAbsPath(pszFileName, MAX_PATH, szPath))
    return VK_NULL_HANDLE;

  hr = DirectX::LoadFromDDSFile(szPath, DirectX::DDS_FLAGS_ALLOW_LARGE_FILES,
    &metaData, images);
  if (FAILED(hr)) {
    V(!(hr && "Can not load dds texture from file!"));
    return hr;
  }

  VkImageCreateInfo imageInfo = {
    VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // sType;
    nullptr, // pNext;
    0, // flags;
    VkImageType(metaData.dimension - 2), // imageType;
    TranslateDxgiFormatIntoVulkans(metaData.format), // format;
    { (uint32_t)metaData.width, (uint32_t)metaData.height, (uint32_t)metaData.depth }, // extent;
    (uint32_t)metaData.mipLevels, // mipLevels;
    (uint32_t)metaData.arraySize, // arrayLayers;
    VK_SAMPLE_COUNT_1_BIT, // samples;
    VK_IMAGE_TILING_OPTIMAL, // tiling;
    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // usage;
    VK_SHARING_MODE_EXCLUSIVE, // sharingMode;
    0, // queueFamilyIndexCount;
    nullptr, // pQueueFamilyIndices;
    VK_IMAGE_LAYOUT_UNDEFINED // initialLayout;
  };

  V_RETURN(CreateDefaultTexture(pDevice, &imageInfo, &m_pDefaultBuffer, &m_pDefaultBufferMem));

  VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
  viewInfo.image = m_pDefaultBuffer;
  switch (imageInfo.imageType) {
  case VK_IMAGE_TYPE_1D:
    if (imageInfo.arrayLayers == 1)
      viewInfo.viewType = VK_IMAGE_VIEW_TYPE_1D;
    else
      viewInfo.viewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
    break;
  case VK_IMAGE_TYPE_2D:
    if (imageInfo.arrayLayers == 1)
      viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    else
      viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    break;
  case VK_IMAGE_TYPE_3D:
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
  }

  if (metaData.IsCubemap()) {
    if (imageInfo.arrayLayers == 1)
      viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    else
      viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
  } else if (metaData.IsVolumemap())
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;

  viewInfo.format = imageInfo.format;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = imageInfo.mipLevels;
  viewInfo.subresourceRange.layerCount = imageInfo.arrayLayers;

  V(vkCreateImageView(pDevice, &viewInfo, nullptr, &m_pTextureView));
  if (VK_FAILED(hr)) {
    DestroyVmaImage(m_pDefaultBuffer, m_pDefaultBufferMem);
    m_pDefaultBuffer = nullptr; m_pDefaultBufferMem = nullptr;
    return hr;
  }

  /// Copy texels' data.
  void *pMappedData;

  V(CreateUploadBuffer(pDevice, images.GetPixelsSize(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &m_pUploadBuffer, &m_pUploadBufferMem, &pMappedData));
  if (VK_FAILED(hr)) {
    DestroyVmaBuffer(m_pUploadBuffer, m_pUploadBufferMem);
    m_pUploadBuffer = nullptr; m_pUploadBufferMem = nullptr;
    vkDestroyImageView(pDevice, m_pTextureView, nullptr);
    m_pTextureView = nullptr;
    return hr;
  }
  memcpy(pMappedData, images.GetPixels(), images.GetPixelsSize());

  std::vector<VkBufferImageCopy> copyRegions(images.GetImageCount());
  const DirectX::Image *pImage = images.GetImages();

  if (images.GetImageCount() == (metaData.depth * metaData.arraySize * metaData.mipLevels)) {
    uint32_t i, j, k, index;

    index = 0;
    for (i = 0; i < metaData.depth; ++i) {
      for (j = 0; j < metaData.arraySize; ++j) {
        for (k = 0; k < metaData.mipLevels; ++k) {
          index = (uint32_t)metaData.ComputeIndex(k, j, i);
          pImage = images.GetImage(k, j, i);

          auto &copyRegion = copyRegions[index];
          copyRegion.bufferOffset = pImage->pixels - images.GetPixels();
          copyRegion.bufferRowLength = 0;
          copyRegion.bufferImageHeight = 0;
          copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
          copyRegion.imageSubresource.mipLevel = k;
          copyRegion.imageSubresource.baseArrayLayer = j;
          copyRegion.imageSubresource.layerCount = (uint32_t)metaData.arraySize;
          copyRegion.imageOffset.x = 0;
          copyRegion.imageOffset.y = 0;
          copyRegion.imageOffset.z = 0;
          copyRegion.imageExtent.width = (uint32_t)pImage->width;
          copyRegion.imageExtent.height = (uint32_t)pImage->height;
          copyRegion.imageExtent.depth = 1;
        }
      }
    }
  } else {
    V(!!("Error in loaded image data source!"));
    return hr;
  }

  VkImageMemoryBarrier barrier = {
    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // sType;
    nullptr, // pNext;
    0, // srcAccessMask;
    VK_ACCESS_TRANSFER_WRITE_BIT, // dstAccessMask;
    VK_IMAGE_LAYOUT_UNDEFINED, // oldLayout;
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // newLayout;
    VK_QUEUE_FAMILY_IGNORED, // srcQueueFamilyIndex;
    VK_QUEUE_FAMILY_IGNORED, // dstQueueFamilyIndex;
    m_pDefaultBuffer, // image;
    {
      VK_IMAGE_ASPECT_COLOR_BIT, // aspectMask;
      0, // baseMipLevel;
      (uint32_t)metaData.mipLevels, // levelCount;
      0, // baseArrayLayer;
      (uint32_t)metaData.arraySize // layerCount;
    }
  };
  vkCmdPipelineBarrier(
    pCmdBuffer,
    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
    0, 0, 0,
    0, nullptr,
    1, &barrier
  );

  /// Recorde a command and a barrier to copy the resource.
  vkCmdCopyBufferToImage(pCmdBuffer,
    m_pUploadBuffer, m_pDefaultBuffer,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    (uint32_t)copyRegions.size(),
    copyRegions.data());

  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = accessFlags;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = destLayout;
  vkCmdPipelineBarrier(
    pCmdBuffer,
    VK_PIPELINE_STAGE_TRANSFER_BIT, destPipelineStage,
    0, 0, 0,
    0, nullptr,
    1, &barrier
  );

  /// Texture information.
  m_Format = imageInfo.format;
  m_Dimension = imageInfo.imageType;
  m_uMipLevels = imageInfo.mipLevels;
  m_uLayerCount = imageInfo.arrayLayers;

  m_bIsCubeMap = metaData.IsCubemap();
  m_bIsVolumeMap = metaData.IsVolumemap();

  return hr;

}



