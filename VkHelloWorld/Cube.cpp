#include <VulkanApp.h>
#include <Windows.h>
#include <vector>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include "Camera.h"
#include "VkUploadBuffer.h"
#include "GeometryGenerator.h"

#include "VkTexture.h"

using namespace DirectX;

static
VulkanApp *CreateAppInstance(HINSTANCE hInstance);

int main() {

  VKHRESULT hr;
  VulkanApp *pTheApp;

  // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

  pTheApp = CreateAppInstance(NULL);
  V(pTheApp->Initialize());
  if (VK_FAILED(hr)) {
    SAFE_DELETE(pTheApp);
    return hr;
  }

  pTheApp->Run();
  SAFE_DELETE(pTheApp);

  return hr;
}


struct ObjectConstants {
  XMFLOAT4X4 WorldViewProj;
  XMFLOAT4X4 TexTransform;
};

class FrameResources {
public:
  static VKHRESULT CreateBuffers(VkDevice pDevice, UINT uFrameCount) {
    VKHRESULT hr;

    V_RETURN(ObjectUBs.CreateBuffer(pDevice, uFrameCount, sizeof(ObjectConstants), TRUE));

    return hr;
  }

  static VOID FreeBuffers() {
    ObjectUBs.FreeBuffer();
  }

  static VkUploadBuffer ObjectUBs;
};

VkUploadBuffer FrameResources::ObjectUBs;

class CubeApp : public VulkanApp {
public:
  CubeApp(HINSTANCE hInstance)
    : VulkanApp(hInstance)
  {
    m_aDeviceConfig.MsaaEnabled = true;
    m_aDeviceConfig.MsaaQaulityLevel = 8;

    m_aDeviceConfig.VsyncEnabled = TRUE;

    m_MainWndCaption = L"TestTriangle";

    ZeroMemory(m_aStaticSamplers, sizeof(m_aStaticSamplers));

    m_pDiffuseDescriptorSetLayout = VK_NULL_HANDLE;
    m_pDescriptorSetLayout = VK_NULL_HANDLE;
    m_pPipelineLayout = VK_NULL_HANDLE;
    m_pPSO = VK_NULL_HANDLE;

    m_pVertexBuffer = VK_NULL_HANDLE;
    m_pIndexBuffer = VK_NULL_HANDLE;

    m_pVertexUploadBuffer = VK_NULL_HANDLE;
    m_pVertexUploadMem = VK_NULL_HANDLE;
    m_pVertexBuffer = VK_NULL_HANDLE;
    m_pVertexMem = VK_NULL_HANDLE;
    m_pIndexUploadBuffer = VK_NULL_HANDLE;
    m_pIndexUploadMem = VK_NULL_HANDLE;
    m_pIndexBuffer = VK_NULL_HANDLE;
    m_pIndexMem = VK_NULL_HANDLE;

    m_pDescriptorPool = VK_NULL_HANDLE;

    m_uIndexCount = 0;

    m_Camera.SetOrbit(5.0f, 1.25f * XM_PI, 0.25f * XM_PI);
  }

  virtual VKHRESULT Initialize() override {

    HRESULT hr;
    V_RETURN(__super::Initialize());

    /// Start Record initialize commands.
    auto pCmdBuffer = m_aRendererItemCtx[m_iCurrRendererItem].pCommandBuffer;
    auto pFence = m_aRendererItemCtx[m_iCurrRendererItem].pCmdBufferInFightFence;

    VkCommandBufferBeginInfo cmdBeginInfo = {
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, //sType;
      nullptr, //pNext;
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, //flags;
      nullptr, //pInheritanceInfo;
    };
    V(vkBeginCommandBuffer(pCmdBuffer, &cmdBeginInfo));

    V_RETURN(CreateBuffers());
    V_RETURN(LoadTextures());
    V_RETURN(CreateStaticSamplers());

    V_RETURN(CreatePSOs());
    V_RETURN(CreateDescriptorPool());
    V_RETURN(CreateDescriptorSets());

    /// Submit intialize commands.
    vkEndCommandBuffer(pCmdBuffer);

    VkSubmitInfo submitInfo = {
      VK_STRUCTURE_TYPE_SUBMIT_INFO, // sType;
      nullptr, // pNext;
      0, // waitSemaphoreCount;
      nullptr, // pWaitSemaphores;
      nullptr, // pWaitDstStageMask;
      1, // commandBufferCount;
      &pCmdBuffer, // pCommandBuffers;
      0,// signalSemaphoreCount;
      nullptr// pSignalSemaphores;
    };

    vkResetFences(m_pDevice, 1, &pFence);
    V(vkQueueSubmit(m_pGraphicQueue, 1, &submitInfo, pFence));
    vkWaitForFences(m_pDevice, 1, &pFence, FALSE, UINT64_MAX);

    /// Right not post some clean up.
    PostInitialize();

    return hr;
  }

