#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/scene_handler.hpp>
#include <metameric/core/detail/scheduler_subtasks.hpp>

namespace met {
  class GenUpliftingDataTask : public detail::TaskNode {
    uint              m_uplifting_i;
    std::vector<Colr> m_csys_boundary_samples;
    std::vector<Spec> m_csys_boundary_spectra;
    std::vector<Colr> m_tesselation_points;

  public:
    GenUpliftingDataTask(uint uplifting_i);

    bool is_active(SchedulerHandle &) override;
    void init(SchedulerHandle &)      override;
    void eval(SchedulerHandle &)      override;
  };

  class GenUpliftingsTask : public detail::TaskNode {
    detail::Subtasks<GenUpliftingDataTask> m_subtasks;

  public:
    void init(SchedulerHandle &info) override {
      met_trace();

      // Get external resources
      const auto &e_scene_handler = info.global("scene_handler").read_only<SceneHandler>();
      const auto &e_upliftings    = e_scene_handler.scene.components.upliftings;

      // Add subtasks to perform mapping
      m_subtasks.init(info, e_upliftings.size(), 
        [](uint i)         { return fmt::format("gen_uplifting_{}", i); },
        [](auto &, uint i) { return GenUpliftingDataTask(i);            });
    }

    void eval(SchedulerHandle &info) override {
      met_trace();

      // Get external resources
      const auto &e_scene_handler = info.global("scene_handler").read_only<SceneHandler>();
      const auto &e_upliftings    = e_scene_handler.scene.components.upliftings;

      // Adjust nr. of subtasks
      m_subtasks.eval(info, e_upliftings.size());
    }
  };
} // namespace met