#include <metameric/core/data.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/linprog.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/components/pipeline/task_gen_color_system_solid.hpp>
#include <omp.h>

namespace met {
  void GenColorSystemSolidTaskinit(SchedulerHandle &info) {
    met_trace_full();

  }

  void GenColorSystemSolidTaskeval(SchedulerHandle &info) {
    met_trace_full();

  }

  bool GenColorSystemSolidTaskeval_state(SchedulerHandle &info) {
    met_trace_full();
    return info.resource("state", "pipeline_state").read_only<ProjectState>().csys[0];
  }
} // namespace met