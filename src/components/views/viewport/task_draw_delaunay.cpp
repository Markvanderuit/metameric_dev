#include <metameric/core/data.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/viewport/task_draw_delaunay.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/utility.hpp>

namespace met {
  ViewportDrawDelaunayTask::ViewportDrawDelaunayTask(const std::string &name)
  : detail::AbstractTask(name, true) { }

  void ViewportDrawDelaunayTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

  }

  void ViewportDrawDelaunayTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();
    
  }
} // namespace met