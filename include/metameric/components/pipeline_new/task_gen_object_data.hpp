#pragma once

#include <metameric/core/scene.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/detail/scheduler_subtasks.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/program.hpp>

namespace met {
  class GenObjectDataTask : public detail::TaskNode {
    struct UnifLayout {
      alignas(4) uint         object_i;
    };

    uint             m_object_i;
    uint             m_atlas_layer_i;
    gl::Program      m_program_txtr; // Program for texture handling
    gl::Program      m_program_colr; // Program for single-color handling
    gl::Buffer       m_unif_buffer;
    UnifLayout      *m_unif_map;
    gl::Framebuffer  m_fbo;

    // Keys for program caches
    std::string m_cache_key_txtr; // Program for texture handling
    std::string m_cache_key_colr; // Program for single-color handling


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