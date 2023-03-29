#include <metameric/core/data.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/components/pipeline/task_gen_generalized_weights.hpp>
#include <small_gl/utility.hpp>
#include <algorithm>
#include <execution>
#include <ranges>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;
  constexpr uint buffer_init_size    = 1024u;

  void GenGeneralizedWeightsTask::init(SchedulerHandle &info) {
    met_trace_full();

  }

  bool GenGeneralizedWeightsTask::is_active(SchedulerHandle &info) {
    met_trace_full();
    return info("state", "pipeline_state").read_only<ProjectState>().any_verts;
  }

  void GenGeneralizedWeightsTask::eval(SchedulerHandle &info) {
    met_trace_full();
    
  }
} // namespace met