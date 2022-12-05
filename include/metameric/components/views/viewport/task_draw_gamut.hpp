#pragma once

#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <metameric/core/detail/scheduler_task.hpp>

namespace met {
  class ViewportDrawGamutTask : public detail::AbstractTask {
    // Caches to detect buffer/selection changes
    uint              m_gamut_vert_cache;
    std::vector<uint> m_gamut_select_cache;
    std::vector<uint> m_gamut_msover_cache;

    // Local components to have individual opacities for selections/mouseovers
    gl::Buffer       m_opac_buffer;
    std::span<float> m_opac_map;

    // Gamut draw components
    gl::Array    m_array;
    gl::Program  m_program;
    gl::DrawInfo m_draw;

  public:
    ViewportDrawGamutTask(const std::string &);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
    void dstr(detail::TaskDstrInfo &) override;
  };
} // namespace met