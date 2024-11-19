#pragma once

#include <metameric/scene/scene.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/detail/scheduler_subtasks.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/sampler.hpp>

namespace met {
  class GenObjectDataTask : public detail::TaskNode {
    struct UnifLayout {
      alignas(4) uint  object_i;
      alignas(4) float px_scale;
    };

    // Shared data
    uint             m_object_i;
    gl::Sampler      m_sampler;

    // Objects specifically for spectral coefficient bake
    uint            m_coef_layer_i;
    gl::Buffer      m_coef_unif;
    UnifLayout     *m_coef_unif_map;
    gl::Framebuffer m_coef_fbo;
    std::string     m_coef_cache_key; // Program-cache key for texture handling

    // Objects specifically for brdf data bake
    uint            m_brdf_layer_i;
    gl::Buffer      m_brdf_unif;
    UnifLayout     *m_brdf_unif_map;
    gl::Framebuffer m_brdf_fbo;
    std::string     m_brdf_cache_key; // Program-cache key for texture handling

  public:
    GenObjectDataTask(uint object_i);

    bool is_active(SchedulerHandle &) override;
    void init(SchedulerHandle &)      override;
    void eval(SchedulerHandle &)      override;
  };

  class GenObjectsTask : public detail::TaskNode {
    detail::Subtasks<GenObjectDataTask> m_subtasks;

  public:
    void init(SchedulerHandle &info) override {
      met_trace();

      // Get external resources
      const auto &e_scene = info.global("scene").getr<Scene>();

      // Add subtasks of type GenObjectDataTask
      m_subtasks.init(info, e_scene.components.objects.size(), 
        [](uint i)         { return fmt::format("gen_object_{}", i); },
        [](auto &, uint i) { return GenObjectDataTask(i);            });
    }

    void eval(SchedulerHandle &info) override {
      met_trace();

      // Get external resources
      const auto &e_scene = info.global("scene").getr<Scene>();

      // Adjust nr. of subtasks to current nr. of objects
      m_subtasks.eval(info, e_scene.components.objects.size());
    }
  };
} // namespace met