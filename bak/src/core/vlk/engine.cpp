#include <array>
#include <cmath>
#include <fstream>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#include <metameric/core/define.h>
#include <metameric/core/exception.h>
#include <metameric/core/vlk/engine.h>
#include <metameric/core/vlk/initializers.h>
#include <metameric/core/vlk/detail/exception.h>

using namespace metameric::vlk;

GLFWwindow * create_glfw_window(const char *window_title) {
  runtime_assert(glfwInit(), "glfwInit() failed");

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, 0);
  glfwWindowHint(GLFW_VISIBLE, 1);
  glfwWindowHint(GLFW_DECORATED, 1);
  glfwWindowHint(GLFW_FOCUSED, 1);

  GLFWwindow *ptr = glfwCreateWindow(1280, 800, window_title, nullptr, nullptr);
  runtime_assert(ptr, "glfwCreateWindow(...) failed");
  return ptr;
}

void destroy_glfw_window(GLFWwindow *ptr) {
  glfwDestroyWindow(ptr);
  glfwTerminate();
}

vk::ShaderModule load_shader_module(const vk::Device &device, const std::string &file_path) {
  // Attempt to open corresponding file
  std::ifstream ifs(file_path, std::ios::ate | std::ios::binary);
  runtime_assert(ifs.is_open(), "Could not open shader file: " + file_path);

  // Read file into a buffer vector
  size_t file_size = static_cast<size_t>(ifs.tellg());
  std::vector<uint> buffer(file_size / sizeof(uint));
  ifs.seekg(0);
  ifs.read((char *) buffer.data(), file_size);
  ifs.close();
  
  vk::ShaderModule module = device.createShaderModule({{}, buffer});
  runtime_assert(module, "Could not create shader module: " + file_path);
  return module;
}

void Engine::init() {
  guard(!_is_init);

  _window_ptr = create_glfw_window("Metameric");

  init_vk();
  init_swapchain();
  init_commands();
  init_default_renderpass();
  init_framebuffers();
  init_sync_structures();
  init_pipelines();

  _is_init = true;
}

void Engine::dstr() {
  guard(_is_init);
  
  dstr_pipelines();
  dstr_sync_structures();
  dstr_framebuffers();
  dstr_default_renderpass();
  dstr_commands();
  dstr_swapchain();
  dstr_vk();
  
  destroy_glfw_window(_window_ptr);

  _is_init = false;
}

void Engine::draw() {
  // Start of loop; wait for GPU to finish rendering previous frame
  vlk_assert(_device.waitForFences({_render_fence}, true, 1000000000), 
             "vk::Device::waitForFences(...) failed");
  _device.resetFences({_render_fence});

  // Request image (index) from the swapchain
  // Note how this SIGNALS _present_semaphore when done
  // In the meantime, we can start setting up our command buffer!
  uint swapchain_image_idx = _device.acquireNextImageKHR(_swapchain, 1000000000, _present_semaphore);

  // Begin command buffer recording. Reset it before beginning recording
  // Note, ::eOneTimeSubmit implies this buffer is submitted only once,
  // allowing for driver optimization. We're re-recording this thing every frame, for now.
  _main_command_buffer.reset();
  _main_command_buffer.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  // Define a clearcolor value that shows the frame number
  // float color_value = std::abs(std::sin(_frame_number / 120.f ));
  std::array<float, 4> color_value = {
    0.f, 0.f, std::abs(std::sin(_frame_number / 120.f )), 1.f
  };
  vk::ClearValue clear_value = { color_value };
  
  // Begin the default render pass
  _main_command_buffer.beginRenderPass({
    _default_renderpass,
    _framebuffers[swapchain_image_idx],
    { { 0, 0 }, _window_extent }, // render offset, render extent
    { clear_value }
  }, vk::SubpassContents::eInline);

  //  Actual render stuffs comes later :(
  // { ... }

  // Finalize render pass
  _main_command_buffer.endRenderPass();

  // Finalize command buffer
  _main_command_buffer.end();

  // Submit command buffer to graphics queue
  // but wait on _present_semaphore and signal to _render_semaphore
  vk::PipelineStageFlags flag = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  _graphics_queue.submit({{
    { _present_semaphore },
    { flag },
    { _main_command_buffer },
    { _render_semaphore }
  }}, _render_fence);

  // Present rendered image
  vlk_assert(_graphics_queue.presentKHR({
    { _render_semaphore },
    { _swapchain },
    { swapchain_image_idx }
  }), "vk::Queue::presentKHR(...) failed");

  // Increment frame count
  _frame_number++;
}

void Engine::run() {
  while (!glfwWindowShouldClose(_window_ptr)) {
    glfwPollEvents();
    draw();
    glfwSwapBuffers(_window_ptr);
  }
} 

