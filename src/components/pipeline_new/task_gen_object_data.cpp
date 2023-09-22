#include <metameric/core/scene.hpp>
#include <metameric/components/pipeline_new/task_gen_object_data.hpp>
#include <metameric/components/misc/detail/scene.hpp>
#include <format>

namespace met {
  GenObjectDataTask:: GenObjectDataTask(uint object_i)
  : m_object_i(object_i) { }

  bool GenObjectDataTask::is_active(SchedulerHandle &info) {
    met_trace();
    
    const auto &e_scene     = info.global("scene").getr<Scene>();
    const auto &e_object    = e_scene.components.objects[m_object_i];
    const auto &e_uplifting = e_scene.components.upliftings[e_object.value.uplifting_i];
    
    // TODO show fine-grained dependence on object materials and tesselation
    return e_object.state ||  e_uplifting.state            ||
           info("scene_handler", "mesh_data").is_mutated() ||
           info("scene_handler", "txtr_data").is_mutated() ||
           info("scene_handler", "uplf_data").is_mutated();
  }

  void GenObjectDataTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Allocate space for resources, if necessary
  }

  void GenObjectDataTask::eval(SchedulerHandle &info) {
    met_trace_full();
    
    // Get external resources
    const auto &e_scene       = info.global("scene").getr<Scene>();
    const auto &e_object      = e_scene.components.objects[m_object_i];
    const auto &e_uplifting   = e_scene.components.upliftings[e_object.value.uplifting_i];
    auto uplifting_task_name  = std::format("gen_upliftings.gen_uplifting_{}", e_object.value.uplifting_i);
    const auto &e_txtr_data   = info("scene_handler", "txtr_data").getr<detail::RTTextureData>();
    const auto &e_uplf_data   = info("scene_handler", "uplf_data").getr<detail::RTUpliftingData>();
    const auto &e_tesselation = info(uplifting_task_name, "tesselation").getr<AlDelaunay>();

    // 1. Generate tesselation coordinates
    {
      // ...
    }

    // 2. Consolidate and upload data to GL-side in one nice place
    {
      // ...
    }
  }
} // namespace met