project(Common VERSION 0.1.0)

set(SOURCE_FILE_LIST
  Common.cpp
  VkPipelineDescriptorSignature.cpp
  VkTexture.cpp
  VkUploadBuffer.cpp
  VkUtilities.cpp
  VulkanRenderContext.cpp
  VulkanRenderContext.hpp
  Camera.cpp
  Camera.hpp
  GameTimer.cpp
  GameTimer.hpp
  GeometryGenerator.cpp
  GeometryGenerator.hpp
)

add_library(Common
  ${SOURCE_FILE_LIST}
)