#include <metameric/components/misc/task_scene_handler.hpp>
#include <metameric/components/misc/detail/scene.hpp>
#include <algorithm>
#include <deque>

namespace met {
  void SceneHandlerTask::init(SchedulerHandle &info) {
    met_trace();

    // Get external resources
    const auto &e_scene = info.global("scene").getr<Scene>();
    
    // Initialize holder objects for gpu-side resources for newly loaded scene
    info("txtr_data").init<detail::RTTextureData>(e_scene);
    info("mesh_data").init<detail::RTMeshData>(e_scene);
    info("uplf_data").init<detail::RTUpliftingData>(e_scene);
    info("objc_data").init<detail::RTObjectData>(e_scene);
    info("cmfs_data").init<detail::RTObserverData>(e_scene);
    info("illm_data").init<detail::RTIlluminantData>(e_scene);
    info("csys_data").init<detail::RTColorSystemData>(e_scene);
    
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

    if (e_scene.components.objects.is_mutated()) {
      fmt::print("Objects updated\n");
    }

    // Process updates to gpu-side illuminant components
    if (auto handle = info("illm_data"); handle.getr<detail::RTIlluminantData>().is_stale(e_scene))
      handle.getw<detail::RTIlluminantData>().update(e_scene);

    // Process updates to gpu-side observer components
    if (auto handle = info("cmfs_data"); handle.getr<detail::RTObserverData>().is_stale(e_scene))
      handle.getw<detail::RTObserverData>().update(e_scene);

    // Process updates to gpu-side object components
    if (auto handle = info("csys_data"); handle.getr<detail::RTColorSystemData>().is_stale(e_scene))
      handle.getw<detail::RTColorSystemData>().update(e_scene);
      
    // Process updates to gpu-side image resources
    if (auto handle = info("txtr_data"); handle.getr<detail::RTTextureData>().is_stale(e_scene))
      handle.getw<detail::RTTextureData>().update(e_scene);

    // Process updates to gpu-side mesh resources
    if (auto handle = info("mesh_data"); handle.getr<detail::RTMeshData>().is_stale(e_scene))
      handle.getw<detail::RTMeshData>().update(e_scene);

    // Process updates to gpu-side uplifting resources
    if (auto handle = info("uplf_data"); handle.getr<detail::RTUpliftingData>().is_stale(e_scene))
      handle.getw<detail::RTUpliftingData>().update(e_scene);

    // Process updates to gpu-side object components
    if (auto handle = info("objc_data"); handle.getr<detail::RTObjectData>().is_stale(e_scene))
      handle.getw<detail::RTObjectData>().update(e_scene);

    { // Post-load bookkeeping on resources; assume no further changes as gpu-side
      // All resources should be up-to-date now
      auto &e_scene = info.global("scene").getw<Scene>();
      e_scene.resources.meshes.set_mutated(false);
      e_scene.resources.images.set_mutated(false);
      e_scene.resources.illuminants.set_mutated(false);
      e_scene.resources.observers.set_mutated(false);
      e_scene.resources.bases.set_mutated(false);
    }
  }
} // namespace met