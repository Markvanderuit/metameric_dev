#include <metameric/core/scene.hpp>
#include <metameric/components/views/mesh_viewport/task_draw_overlay.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/dispatch.hpp>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  bool MeshViewportDrawOverlayTask::is_active(SchedulerHandle &info) {
    met_trace();
    return true;
  }

  void MeshViewportDrawOverlayTask::init(SchedulerHandle &info) {
    met_trace_full();
  }
    
  void MeshViewportDrawOverlayTask::eval(SchedulerHandle &info) {
    met_trace_full();
  }
} // namespace met