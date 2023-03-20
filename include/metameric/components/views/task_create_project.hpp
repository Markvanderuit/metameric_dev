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
  class CreateProjectTask : public detail::TaskBase {
    std::string m_view_title;

    ProjectCreateInfo                                    m_proj_data;
    std::vector<std::pair<std::string, gl::Texture2d3f>> m_imag_data;

    // Project building functions
    bool create_project_safe(SchedulerHandle &info);
    bool create_project(SchedulerHandle &info);
    
    // eval() sections
    void eval_images_section(SchedulerHandle &info);
    void eval_data_section(SchedulerHandle &info);
    void eval_progress_modal(SchedulerHandle &info);

  public:
    CreateProjectTask(const std::string &view_title);
    
    void init(SchedulerHandle &info) override;
    void dstr(SchedulerHandle &info) override;
    void eval(SchedulerHandle &info) override;
  };
} // namespace met