#include <metameric/core/math.hpp>
#include <metameric/core/linprog.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/pca.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/data.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/pipeline/task_gen_spectral_samples.hpp>
#include <small_gl/buffer.hpp>
#include <algorithm>
#include <execution>
#include <ranges>

namespace met {
  GenSpectralSamplesTask::GenSpectralSamplesTask(const std::string &name)
  : detail::AbstractTask(name) { }
  
  void GenSpectralSamplesTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Submit shared resources 
    info.insert_resource<std::vector<Spec>>("spectra", { });
  }
  
  void GenSpectralSamplesTask::dstr(detail::TaskDstrInfo &info) {
    met_trace_full();
  }
  
  void GenSpectralSamplesTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Continue only on relevant state change
    auto &e_appl_data  = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_pipe_state = info.get_resource<ProjectState>("state", "pipeline_state");
    guard(e_pipe_state.any_samps || e_pipe_state.any_verts);

    
  }
} // namespace met