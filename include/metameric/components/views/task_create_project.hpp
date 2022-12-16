#pragma once

#include <metameric/core/data.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/components/schedule.hpp>
#include <metameric/components/views/detail/file_dialog.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <string>

namespace met {
  class CreateProjectTask : public detail::AbstractTask {
    std::string m_input_path;
    std::string m_view_title;

    // Modal spawning functions
    void insert_progress_warning(detail::TaskEvalInfo &info);
    void insert_file_warning();

    // Project building functions
    bool create_project_safe(detail::TaskEvalInfo &info);
    bool create_project(detail::TaskEvalInfo &info);

  public:
    CreateProjectTask(const std::string &name, const std::string &view_title);
    void eval(detail::TaskEvalInfo &info) override;
  };
} // namespace met