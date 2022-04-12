#pragma once

#include <vector>
#include <metameric/core/vlk/types.h>

namespace metameric::vlk {
  class Engine {
  public:
    // Setup/teardown functions
    void init();
    void dstr();

    // Draw loop
    void draw();

    // Run main loop
    void run();

  private:
    // Internal setup functions
    void init_vk();
    void init_swapchain();
    void init_commands();
    void init_default_renderpass();
    void init_framebuffers();
    void init_sync_structures();
    void init_pipelines();

    // Internal teardown functions
    void dstr_pipelines();
    void dstr_sync_structures();
    void dstr_vk();
    void dstr_swapchain();
    void dstr_commands();
    void dstr_default_renderpass();
    void dstr_framebuffers();

    // Miscellaneous components
    bool _is_init { false };
    uint _frame_number { 0 };
    struct GLFWwindow * _window_ptr { nullptr }; // fwd
    vk::Extent2D _window_extent { 1280, 800 };

    // Components initialized by init_vk()
    vk::Instance _instance;
    vk::PhysicalDevice _physical_device;
    vk::Device _device;
    vk::SurfaceKHR _surface;
    vk::DebugUtilsMessengerEXT _debug_messenger;
    vk::Queue _graphics_queue;
    uint _graphics_queue_family;

    // Components initialized by init_swapchain()
    vk::SwapchainKHR _swapchain;
    vk::Format _swapchain_image_format;
    std::vector<vk::Image> _swapchain_images;
    std::vector<vk::ImageView> _swapchain_image_views;

    // Components initialized by init_commands()
    vk::CommandPool _command_pool;
    vk::CommandBuffer _main_command_buffer;

    // Components initialized by init_default_renderpass()
    vk::RenderPass _default_renderpass;

    // Components initialized by init_framebuffers()
    std::vector<vk::Framebuffer> _framebuffers;

    // Components initialized by init_sync_structures()
    vk::Semaphore _present_semaphore, _render_semaphore;
    vk::Fence _render_fence;

    // Components initialized by init_pipelines()
    vk::Pipeline _pipeline;
  };
} // namespace metameric::vlk