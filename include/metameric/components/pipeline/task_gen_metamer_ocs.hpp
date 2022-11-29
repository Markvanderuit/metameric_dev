#pragma once

#include <metameric/core/mesh.hpp>
#include <metameric/core/scheduler.hpp>

namespace met {
  class GenMetamerOCSTask : public detail::AbstractTask {
    using Array6f = eig::Array<float, 6, 1>;

    std::vector<Array6f> m_sphere_samples;    
    AlArray3fMesh        m_sphere_mesh;
    float                m_threshold = 0.05f;
    float                m_error     = 0.5f;
    
  public:
    GenMetamerOCSTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met