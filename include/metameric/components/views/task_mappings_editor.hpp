#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/state.hpp>
#include <string>

namespace met {
  class MappingsEditorTask : public detail::AbstractTask {
    int         m_selected_i;
    std::string m_selected_key;
    MappingData m_selected_mapping;
    
    void add_mapping(detail::TaskEvalInfo &);
    void remove_mapping(detail::TaskEvalInfo &);
    void change_mapping(detail::TaskEvalInfo &);
    void reset_mapping(detail::TaskEvalInfo &);

    void draw_list(detail::TaskEvalInfo &);
    void draw_selection(detail::TaskEvalInfo &);

  public:
    MappingsEditorTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met