  void PostInitialize() {

    DestroyVmaBuffer(m_pVertexUploadBuffer, m_pVertexUploadMem);
    m_pVertexUploadBuffer = nullptr; m_pVertexUploadMem = nullptr;
    DestroyVmaBuffer(m_pIndexUploadBuffer, m_pIndexUploadMem);
    m_pIndexUploadBuffer = nullptr; m_pIndexUploadMem = nullptr;

    m_aDiffuseMap.DisposeUploaders();
    m_aMaskDiffuseMap.DisposeUploaders();
  }

  virtual void Cleanup() override {

    FrameResources::FreeBuffers();

    for (auto &sampler : m_aStaticSamplers) {
      vkDestroySampler(m_pDevice, sampler, nullptr);
      sampler = VK_NULL_HANDLE;
    }

    DestroyVmaBuffer(m_pVertexUploadBuffer, m_pVertexUploadMem);
    DestroyVmaBuffer(m_pVertexBuffer, m_pVertexMem);
    DestroyVmaBuffer(m_pIndexUploadBuffer, m_pIndexUploadMem);
    DestroyVmaBuffer(m_pIndexBuffer, m_pIndexMem);

    m_aDiffuseMap.DisposeFinally(m_pDevice);
    m_aMaskDiffuseMap.DisposeFinally(m_pDevice);

    vkDestroyDescriptorPool(m_pDevice, m_pDescriptorPool, nullptr);

    vkDestroyPipeline(m_pDevice, m_pPSO, nullptr);
    vkDestroyPipelineLayout(m_pDevice, m_pPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_pDevice, m_pDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_pDevice, m_pDiffuseDescriptorSetLayout, nullptr);

    __super::Cleanup();
  }

  virtual void Update(float fTime, float fTimeElapsed) override {

    ObjectConstants objConstants;
    XMMATRIX matWVP;

    matWVP = XMMatrixIdentity();
    matWVP = matWVP * XMLoadFloat4x4(&m_Camera.GetViewProj());

    XMStoreFloat4x4(&objConstants.WorldViewProj, matWVP);

    XMMATRIX matRotate = XMMatrixRotationAxis(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), fTimeElapsed * 2.0f * XM_PI);
    XMMATRIX matTrans1 = XMMatrixTranslation(-0.5f, -0.5f, 0.0f);
    XMMATRIX matTrans2 = XMMatrixTranslation(0.5f, 0.5f, 0.0f);

    /// GLSL have different texture coodinates with D3D, so, we need to fix it.
    XMMATRIX matGLSLTexcoordsFixup = XMMatrixSet(
      1.0f, .0f, .0f, .0f,
      .0f, -1.0f, .0f, .0f,
      .0f, .0f, 1.0f, .0f,
      .0f, 1.0f, .0f, 1.0f
    );

    XMStoreFloat4x4(&objConstants.TexTransform, matGLSLTexcoordsFixup * matTrans1 * matRotate * matTrans2);

