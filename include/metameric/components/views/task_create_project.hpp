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

    // Project building functions
    bool create_project_safe(detail::TaskInfo &info);
    bool create_project(detail::TaskInfo &info);
    
    // eval() sections
    void eval_images_section(detail::TaskInfo &info);
    void eval_data_section(detail::TaskInfo &info);
    void eval_progress_modal(detail::TaskInfo &info);

  public:
    CreateProjectTask(const std::string &name, const std::string &view_title);
    
    void init(detail::TaskInfo &info) override;
    void dstr(detail::TaskInfo &info) override;
    void eval(detail::TaskInfo &info) override;
  };
} // namespace met