#pragma once

#include <metameric/core/mesh.hpp>
#include <metameric/core/scheduler.hpp>

namespace met {
  class GenMetamerOCSTask : public detail::AbstractTask {
    using Array6f = eig::Array<float, 6, 1>;

    std::vector<Array6f> m_sphere_samples;    
    AlArray3fMesh        m_sphere_mesh;
    
  public:
    GenMetamerOCSTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met