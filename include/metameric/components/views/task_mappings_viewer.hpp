#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>
#include <small_gl/texture.hpp>
#include <vector>

namespace met {
  class MappingsViewerTask : public detail::AbstractTask {
    std::span<Spec>              m_spectrum_buffer_map;
    std::vector<gl::Texture2d4f> m_texture_small;
    eig::Array2u                 m_texture_size = 256;

    void init_resample_subtasks(detail::AbstractTaskInfo &info, uint n);
    void dstr_resample_subtasks(detail::AbstractTaskInfo &info, uint n);
    void add_resample_subtask(detail::AbstractTaskInfo &info, uint i);
    void rmv_resample_subtask(detail::AbstractTaskInfo &info, uint i);
    
    void handle_tooltip(detail::TaskEvalInfo &info, uint texture_i);
    void handle_popout(detail::TaskEvalInfo &info, uint texture_i);

  public:
    MappingsViewerTask(const std::string &name);
    
    void init(detail::TaskInitInfo &) override;
    void dstr(detail::TaskDstrInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met
