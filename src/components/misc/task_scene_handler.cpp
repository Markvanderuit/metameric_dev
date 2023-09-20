#include <metameric/components/misc/task_scene_handler.hpp>
#include <algorithm>
#include <deque>

namespace met {
  void SceneHandlerTask::init(SchedulerHandle &info) {
    met_trace();
    
    // Initialize empty holder objects for gpu-side resources
    info("objc_data").set<detail::RTObjectData>({ });
    info("mesh_data").set<detail::RTMeshData>({ });
    info("txtr_data").set<detail::RTTextureData>({ });
    info("uplf_data").set<detail::RTUpliftingData>({ });

    // Run initial state test
    {
      auto &e_scene_handler = info.global("scene_handler").writeable<SceneHandler>();
      auto &e_scene         = e_scene_handler.scene;

      e_scene.settings.state.update(e_scene.settings.value);
      e_scene.observer_i.state.update(e_scene.observer_i.value);

      e_scene.components.colr_systems.test_mutated();
      e_scene.components.emitters.test_mutated();
      e_scene.components.objects.test_mutated();
      e_scene.components.upliftings.test_mutated();
    }

    // Force upload of all resources  on first run
    m_is_init = false;
  }

  void SceneHandlerTask::eval(SchedulerHandle &info) {
    met_trace_full();
    
    // Get external resources
    const auto &e_scene_handler = info.global("scene_handler").read_only<SceneHandler>();
    const auto &e_scene         = e_scene_handler.scene;
    const auto &e_settings      = e_scene.settings;
    const auto &e_images        = e_scene.resources.images;
    const auto &e_meshes        = e_scene.resources.meshes;
    const auto &e_objects       = e_scene.components.objects;
    const auto &e_upliftings    = e_scene.components.upliftings;

    { // Pre-load bookkeeping on components; test for changes from last iteration
      auto &e_scene_handler = info.global("scene_handler").writeable<SceneHandler>();
      auto &e_scene         = e_scene_handler.scene;

      e_scene.settings.state.update(e_scene.settings.value);
      e_scene.observer_i.state.update(e_scene.observer_i.value);

      e_scene.components.colr_systems.test_mutated();
      e_scene.components.emitters.test_mutated();
      e_scene.components.objects.test_mutated();
      e_scene.components.upliftings.test_mutated();
    }

    // Get uninitialized handles to holder objects
    // we avoid accessing these as writeable unless absolutely necessary
    auto i_mesh_data = info("mesh_data");
    auto i_txtr_data = info("txtr_data");
    auto i_uplf_data = info("uplf_data");
    auto i_objc_data = info("objc_data");

    // Process updates to gpu-side mesh resources 
    if (!m_is_init || e_meshes.is_mutated()) {
      auto &i_mesh_data = info("mesh_data").writeable<detail::RTMeshData>();
      i_mesh_data = detail::RTMeshData::realize(e_meshes);
    }
    
    // Process updates to gpu-side image resources
    if (!m_is_init) {
      i_txtr_data.writeable<detail::RTTextureData>() = detail::RTTextureData::realize(e_scene);
    } else if (i_txtr_data.read_only<detail::RTTextureData>().is_stale(e_scene)) {
      i_txtr_data.writeable<detail::RTTextureData>().update(e_scene);
    }

    // Process updates to gpu-side object components
    if (!m_is_init) {
      i_objc_data.writeable<detail::RTObjectData>() = detail::RTObjectData::realize(e_scene);
    } else if (i_objc_data.read_only<detail::RTObjectData>().is_stale(e_scene)) {
      i_objc_data.writeable<detail::RTObjectData>().update(e_scene);
    }

    // Process updates to gpu-side uplifting resources
    if (!m_is_init) {
      i_uplf_data.writeable<detail::RTUpliftingData>() = detail::RTUpliftingData::realize(e_scene);
    } else if (i_uplf_data.read_only<detail::RTUpliftingData>().is_stale(e_scene)) {
      i_uplf_data.writeable<detail::RTUpliftingData>().update(e_scene);
    }

    { // Post-load bookkeeping on resources; assume no further changes
      auto &e_scene_handler = info.global("scene_handler").writeable<SceneHandler>();
      auto &e_scene         = e_scene_handler.scene;

      e_scene.resources.meshes.set_mutated(false);
      e_scene.resources.images.set_mutated(false);
      e_scene.resources.illuminants.set_mutated(false);
      e_scene.resources.observers.set_mutated(false);
      e_scene.resources.bases.set_mutated(false);
    }

    m_is_init = true;
  }
} // namespace met