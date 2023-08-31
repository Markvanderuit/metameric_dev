#pragma once

#include <metameric/core/scene_handler.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/components/misc/detail/scene.hpp>

namespace met {
  struct SceneHandlerTask : public detail::TaskNode {
    bool is_active(SchedulerHandle &info) override {
      return info.global("scene_handler").read_only<SceneHandler>().is_mutated();
    }

    void init(SchedulerHandle &info) override {
      // Initialize empty holder objects
      info("meshes").set<std::vector<detail::MeshLayout>>({ });
      info("textures").set<std::vector<detail::TextureLayout>>({ });
    }
    
    void eval(SchedulerHandle &info) override {
      met_trace_full();
      
      const auto &e_scene_handler = info.global("scene_handler").read_only<SceneHandler>();
      const auto &e_scene         = e_scene_handler.scene;

      // Process updates to mesh resources
      if (e_scene.resources.meshes.is_mutated()) {
        auto &i_meshes = info("meshes").writeable<std::vector<detail::MeshLayout>>();
        i_meshes.resize(e_scene.resources.meshes.size());

        for (uint i = 0; i < i_meshes.size(); ++i) {
          const auto &rsrc = e_scene.resources.meshes[i];
          guard_continue(rsrc.is_mutated());
          i_meshes[i] = detail::MeshLayout::realize(rsrc.value());
        } // for (uint i)
      } // if (meshes)

      
      // Process updates to images resources
      if (e_scene.resources.images.is_mutated()) {
        auto &i_textures = info("textures").writeable<std::vector<detail::TextureLayout>>();
        i_textures.resize(e_scene.resources.images.size());
        
        for (uint i = 0; i < i_textures.size(); ++i) {
          const auto &rsrc = e_scene.resources.images[i];
          guard_continue(rsrc.is_mutated());
          i_textures[i] = detail::TextureLayout::realize(rsrc.value());
        } // for (uint i)
      } // if (images)

      // Scene resources are now up-to-date; set internal state flags to non-mutated in scene object
      info.global("scene_handler").writeable<SceneHandler>().set_mutated(false);
    }
  };
} // namespace met