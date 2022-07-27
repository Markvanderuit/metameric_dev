#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/state.hpp>
#include <string>

namespace met {
  class MappingViewer : public detail::AbstractTask {
    std::string m_selected_mapping_key;
    MappingData m_selected_mapping_edit;
    uint        m_selected_mapping_i;
    
  public:
    MappingViewer(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met