void Engine::init_vk() {
  // Specify a vulkan instance
  vkb::Instance vkb_inst = vkb::InstanceBuilder()
    .set_app_name("Metameric")
		.request_validation_layers(true)
		.require_api_version(1, 1, 0)
		.use_default_debug_messenger()
		.build().value();

  _instance = vkb_inst.instance;
  _debug_messenger = vkb_inst.debug_messenger;

  // Create vulkan surface by punching GLFW; does not like vulkan-hpp bindings, so use ptr
  VkSurfaceKHR temp_surface;
  vlk_assert(vk::Result(glfwCreateWindowSurface(_instance, _window_ptr, nullptr, &temp_surface)), 
             "glfwCreateWindowSurface(...) failed");

  // Specify a physical device
  vkb::PhysicalDevice vkb_physical_device = vkb::PhysicalDeviceSelector(vkb_inst)
		.set_minimum_version(1, 1)
		.set_surface(temp_surface)
		.select().value();

  // Map to actual vkDevice
  vkb::Device vkb_device = vkb::DeviceBuilder(vkb_physical_device)
    .build().value();

  _surface =  temp_surface;
  _device = vkb_device.device;
  _physical_device = vkb_physical_device.physical_device;

  _graphics_queue = vkb_device.get_queue(vkb::QueueType::graphics).value();
  _graphics_queue_family = vkb_device.get_queue_index(vkb::QueueType::graphics).value();
}

void Engine::dstr_vk() {
  _device.destroy();
  vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
  _instance.destroySurfaceKHR(_surface);
  _instance.destroy();
}

void Engine::init_swapchain() {
  vkb::Swapchain swapchain = vkb::SwapchainBuilder(_physical_device, _device, _surface)
		.use_default_format_selection()
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR) // use vsync present mode
		.set_desired_extent(_window_extent.width, _window_extent.height)
		.build().value();

  _swapchain = swapchain.swapchain;
  _swapchain_image_format = static_cast<vk::Format>(swapchain.image_format);

  auto swapchain_images = swapchain.get_images().value();
  auto swapchain_image_views = swapchain.get_image_views().value();

  for (auto &i : swapchain_images) {
    _swapchain_images.push_back(i);
  }
  for (auto &i : swapchain_image_views) {
    _swapchain_image_views.push_back(i);
  }
}

void Engine::dstr_swapchain() {
  for (auto &image_view : _swapchain_image_views) {
    _device.destroyImageView(image_view);
  }
  _device.destroySwapchainKHR(_swapchain);
}

void Engine::init_commands() {
  _command_pool = _device.createCommandPool({
    vk::CommandPoolCreateFlagBits::eResetCommandBuffer, 
    _graphics_queue_family});

  _main_command_buffer = _device.allocateCommandBuffers({
    _command_pool, vk::CommandBufferLevel::ePrimary, 1 })[0]; // only one alloc.
}

void Engine::dstr_commands() {
  _device.destroyCommandPool(_command_pool);
}

void Engine::init_default_renderpass() {
  // Create main color attachment description
  auto color_attachment = vk::AttachmentDescription({}, 
    _swapchain_image_format,
    // 1 bit, no msaa
    vk::SampleCountFlagBits::e1,
    // load/store ops
    vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
    // stencil load/store ops
    vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
    // starting layout is a don't care
    vk::ImageLayout::eUndefined,
    // final layout should be for presenting on display
    vk::ImageLayout::ePresentSrcKHR);

  // Create reference to color attachmetn fro subpass
  auto color_attachment_ref = vk::AttachmentReference(
    0,
    vk::ImageLayout::eColorAttachmentOptimal);

  // Create subpass description
  auto subpass_description = vk::SubpassDescription({}, 
    vk::PipelineBindPoint::eGraphics)
    .setColorAttachmentCount(1)
    .setPColorAttachments(&color_attachment_ref);

  // Create render pass
  _default_renderpass = _device.createRenderPass({{},
    { color_attachment },
    { subpass_description }});
}

void Engine::dstr_default_renderpass() {
  _device.destroyRenderPass(_default_renderpass);
}

void Engine::init_framebuffers() {
  // Create one framebuffer for each image on the swapchain
  _framebuffers.reserve(_swapchain_image_views.size());
  for (const auto &image_view : _swapchain_image_views) {
    _framebuffers.push_back(_device.createFramebuffer({{},
      _default_renderpass,
      1,
      &image_view,
      _window_extent.width,
      _window_extent.height,
      1}));
  }
}

void Engine::dstr_framebuffers() {
  for (auto &framebuffer : _framebuffers) {
    _device.destroyFramebuffer(framebuffer);
  }
}

void Engine::init_sync_structures() {
  _render_fence = _device.createFence({vk::FenceCreateFlagBits::eSignaled});
  _render_semaphore = _device.createSemaphore({});
  _present_semaphore = _device.createSemaphore({});
}

void Engine::dstr_sync_structures() {
  _device.destroySemaphore(_present_semaphore);
  _device.destroySemaphore(_render_semaphore);
  _device.destroyFence(_render_fence);
}

void Engine::init_pipelines() {
  auto triangle_vert = load_shader_module(_device, "../resources/shaders/triangle.vert.spv");
  auto triangle_vfrag = load_shader_module(_device, "../resources/shaders/triangle.vert.spv");

  // ...
}

void Engine::dstr_pipelines() {
  // ...
}