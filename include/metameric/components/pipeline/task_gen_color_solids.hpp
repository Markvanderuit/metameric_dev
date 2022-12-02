#pragma once

#include <metameric/core/mesh.hpp>
#include <metameric/core/scheduler.hpp>

namespace met {
  class GenColorSolidsTask : public detail::AbstractTask {
    using Array6f = eig::Array<float, 6, 1>;

    std::vector<Array6f> m_sphere_samples;    
    HalfedgeMesh         m_sphere_mesh;
    
  public:
    GenColorSolidsTask(const std::string &name);
    void init(detail::TaskInitInfo &) override;
    void eval(detail::TaskEvalInfo &) override;
  };
} // namespace met