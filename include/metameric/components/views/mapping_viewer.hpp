#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/state.hpp>
#include <string>

namespace met {
  class MappingViewer : public detail::AbstractTask {
    int         m_selected_i;
    std::string m_selected_key;
    MappingData m_selected_mapping;
    
    void handle_add_mapping(detail::TaskEvalInfo &);
    void handle_remove_mapping(detail::TaskEvalInfo &);

    void draw_list(detail::TaskEvalInfo &);
    void draw_selection(detail::TaskEvalInfo &);

  public:
    MappingViewer(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met