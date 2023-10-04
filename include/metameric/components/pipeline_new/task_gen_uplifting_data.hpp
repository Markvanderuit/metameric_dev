#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/scene.hpp>
#include <metameric/core/detail/scheduler_subtasks.hpp>

namespace met {
  class GenUpliftingDataTask : public detail::TaskNode {
    uint              m_uplifting_i;

    // Packed wrapper data for tetrahedron; 64 bytes for std430 
    struct ElemPack {
      eig::Matrix<float, 4, 3> inv; // Last column is padding
      eig::Matrix<float, 4, 1> sub; // Last value is padding
    };

    std::vector<Colr>   m_csys_boundary_samples;
    std::vector<Spec>   m_csys_boundary_spectra;
    std::vector<Colr>   m_tesselation_points;
    std::span<ElemPack> m_tesselation_pack_map;

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
      const auto &e_scene      = info.global("scene").getr<Scene>();
      const auto &e_upliftings = e_scene.components.upliftings;

      // Add subtasks to perform mapping
      m_subtasks.init(info, e_upliftings.size(), 
        [](uint i)         { return fmt::format("gen_uplifting_{}", i); },
        [](auto &, uint i) { return GenUpliftingDataTask(i);            });
    }

    void eval(SchedulerHandle &info) override {
      met_trace();

      // Get external resources
      const auto &e_scene      = info.global("scene").getr<Scene>();
      const auto &e_upliftings = e_scene.components.upliftings;

      // Adjust nr. of subtasks
      m_subtasks.eval(info, e_upliftings.size());
    }
  };
} // namespace met