#pragma once
#include "VkUtilities.h"
#include <vector>


class VkPipelineDescriptorSignature
{
public:
  VkPipelineDescriptorSignature();
  ~VkPipelineDescriptorSignature();

private:
  VkPipelineLayout m_pSignature;
  std::vector<VkDescriptorSetLayout> m_aDescriptorSetLayout;
};

