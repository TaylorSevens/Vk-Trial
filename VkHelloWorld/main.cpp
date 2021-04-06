#include <VulkanRenderContext.hpp>

extern VulkanRenderContext *CreateSampleRenderContext();
extern int RunSample(const char *pTitle, int width, int height, VulkanRenderContext *pRenderContext);

int main() {
  auto pRenderContext = CreateSampleRenderContext();
  int rc = RunSample("TestTriangle", 800, 600, pRenderContext);
  SAFE_DELETE(pRenderContext);
  return rc;
}