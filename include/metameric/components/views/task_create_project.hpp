#pragma once

#include <metameric/core/data.hpp>
#include <metameric/core/io.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/components/schedule.hpp>
#include <metameric/components/views/detail/file_dialog.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/texture.hpp>
#include <string>

namespace met {
  class CreateProjectTask : public detail::AbstractTask {
    std::string m_view_title;

    ProjectCreateInfo                                    m_proj_data;
    std::vector<std::pair<std::string, gl::Texture2d3f>> m_imag_data;

    // Modal spawning functions
    void insert_progress_warning(detail::TaskEvalInfo &info);

    // Project building functions
    bool create_project_safe(detail::TaskEvalInfo &info);
    bool create_project(detail::TaskEvalInfo &info);

  public:
    CreateProjectTask(const std::string &name, const std::string &view_title);
    void init(detail::TaskInitInfo &info) override;
    void dstr(detail::TaskDstrInfo &info) override;
    void eval(detail::TaskEvalInfo &info) override;
  };
} // namespace met