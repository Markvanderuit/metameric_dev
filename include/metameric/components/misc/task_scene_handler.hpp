#pragma once

#include <metameric/core/scene_handler.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/components/misc/detail/scene.hpp>

namespace met {
  class SceneHandlerTask : public detail::TaskNode {
    bool m_is_init = false;

    void init(SchedulerHandle &info) override {
      // Initialize empty holder objects for gpu-side resources
      info("meshes").set<std::vector<detail::MeshLayout>>({ });
      info("textures").set<std::vector<detail::TextureLayout>>({ });

      // Force upload of all resources  on first run
      m_is_init = false;
    }
    
    void eval(SchedulerHandle &info) override {
      met_trace_full();
      
      const auto &e_scene_handler = info.global("scene_handler").read_only<SceneHandler>();
      const auto &e_scene         = e_scene_handler.scene;

      // Process updates to gpu-side mesh resources 
      if (!m_is_init || e_scene.resources.meshes.is_mutated()) {
        fmt::print("Pushing meshes\n");
        auto &i_meshes = info("meshes").writeable<std::vector<detail::MeshLayout>>();
        i_meshes.resize(e_scene.resources.meshes.size());

        for (uint i = 0; i < i_meshes.size(); ++i) {
          const auto &rsrc = e_scene.resources.meshes[i];
          guard_continue(!m_is_init || rsrc.is_mutated());
          i_meshes[i] = detail::MeshLayout::realize(rsrc.value());
        } // for (uint i)
      }
      
      // Process updates to gpu-side image resources
      if (!m_is_init || e_scene.resources.images.is_mutated()) {
        fmt::print("Pushing images\n");
        auto &i_textures = info("textures").writeable<std::vector<detail::TextureLayout>>();
        i_textures.resize(e_scene.resources.images.size());
        
        for (uint i = 0; i < i_textures.size(); ++i) {
          const auto &rsrc = e_scene.resources.images[i];
          guard_continue(!m_is_init || rsrc.is_mutated());
          i_textures[i] = detail::TextureLayout::realize(rsrc.value());
        } // for (uint i)
      }

      // Process updates to gpu-side illuminant resources
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
      }

      // Scene resources are now up-to-date;
      // do some bookkeeping on state tracking across resources/components
      {
        auto &e_scene_handler = info.global("scene_handler").writeable<SceneHandler>();
        auto &e_scene         = e_scene_handler.scene;

        e_scene.resources.meshes.set_mutated(false);
        e_scene.resources.images.set_mutated(false);
        e_scene.resources.illuminants.set_mutated(false);
        e_scene.resources.observers.set_mutated(false);
        e_scene.resources.bases.set_mutated(false);

        e_scene.components.colr_systems.test_mutated();
        e_scene.components.emitters.test_mutated();
        e_scene.components.materials.test_mutated();
        e_scene.components.objects.test_mutated();
        e_scene.components.upliftings.test_mutated();
      }

      m_is_init = true;
    }
  };
} // namespace met