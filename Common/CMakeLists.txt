project(Common VERSION 0.1.0)

set(SOURCE_FILE_LIST
  Common.cpp
  VkPipelineDescriptorSignature.cpp
  VkTexture.cpp
  VkUploadBuffer.cpp
  VkUtilities.cpp
  VulkanApp.cpp
  Camera.cpp
  GameTimer.cpp
)

add_library(Common
  ${SOURCE_FILE_LIST}
)