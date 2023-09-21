#include <metameric/components/pipeline_new/task_gen_object_data.hpp>

namespace met {
  GenObjectDataTask:: GenObjectDataTask(uint object_i)
  : object_i(object_i) { }

  bool GenObjectDataTask::is_active(SchedulerHandle &info) {
    met_trace();
    
    const auto &e_scene   = info.global("scene").read_only<Scene>();
    const auto &e_objects = e_scene.components.objects;
    
    // TODO show dependence on object materials, and uplifting tesselation
    return e_objects[object_i].state;
  }

  void GenObjectDataTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Allocate space for resources, if necessary
  }

  void GenObjectDataTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_scene     = info.global("scene").read_only<Scene>();
    const auto &e_object    = e_scene.components.objects[object_i];
    const auto &e_uplifting = e_scene.components.upliftings[e_object.value.uplifting_i];
    
    // 1. Generate tesselation coordinates
    {
      
    }
  }
} // namespace met