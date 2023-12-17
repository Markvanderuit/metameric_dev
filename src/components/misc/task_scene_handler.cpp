#include <metameric/components/misc/task_scene_handler.hpp>
#include <metameric/render/scene_data.hpp>
#include <algorithm>
#include <deque>

namespace met {
  void SceneHandlerTask::init(SchedulerHandle &info) {
    met_trace();

    // Get external resources
    const auto &e_scene = info.global("scene").getr<Scene>();
    
    // Initialize holder objects for gpu-side resources for newly loaded scene
    info("txtr_data").init<TextureData>(e_scene);
    info("mesh_data").init<MeshData>(e_scene);
    info("uplf_data").init<UpliftingData>(e_scene);
    info("objc_data").init<ObjectData>(e_scene);
    info("cmfs_data").init<ObserverData>(e_scene);
    info("illm_data").init<IlluminantData>(e_scene);
    info("csys_data").init<ColorSystemData>(e_scene);
    info("bvhs_data").init<BVHData>(e_scene);
    
    { // Pre-load bookkeeping on resources
      auto &e_scene = info.global("scene").getw<Scene>();
      e_scene.resources.meshes.set_mutated(true);
      e_scene.resources.images.set_mutated(true);
      e_scene.resources.illuminants.set_mutated(true);
      e_scene.resources.observers.set_mutated(true);
      e_scene.resources.bases.set_mutated(true);
    }
  }

  void SceneHandlerTask::eval(SchedulerHandle &info) {
    met_trace_full();
    
    // Get external resources
    const auto &e_scene = info.global("scene").getr<Scene>();

    { // Pre-load bookkeeping on components; test for state changes from last iteration
      auto &e_scene = info.global("scene").getw<Scene>();
      e_scene.components.settings.state.update(e_scene.components.settings.value);
      e_scene.components.observer_i.state.update(e_scene.components.observer_i.value);
      e_scene.components.colr_systems.update();
      e_scene.components.emitters.update();
      e_scene.components.upliftings.update();
      e_scene.components.objects.update();
    }

    // Process updates to gpu-side resources, if they are stale
    for (auto key : { "illm_data", "cmfs_data", "csys_data", "txtr_data", "uplf_data", "objc_data", "mesh_data", "bvhs_data" }) {
      auto handle = info(key); 
      if (handle.getr<detail::SceneDataBase>().is_stale(e_scene)) {
        handle.getw<detail::SceneDataBase>().update(e_scene);
      }
    }

    { // Post-load bookkeeping on resources; assume no further changes as gpu-side
      // all the resources should be updatednow
      auto &e_scene = info.global("scene").getw<Scene>();
      e_scene.resources.meshes.set_mutated(false);
      e_scene.resources.images.set_mutated(false);
      e_scene.resources.illuminants.set_mutated(false);
      e_scene.resources.observers.set_mutated(false);
      e_scene.resources.bases.set_mutated(false);
    }
  }
} // namespace met