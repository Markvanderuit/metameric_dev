#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/scheduler.hpp>

namespace met {
  class GenSpectralMappingsTask : public detail::AbstractTask {
    uint m_max_maps;
    
  public:
    GenSpectralMappingsTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met