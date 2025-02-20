#pragma once

#include <etna/Sampler.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/Window.hpp>
#include <etna/PerFrameCmdMgr.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <etna/Image.hpp>

#include "wsi/OsWindowingManager.hpp"


class App
{
public:
  App();
  ~App();

  void run();

private:
  void drawFrame();
  void renderScene(vk::CommandBuffer& cmdBuffer, vk::Image backbuffer, vk::ImageView backbufferView);
  void renderTexture(vk::CommandBuffer& cmdBuffer);

private:
  OsWindowingManager windowing;
  std::unique_ptr<OsWindow> osWindow;

  glm::uvec2 resolution;
  bool useVsync;

  etna::Image fileImage;
  etna::Image texImage;
  etna::Sampler sampler;

  etna::GraphicsPipeline fvPipeline;
  etna::GraphicsPipeline texPipeline;

  std::unique_ptr<etna::Window> vkWindow;
  std::unique_ptr<etna::PerFrameCmdMgr> commandManager;

  long long startTime;
};
