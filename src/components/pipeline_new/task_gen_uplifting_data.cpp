#include <metameric/components/pipeline_new/task_gen_uplifting_data.hpp>

namespace met {
  namespace detail {
    void generate_csys_boundary() {
      // ...
    }

    void generate_constraint_spectrum() {
      // ...
    }

    void generate_tesselation() {
      // ...
    }
  } // namespace detail

  GenUpliftingDataTask:: GenUpliftingDataTask(uint uplifting_i)
  : uplifting_i(uplifting_i) { }

  bool GenUpliftingDataTask::is_active(SchedulerHandle &info) {
    met_trace();

    const auto &e_scene_handler = info.global("scene_handler").read_only<SceneHandler>();
    const auto &e_upliftings    = e_scene_handler.scene.components.upliftings;
    
    return e_upliftings[uplifting_i].state;
  }

  void GenUpliftingDataTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Allocate space for resources, if necessary
  }

  void GenUpliftingDataTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_scene_handler = info.global("scene_handler").read_only<SceneHandler>();
    const auto &e_scene         = e_scene_handler.scene;
    const auto &e_uplifting     = e_scene.components.upliftings[uplifting_i];
    const auto &e_csys_i        = e_scene.components.colr_systems[e_uplifting.value.csys_i];
    
    // 1. Generate color system boundary
    {

    }

    // 2. Generate constraint spectra
    {

    }

    // 3. Consolidate and upload boundary, constraint and measured spectra
    {

    }

    // 4. Generate color system tesselation
    {

    }
  }
} // namespace met