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

    // Force upload of all resources  on first run
    m_is_init = false;
  }

  void SceneHandlerTask::eval(SchedulerHandle &info) {
    met_trace_full();
    
    const auto &e_scene_handler = info.global("scene_handler").read_only<SceneHandler>();
    const auto &e_scene         = e_scene_handler.scene;
    const auto &e_settings      = e_scene.settings;
    const auto &e_images        = e_scene.resources.images;
    const auto &e_meshes        = e_scene.resources.meshes;
    const auto &e_objects       = e_scene.components.objects;
    const auto &e_upliftings    = e_scene.components.upliftings;

    // Process updates to gpu-side mesh resources 
    if (!m_is_init || e_meshes.is_mutated()) {
      auto &i_mesh_data = info("mesh_data").writeable<detail::RTMeshData>();
      i_mesh_data = detail::RTMeshData::realize(e_meshes);
    }
    
    // Process updates to gpu-side image resources
    if (!m_is_init || e_images.is_mutated() || e_settings.state.texture_size) {
      auto &i_txtr_data = info("txtr_data").writeable<detail::RTTextureData>();
      i_txtr_data = detail::RTTextureData::realize(e_settings.value.texture_size, e_images);
    }

    // Process updates to gpu-side object components
    if (!m_is_init || e_objects.is_mutated()) {
      auto &i_objc_data = info("objc_data").writeable<detail::RTObjectData>();
      if (!m_is_init) i_objc_data = detail::RTObjectData::realize(e_objects);
      else            i_objc_data.update(e_objects);
    }

    // Process updates to gpu-side uplifting components
    if (!m_is_init || e_objects.is_mutated() || e_upliftings.is_mutated()) {
      fmt::print("Rebuilding uplifting\n");
      auto &i_uplf_data = info("uplf_data").writeable<detail::RTUpliftingData>();
      if (!m_is_init) i_uplf_data = detail::RTUpliftingData::realize(e_scene);
      else            i_uplf_data.update(e_scene);
    }

    /* // Process updates to gpu-side illuminant resources
    if (!m_is_init || e_scene.resources.illuminants.is_mutated()) {
      // ...
    }
    
    // Process updates to gpu-side observer resources
    if (!m_is_init || e_scene.resources.observers.is_mutated()) {
      // ...
    }
    
    // Process updates to gpu-side basis function resources
    if (!m_is_init || e_scene.resources.bases.is_mutated()) {
      // ...
    } */

    // Scene resources are now up-to-date;
    // do some bookkeeping on state tracking across resources/components
    {
      auto &e_scene_handler = info.global("scene_handler").writeable<SceneHandler>();
      auto &e_scene         = e_scene_handler.scene;

      e_scene.settings.state.update(e_scene.settings.value);
      e_scene.observer_i.state.update(e_scene.observer_i.value);

      e_scene.resources.meshes.set_mutated(false);
      e_scene.resources.images.set_mutated(false);
      e_scene.resources.illuminants.set_mutated(false);
      e_scene.resources.observers.set_mutated(false);
      e_scene.resources.bases.set_mutated(false);

      e_scene.components.colr_systems.test_mutated();
      e_scene.components.emitters.test_mutated();
      e_scene.components.objects.test_mutated();
      e_scene.components.upliftings.test_mutated();
    }

    m_is_init = true;
  }
} // namespace met