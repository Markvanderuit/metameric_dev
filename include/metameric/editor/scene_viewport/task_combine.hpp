#pragma once

#include <metameric/core/scheduler.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/program.hpp>

namespace met {
  class ViewportCombineTask : public detail::TaskNode {
    struct UnifLayout {
      eig::Vector2u viewport_size;
      uint          sample_checkerboard;
    };

    std::string  m_program_key;
    gl::Buffer   m_unif_buffer;
    UnifLayout  *m_unif_buffer_map;
    
  public:
    bool is_active(SchedulerHandle &info) override;
    void init(SchedulerHandle &info)      override;
    void eval(SchedulerHandle &info)      override;
  };
} // namespace met