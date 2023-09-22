#include <metameric/components/misc/task_scene_handler.hpp>
#include <metameric/components/misc/detail/scene.hpp>
#include <algorithm>
#include <deque>

namespace met {
  void SceneHandlerTask::init(SchedulerHandle &info) {
    met_trace();

    // Get external resources
    const auto &e_scene = info.global("scene").read_only<Scene>();
    
    // Initialize empty holder objects for gpu-side resources
    info("objc_data").init<detail::RTObjectData>(e_scene);
    info("mesh_data").init<detail::RTMeshData>(e_scene);
    info("txtr_data").init<detail::RTTextureData>(e_scene);
    info("uplf_data").init<detail::RTUpliftingData>(e_scene);

    { // Run initial state test
      auto &e_scene = info.global("scene").writeable<Scene>();

      e_scene.components.settings.state.update(e_scene.components.settings.value);
      e_scene.components.observer_i.state.update(e_scene.components.observer_i.value);
      e_scene.components.colr_systems.update();
      e_scene.components.emitters.update();
      e_scene.components.objects.update();
      e_scene.components.upliftings.update();
    }

    // Force upload of all resources  on first run
    m_is_init = false;
  }

  void SceneHandlerTask::eval(SchedulerHandle &info) {
    met_trace_full();
    
    // Get external resources
    const auto &e_scene = info.global("scene").read_only<Scene>();

    { // Pre-load bookkeeping on components; test for changes from last iteration
      auto &e_scene = info.global("scene").writeable<Scene>();

      e_scene.components.settings.state.update(e_scene.components.settings.value);
      e_scene.components.observer_i.state.update(e_scene.components.observer_i.value);
      e_scene.components.colr_systems.update();
      e_scene.components.emitters.update();
      e_scene.components.objects.update();
      e_scene.components.upliftings.update();
    }

    // Process updates to gpu-side mesh resources
    if (auto handle = info("mesh_data"); handle.read_only<detail::RTMeshData>().is_stale(e_scene)) {
      handle.writeable<detail::RTMeshData>().update(e_scene);
    }
    
    // Process updates to gpu-side image resources
    if (auto handle = info("txtr_data"); handle.read_only<detail::RTTextureData>().is_stale(e_scene)) {
      handle.writeable<detail::RTTextureData>().update(e_scene);
    }

    // Process updates to gpu-side object components
    if (auto handle = info("objc_data"); handle.read_only<detail::RTObjectData>().is_stale(e_scene)) {
      handle.writeable<detail::RTObjectData>().update(e_scene);
    }

    // Process updates to gpu-side uplifting resources
    if (auto handle = info("uplf_data"); handle.read_only<detail::RTUpliftingData>().is_stale(e_scene)) {
      handle.writeable<detail::RTUpliftingData>().update(e_scene);
    }

    { // Post-load bookkeeping on resources; assume no further changes
      auto &e_scene = info.global("scene").writeable<Scene>();
      
      e_scene.resources.meshes.set_mutated(false);
      e_scene.resources.images.set_mutated(false);
      e_scene.resources.illuminants.set_mutated(false);
      e_scene.resources.observers.set_mutated(false);
      e_scene.resources.bases.set_mutated(false);
    }

    m_is_init = true;
  }
} // namespace met