#pragma once

#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <metameric/core/detail/scheduler_task.hpp>

namespace met {
  class ViewportDrawGamutTask : public detail::AbstractTask {
    // Cache objects to detect state changes in UI components
    uint              m_buffer_object_cache;
    std::vector<uint> m_vert_select_cache;
    std::vector<uint> m_vert_msover_cache;
    std::vector<uint> m_elem_select_cache;
    std::vector<uint> m_elem_msover_cache;

    // Local buffer to store individual opacities/sizes for vertex/element selection/mouseover;
    // each buffer is mapped for flushable changes
    gl::Buffer        m_vert_size_buffer;
    gl::Buffer        m_elem_opac_buffer;
    std::span<float>  m_vert_size_map;
    std::span<float>  m_elem_opac_map;

    // Graphics draw components
    gl::Array    m_vert_array;
    gl::Array    m_elem_array;
    gl::Buffer   m_inst_vert_buffer;
    gl::Buffer   m_inst_elem_buffer;
    gl::Program  m_vert_program;
    gl::Program  m_elem_program;
    gl::DrawInfo m_vert_draw;
    gl::DrawInfo m_elem_draw;

  public:
    ViewportDrawGamutTask(const std::string &);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
    void dstr(detail::TaskDstrInfo &) override;
  };
} // namespace met