    FrameResources::ObjectUBs.CopyData(&objConstants, sizeof(objConstants), m_iCurrRendererItem);
  }

  virtual void RenderFrame(float fTime, float fTimeElapsed) override {

    VKHRESULT hr;
    RendererItemContext *pRendererContext = &m_aRendererItemCtx[m_iCurrRendererItem];
    VkCommandBuffer pCmdBuffer = pRendererContext->pCommandBuffer;
    SwapChainItemContext *pSwapchainContext;

    V(PrepareNextFrame(&pSwapchainContext));

    V(WaitForPreviousGraphicsCommandBufferFence(pRendererContext));

    VkCommandBufferBeginInfo cmdBeginInfo = {
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      nullptr,
      VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
      nullptr
    };
    VkClearValue clearValue[2] = {};
    VkRenderPassBeginInfo passBeginInfo = {
      VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      nullptr,
      m_pSwapChainFBsCompatibleRenderPass,
      pSwapchainContext->pFrameBuffer,
      m_ScissorRect,
      2,
      clearValue
    };

    memcpy(&clearValue[0].color, &Colors::LightBlue, sizeof(XMVECTORF32));
    clearValue[1].depthStencil.depth = 1.0f;
    clearValue[1].depthStencil.stencil = 0;

    /// Reset command buffer.
    V(vkResetCommandBuffer(pCmdBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT));

    V(vkBeginCommandBuffer(pCmdBuffer, &cmdBeginInfo));

      vkCmdBeginRenderPass(pCmdBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

      VkDescriptorSet descriptorSets[2] = { m_aDescriptorSets[m_iCurrRendererItem], m_pDiffuseDiscriptorSet };

        vkCmdBindDescriptorSets(pCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
          m_pPipelineLayout, 0, _countof(descriptorSets), descriptorSets,
          0, nullptr);

        vkCmdBindPipeline(pCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pPSO);

        VkDeviceSize vbOffsets[] = { 0 };
        vkCmdBindVertexBuffers(pCmdBuffer, 0, 1, &m_pVertexBuffer, vbOffsets);
        vkCmdBindIndexBuffer(pCmdBuffer, m_pIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(pCmdBuffer, m_uIndexCount, 1, 0, 0, 0);

      vkCmdEndRenderPass(pCmdBuffer);

    V(vkEndCommandBuffer(pCmdBuffer));

    VkSubmitInfo commitInfo = {};
    VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    commitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    commitInfo.waitSemaphoreCount = 1;
    commitInfo.pWaitSemaphores = &pSwapchainContext->pImageAvailableSem;
    commitInfo.pWaitDstStageMask = waitStages;
    commitInfo.commandBufferCount = 1;
    commitInfo.pCommandBuffers = &pCmdBuffer;
    commitInfo.signalSemaphoreCount = 1;
    commitInfo.pSignalSemaphores = &pRendererContext->pRenderFinishedSem;

    V(vkQueueSubmit(m_pGraphicQueue, 1, &commitInfo, pRendererContext->pCmdBufferInFightFence));

    V(Present());
  }

  virtual LRESULT OnResize() {

    VKHRESULT hr;

    m_Camera.SetProjMatrix(0.25f * XM_PI, GetAspectRatio(), 1.0f, 1000.0f);

    V(vkDeviceWaitIdle(m_pDevice));

    V(RecreateSwapChain());

    V(CreatePSOs());

    return (LRESULT)hr;
  }

  virtual LRESULT OnMouseEvent(UINT uMsg, WPARAM wParam, int x, int y) {
    switch (uMsg) {
    case WM_LBUTTONDOWN:
      m_ptLastMousePos.x = x;
      m_ptLastMousePos.y = y;
      SetCapture(this->m_hMainWnd);
      break;
    case WM_LBUTTONUP:
      ReleaseCapture();
    case WM_MOUSEMOVE:
      if (wParam & MK_LBUTTON) {
        // Make each pixel correspond to a quarter of a degree.
        float dx = XMConvertToRadians(0.25f*static_cast<float>(x - m_ptLastMousePos.x));
        float dy = XMConvertToRadians(0.25f*static_cast<float>(y - m_ptLastMousePos.y));
        m_Camera.Rotate(dx, dy);
      } else if (wParam & MK_RBUTTON) {
        // Make each pixel correspond to 0.005 unit in the scene.
        float dx = 0.005f*static_cast<float>(x - m_ptLastMousePos.x);
        float dy = 0.005f*static_cast<float>(y - m_ptLastMousePos.y);

        m_Camera.Scale(dx - dy, 3.0f, 15.0f);
      }

      m_ptLastMousePos.x = x;
      m_ptLastMousePos.y = y;
      break;
    }

    return 0;
  }

private:
  VKHRESULT LoadTextures() {
    VKHRESULT hr;
    VkCommandBuffer pCmdBuffer = m_aRendererItemCtx[m_iCurrRendererItem].pCommandBuffer;

    V_RETURN(m_aDiffuseMap.LoadFromDDSFile(
      m_pDevice,
      pCmdBuffer,
      L"Media/Textures/DX11/flare.dds",
      VK_ACCESS_SHADER_READ_BIT,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
    ));

    V_RETURN(m_aMaskDiffuseMap.LoadFromDDSFile(
      m_pDevice,
      pCmdBuffer,
      L"Media/Textures/DX11/flarealpha.dds",
      VK_ACCESS_SHADER_READ_BIT,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
    ));

    return hr;
  }

  VKHRESULT CreateBuffers() {

    VKHRESULT hr;
    GeometryGenerator gen;
    using Vertex = GeometryGenerator::Vertex;
    auto box = gen.CreateBox(2.0f, 2.0f, 2.0f, 0);
    auto &vertices = box.Vertices;
    auto &indices = box.GetIndices16();

    size_t vbSize = vertices.size()*sizeof(Vertex);
    size_t ibSize = indices.size()*sizeof(uint16_t);

    auto pCmdBuffer = m_aRendererItemCtx[m_iCurrRendererItem].pCommandBuffer;

    V_RETURN(CreateDefaultBuffer(m_pDevice, pCmdBuffer, vertices.data(), vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      &m_pVertexUploadBuffer, &m_pVertexUploadMem, &m_pVertexBuffer, &m_pVertexMem));

    V_RETURN(CreateDefaultBuffer(m_pDevice, pCmdBuffer, indices.data(), ibSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
      &m_pIndexUploadBuffer, &m_pIndexUploadMem, &m_pIndexBuffer, &m_pIndexMem));
    m_uIndexCount = (uint32_t)indices.size();

    V_RETURN(FrameResources::CreateBuffers(m_pDevice, _countof(m_aRendererItemCtx)));

    return hr;
  }

  VKHRESULT CreateStaticSamplers() {

    HRESULT hr;
    VkSamplerCreateInfo samplerInfo = {
      VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, // sType;
      nullptr, // pNext;
      0, // flags;
      VK_FILTER_LINEAR, // magFilter;
      VK_FILTER_LINEAR, // minFilter;
      VK_SAMPLER_MIPMAP_MODE_LINEAR, // mipmapMode;
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, // addressModeU;
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, // addressModeV;
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, // addressModeW;
      .0f, // mipLodBias;
      VK_FALSE, // anisotropyEnable;
      1, // maxAnisotropy;
      FALSE, // compareEnable;
      VK_COMPARE_OP_ALWAYS, // compareOp;
      0, // minLod;
      9, // maxLod;
      VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,// borderColor;
      VK_FALSE, // unnormalizedCoordinates;
    };
    V_RETURN(vkCreateSampler(m_pDevice, &samplerInfo, nullptr, &m_aStaticSamplers[0]));

    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    V_RETURN(vkCreateSampler(m_pDevice, &samplerInfo, nullptr, &m_aStaticSamplers[1]));

    return hr;
  }

  VKHRESULT CreatePSOs() {
    VKHRESULT hr;
    VkShaderModule shaderModules[2] = {0};
    /// Shader Stages.
    VkPipelineShaderStageCreateInfo shaderStageInfos[2] = {};
    VkPipelineVertexInputStateCreateInfo VIinfo = {};
    VkPipelineInputAssemblyStateCreateInfo IAinfo = {};
    VkPipelineViewportStateCreateInfo VPinfo = {};
    VkPipelineRasterizationStateCreateInfo RSinfo = {};
    VkPipelineMultisampleStateCreateInfo MSAAinfo = {};
    VkPipelineDepthStencilStateCreateInfo DSSinfo = {};
    VkPipelineColorBlendAttachmentState BAS = {};
    VkPipelineColorBlendStateCreateInfo BSinfo = {};
    /// PSO
    VkGraphicsPipelineCreateInfo PSOinfo = {};

    shaderModules[0] = CreateShaderModuleFromSPIRVFile(m_pDevice, L"shaders/box.vert.spv");
    shaderModules[1] = CreateShaderModuleFromSPIRVFile(m_pDevice, L"shaders/box.frag.spv");

    shaderStageInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStageInfos[0].module = shaderModules[0];
    shaderStageInfos[0].pName = "main";

    shaderStageInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStageInfos[1].module = shaderModules[1];
    shaderStageInfos[1].pName = "main";

    V_RETURN(CreatePiplineLayout());

    /// Vertex Binding  Information.
    VkVertexInputBindingDescription bindingDescs = {};
    bindingDescs.binding = 0;
    bindingDescs.stride = sizeof(GeometryGenerator::Vertex);
    bindingDescs.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attribDescs[2] = {};
    attribDescs[0].binding = 0;
    attribDescs[0].location = 0;
    attribDescs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribDescs[0].offset = offsetof(GeometryGenerator::Vertex, Position);

    attribDescs[1].binding = 0;
    attribDescs[1].location = 1;
    attribDescs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attribDescs[1].offset = offsetof(GeometryGenerator::Vertex, TexC);

    /// Vertex Input Infos.
    VIinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VIinfo.vertexBindingDescriptionCount = 1;
    VIinfo.pVertexBindingDescriptions = &bindingDescs;
    VIinfo.vertexAttributeDescriptionCount = _countof(attribDescs);
    VIinfo.pVertexAttributeDescriptions = attribDescs;

    /// IA States
    IAinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    IAinfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    IAinfo.primitiveRestartEnable = VK_FALSE;

    VPinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    VPinfo.viewportCount = 1;
    VPinfo.pViewports = &m_Viewport;
    VPinfo.scissorCount = 1;
    VPinfo.pScissors = &m_ScissorRect;

    /// Rasterization States.
    RSinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    RSinfo.depthClampEnable = VK_FALSE;
    RSinfo.rasterizerDiscardEnable = VK_FALSE;
    RSinfo.polygonMode = VK_POLYGON_MODE_FILL;
    RSinfo.lineWidth = 1.0f;
    RSinfo.cullMode = VK_CULL_MODE_BACK_BIT;
    RSinfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    RSinfo.depthBiasEnable = VK_FALSE;

    /// MSAA setting
    MSAAinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    MSAAinfo.sampleShadingEnable = VK_FALSE;
    MSAAinfo.rasterizationSamples = IsMsaaEnabled() ? (VkSampleCountFlagBits)m_aDeviceConfig.MsaaQaulityLevel : VK_SAMPLE_COUNT_1_BIT;
    MSAAinfo.minSampleShading = 1.0f; /// Optional.
    MSAAinfo.pSampleMask = nullptr;
    MSAAinfo.alphaToCoverageEnable = VK_FALSE;

    DSSinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    DSSinfo.depthTestEnable = VK_TRUE;
    DSSinfo.depthCompareOp = VK_COMPARE_OP_LESS;
    DSSinfo.depthWriteEnable = VK_TRUE;
    DSSinfo.stencilTestEnable = VK_FALSE;

    /// Blend State
    BAS.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    BAS.blendEnable = VK_FALSE;
    BSinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    BSinfo.attachmentCount = 1;
    BSinfo.logicOpEnable = VK_FALSE;
    BSinfo.logicOp = VK_LOGIC_OP_COPY;
    BSinfo.pAttachments = &BAS;

    /// PSO
    PSOinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    PSOinfo.layout = m_pPipelineLayout;
    PSOinfo.pVertexInputState = &VIinfo;
    PSOinfo.pInputAssemblyState = &IAinfo;
    PSOinfo.stageCount = _countof(shaderStageInfos);
    PSOinfo.pStages = shaderStageInfos;
    PSOinfo.pViewportState = &VPinfo;
    PSOinfo.pRasterizationState = &RSinfo;
    PSOinfo.pColorBlendState = &BSinfo;
    PSOinfo.pDepthStencilState = &DSSinfo;
    PSOinfo.pMultisampleState = &MSAAinfo;

    PSOinfo.renderPass = m_pSwapChainFBsCompatibleRenderPass;
    PSOinfo.subpass = 0;
    PSOinfo.basePipelineHandle = VK_NULL_HANDLE;
    PSOinfo.basePipelineIndex = -1;

    if (m_pPSO) {
      vkDestroyPipeline(m_pDevice, m_pPSO, nullptr);
      m_pPSO = nullptr;
    }
    V(vkCreateGraphicsPipelines(m_pDevice, nullptr, 1, &PSOinfo, nullptr,
      &m_pPSO));

    vkDestroyShaderModule(m_pDevice, shaderModules[0], nullptr);
    vkDestroyShaderModule(m_pDevice, shaderModules[1], nullptr);

    return hr;
  }

  VKHRESULT CreatePiplineLayout() {

    VKHRESULT hr = S_OK;

    if (!m_pDescriptorSetLayout) {
      /// Create Pipeline layout along with it.
      VkDescriptorSetLayoutBinding layoutBindings[] = {
        {
          0, // binding;
          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // descriptorType;
          1,// descriptorCount;
          VK_SHADER_STAGE_VERTEX_BIT, // stageFlags;
          nullptr// pImmutableSamplers;
        },
      };

      VkDescriptorSetLayoutCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,// sType;
        nullptr,// pNext;
        0,// flags;
        _countof(layoutBindings),// bindingCount;
        layoutBindings// pBindings;
      };
      V_RETURN(vkCreateDescriptorSetLayout(m_pDevice, &createInfo, nullptr, &m_pDescriptorSetLayout));
    }

    if (!m_pDiffuseDescriptorSetLayout) {
      VkDescriptorSetLayoutBinding layoutBindings[] = {
        {
          0,
          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          1,
          VK_SHADER_STAGE_FRAGMENT_BIT,
          nullptr
        },
        {
          1,
          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          1,
          VK_SHADER_STAGE_FRAGMENT_BIT,
          nullptr
        },
      };

      VkDescriptorSetLayoutCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,// sType;
        nullptr,// pNext;
        0,// flags;
        _countof(layoutBindings),// bindingCount;
        layoutBindings// pBindings;
      };
      V_RETURN(vkCreateDescriptorSetLayout(m_pDevice, &createInfo, nullptr, &m_pDiffuseDescriptorSetLayout));
    }

    if (!m_pPipelineLayout) {
      VkPipelineLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, };
      VkDescriptorSetLayout setLayouts[] = { m_pDescriptorSetLayout, m_pDiffuseDescriptorSetLayout };
      layoutInfo.setLayoutCount = _countof(setLayouts);
      layoutInfo.pSetLayouts = setLayouts;
      V_RETURN(vkCreatePipelineLayout(m_pDevice, &layoutInfo, nullptr, &m_pPipelineLayout));
    }

    return hr;
  }

  VKHRESULT CreateDescriptorPool() {

    VKHRESULT hr;
    VkDescriptorPoolSize poolSizes[2] = {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = _countof(m_aRendererItemCtx);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 2;

    VkDescriptorPoolCreateInfo createInfo = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, // sType;
      nullptr, // pNext;
      0, // flags;
      12, // maxSets;
      _countof(poolSizes), // poolSizeCount;
      poolSizes // pPoolSizes;
    };

    V_RETURN(vkCreateDescriptorPool(m_pDevice, &createInfo, nullptr, &m_pDescriptorPool));

    return hr;
  }

  VKHRESULT CreateDescriptorSets() {
    VKHRESULT hr;
    VkDescriptorSetLayout aPerFrameSetLayouts[_countof(m_aDescriptorSets)];
    uint32_t i;
    uint32_t uUBByteOffset;

    for (i = 0; i < _countof(m_aDescriptorSets); ++i) aPerFrameSetLayouts[i] = m_pDescriptorSetLayout;

    VkDescriptorSetAllocateInfo setsInfo = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // sType;
      nullptr, // pNext;
      m_pDescriptorPool, // descriptorPool;
      _countof(m_aDescriptorSets), // descriptorSetCount;
      aPerFrameSetLayouts// pSetLayouts;
    };
    V_RETURN(vkAllocateDescriptorSets(m_pDevice, &setsInfo, m_aDescriptorSets));

    setsInfo.descriptorSetCount = 1;
    setsInfo.pSetLayouts = &m_pDiffuseDescriptorSetLayout;
    V_RETURN(vkAllocateDescriptorSets(m_pDevice, &setsInfo, &m_pDiffuseDiscriptorSet));

    uUBByteOffset = CalcUniformBufferByteSize(sizeof(ObjectConstants));

    for (i = 0; i < _countof(m_aDescriptorSets); ++i) {
      VkDescriptorBufferInfo bufferInfos[1] = {
        {
          FrameResources::ObjectUBs.GetResource(), // buffer;
          uUBByteOffset*i, // offset;
          sizeof(ObjectConstants)// range;
        },
      };

      VkWriteDescriptorSet descriptorWrite[] = {
        {
          VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // sType;
          nullptr, // pNext;
          m_aDescriptorSets[i], // dstSet;
          0, // dstBinding;
          0, // dstArrayElement;
          _countof(bufferInfos), // descriptorCount;
          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // descriptorType;
          nullptr, // pImageInfo;
          bufferInfos, // pBufferInfo;
          nullptr // pTexelBufferView;
        },
      };

      vkUpdateDescriptorSets(m_pDevice, _countof(descriptorWrite), descriptorWrite, 0, nullptr);
    }

    VkDescriptorImageInfo samplerInfos[2] = {
      {
        m_aStaticSamplers[0], // sampler;
        m_aDiffuseMap.GetResourceView(), // imageView;
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL// imageLayout;
      },
      {
        m_aStaticSamplers[0], // sampler;
        m_aMaskDiffuseMap.GetResourceView(), // imageView;
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL// imageLayout;
      },
    };

    VkWriteDescriptorSet descriptorWrite[] = {
      {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // sType;
        nullptr, // pNext;
        m_pDiffuseDiscriptorSet, // dstSet;
        0, // dstBinding;
        0, // dstArrayElement;
        _countof(samplerInfos), // descriptorCount;
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, // descriptorType;
        samplerInfos, // pImageInfo;
        nullptr, // pBufferInfo;
        nullptr // pTexelBufferView;
      },
    };
    vkUpdateDescriptorSets(m_pDevice, _countof(descriptorWrite), descriptorWrite, 0, nullptr);

    return hr;
  }

  VkBuffer m_pVertexBuffer;
  VMAHandle m_pVertexMem;
  VkBuffer m_pVertexUploadBuffer;
  VMAHandle m_pVertexUploadMem;
  VkBuffer m_pIndexBuffer;
  VMAHandle m_pIndexMem;
  VkBuffer m_pIndexUploadBuffer;
  VMAHandle m_pIndexUploadMem;

  VkTexture m_aDiffuseMap;
  VkTexture m_aMaskDiffuseMap;

  VkSampler m_aStaticSamplers[2];

  uint32_t m_uIndexCount;

  VkDescriptorSetLayout m_pDescriptorSetLayout;
  VkDescriptorSetLayout m_pDiffuseDescriptorSetLayout;

  VkPipelineLayout m_pPipelineLayout;
  VkPipeline m_pPSO;

  VkDescriptorPool m_pDescriptorPool;
  VkDescriptorSet m_aDescriptorSets[_countof(m_aRendererItemCtx)];
  VkDescriptorSet m_pDiffuseDiscriptorSet;

  SimpleOrbitCamera m_Camera;
  POINT m_ptLastMousePos;
};


VulkanApp *CreateAppInstance(HINSTANCE hInstance) {
  return new CubeApp(hInstance);
}

