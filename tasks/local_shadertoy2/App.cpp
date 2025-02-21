#include "App.hpp"

#include <etna/Etna.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>

#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>

#include <etna/BlockingTransferHelper.hpp>


App::App()
  : resolution{1280, 720}
  , useVsync{true}
{
  startTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count();

  {
    auto glfwInstExts = windowing.getRequiredVulkanInstanceExtensions();

    std::vector<const char*> instanceExtensions{glfwInstExts.begin(), glfwInstExts.end()};

    std::vector<const char*> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    etna::initialize(etna::InitParams{
      .applicationName = "Local Shadertoy",
      .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
      .instanceExtensions = instanceExtensions,
      .deviceExtensions = deviceExtensions,
      .physicalDeviceIndexOverride = {},
      .numFramesInFlight = 1,
    });
  }

  osWindow = windowing.createWindow(OsWindow::CreateInfo{
    .resolution = resolution,
  });

  {
    auto surface = osWindow->createVkSurface(etna::get_context().getInstance());

    vkWindow = etna::get_context().createWindow(etna::Window::CreateInfo{
      .surface = std::move(surface),
    });

    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });

    resolution = {w, h};
  }

  commandManager = etna::get_context().createPerFrameCmdMgr();

  {
    etna::create_program("shadertoy2", {LOCAL_SHADERTOY2_SHADERS_ROOT "rect.vert.spv",
                                        LOCAL_SHADERTOY2_SHADERS_ROOT "toy.frag.spv" });

    etna::create_program("tex", {LOCAL_SHADERTOY2_SHADERS_ROOT "rect.vert.spv",
                                 LOCAL_SHADERTOY2_SHADERS_ROOT "tex.frag.spv" });

    fvPipeline = etna::get_context().getPipelineManager().createGraphicsPipeline("shadertoy2",
      etna::GraphicsPipeline::CreateInfo {
        .fragmentShaderOutput = {
          .colorAttachmentFormats = {vk::Format::eB8G8R8A8Srgb}
        }
      });

    texPipeline = etna::get_context().getPipelineManager().createGraphicsPipeline("tex",
      etna::GraphicsPipeline::CreateInfo {
        .fragmentShaderOutput = {
          .colorAttachmentFormats = {vk::Format::eB8G8R8A8Srgb}
        }
      });

    texImage = etna::get_context().createImage(etna::Image::CreateInfo{
      .extent = vk::Extent3D{100, 100, 1},
      .name = "tex",
      .format = vk::Format::eB8G8R8A8Srgb,
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
    });

    sampler = etna::Sampler(etna::Sampler::CreateInfo
      {
        .filter = vk::Filter::eLinear,
        .addressMode = vk::SamplerAddressMode::eRepeat,
        .name = "sampler"
      });
  }

  {
    int x, y, n;
    unsigned char *picData = stbi_load(TEXTURES_ROOT "texture1.bmp", &x, &y, &n, 4);

    etna::Image::CreateInfo fileTextureInfo{
      .extent = vk::Extent3D{(uint32_t)x, (uint32_t)y, 1},
      .name = "fileTex",
      .format = vk::Format::eR8G8B8A8Srgb,
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
    };

    fileImage = etna::create_image_from_bytes(fileTextureInfo, etna::get_context().createOneShotCmdMgr()->start(), picData);

    stbi_image_free(picData);
  }
}

App::~App()
{
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::run()
{
  while (!osWindow->isBeingClosed())
  {
    windowing.poll();

    drawFrame();
  }

  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::renderTexture(vk::CommandBuffer& cmdBuffer)
{
  etna::set_state(
    cmdBuffer,
    texImage.get(),
    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    vk::AccessFlagBits2::eColorAttachmentWrite,
    vk::ImageLayout::eColorAttachmentOptimal,
    vk::ImageAspectFlagBits::eColor);
  etna::flush_barriers(cmdBuffer);

  {
    etna::RenderTargetState state{cmdBuffer, {{}, {100, 100}}, {{texImage.get(), texImage.getView({})}}, {}};

    cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, texPipeline.getVkPipeline());
    cmdBuffer.draw(6, 1, 0, 0);
  }
  
  float time = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count() - startTime) / 1000.0f;
  cmdBuffer.pushConstants(
      fvPipeline.getVkPipelineLayout(), vk::ShaderStageFlagBits::eFragment,
      0, sizeof(time), &time);
}

void App::renderScene(vk::CommandBuffer& cmdBuffer, vk::Image backbuffer, vk::ImageView backbufferView)
{
  etna::set_state(
    cmdBuffer,
    backbuffer,
    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    vk::AccessFlagBits2::eColorAttachmentWrite,
    vk::ImageLayout::eColorAttachmentOptimal,
    vk::ImageAspectFlagBits::eColor);

  etna::set_state(
    cmdBuffer,
    texImage.get(),
    vk::PipelineStageFlagBits2::eFragmentShader,
    vk::AccessFlagBits2::eColorAttachmentRead,
    vk::ImageLayout::eShaderReadOnlyOptimal,
    vk::ImageAspectFlagBits::eColor);

  etna::set_state(
    cmdBuffer,
    fileImage.get(),
    vk::PipelineStageFlagBits2::eFragmentShader,
    vk::AccessFlagBits2::eColorAttachmentRead,
    vk::ImageLayout::eShaderReadOnlyOptimal,
    vk::ImageAspectFlagBits::eColor);
  etna::flush_barriers(cmdBuffer);

  {
    etna::RenderTargetState state{cmdBuffer, {{}, {resolution.x, resolution.y}}, {{backbuffer, backbufferView}}, {}};

    auto fragVertInfo = etna::get_shader_program("shadertoy2");
    auto set = etna::create_descriptor_set(fragVertInfo.getDescriptorLayoutId(0), cmdBuffer,
      {
        etna::Binding{0, fileImage.genBinding(sampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{1, texImage.genBinding(sampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}
      });
    vk::DescriptorSet vkSet = set.getVkSet();

    cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, fvPipeline.getVkPipeline());
    cmdBuffer.bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics, fvPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, nullptr);

    cmdBuffer.draw(6, 1, 0, 0);
  }

  etna::set_state(
    cmdBuffer,
    backbuffer,
    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    {},
    vk::ImageLayout::ePresentSrcKHR,
    vk::ImageAspectFlagBits::eColor);
  etna::flush_barriers(cmdBuffer);
}

void App::drawFrame()
{
  auto currentCmdBuf = commandManager->acquireNext();
  etna::begin_frame();
  auto nextSwapchainImage = vkWindow->acquireNext();

  if (nextSwapchainImage)
  {
    auto [backbuffer, backbufferView, backbufferAvailableSem] = *nextSwapchainImage;

    ETNA_CHECK_VK_RESULT(currentCmdBuf.begin(vk::CommandBufferBeginInfo{}));
    {
      renderTexture(currentCmdBuf);
      renderScene(currentCmdBuf, backbuffer, backbufferView);
    }
    ETNA_CHECK_VK_RESULT(currentCmdBuf.end());

    auto renderingDone = commandManager->submit(std::move(currentCmdBuf), std::move(backbufferAvailableSem));
    const bool presented = vkWindow->present(std::move(renderingDone), backbufferView);

    if (!presented)
      nextSwapchainImage = std::nullopt;
  }

  etna::end_frame();

  if (!nextSwapchainImage && osWindow->getResolution() != glm::uvec2{0, 0})
  {
    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });
    ETNA_VERIFY((resolution == glm::uvec2{w, h}));
  }
}
