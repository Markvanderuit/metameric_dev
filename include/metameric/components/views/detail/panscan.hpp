#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/utility.hpp>

namespace met::detail {
  struct PanscanCreateInfo {

  };

  class Panscan {
    eig::Affine3f     m_view;
    eig::Projective3f m_orth;
    eig::Projective3f m_full;
    eig::Array3f      m_eye;
    eig::Array3f      m_center;
    eig::Array3f      m_up;
    float             m_scale;
    float             m_scale_delta_mult;
    eig::Array2f      m_pos_delta_mult;

  public:
    using InfoType = PanscanCreateInfo;

    /* public data members; call update_matrices() after changing */

    float m_near_z;
    float m_far_z;
    float m_aspect;

    /* public matrix accessors; call after update_matrices() */
     
    eig::Affine3f           & view()       { return m_view; };
    eig::Projective3f       & full()       { return m_full; };
    eig::Projective3f       & orth()       { return m_orth; };
    const eig::Affine3f     & view() const { return m_view; };
    const eig::Projective3f & full() const { return m_full; };
    const eig::Projective3f & orth() const { return m_orth; };

    void update_matrices() {
      m_view = eig::lookat_rh(m_eye, m_center, m_up);
      m_orth = eig::ortho(-1, 1, -1, 1, m_near_z, m_far_z);
      m_full = m_orth * m_view;  
    }

    // Before next update_matrices() call, set scaling delta
    void set_scale_delta(float scale_delta) {
      m_scale = std::max(m_scale + scale_delta * m_scale_delta_mult, 0.01f);
    }

    // Before next update_matrices() call, set positional delta
    void set_pos_delta(eig::Array2f pos_delta) {
      guard(!pos_delta.isZero());

      // Describe 2-dimensional translation
      eig::Array2f delta = pos_delta * m_pos_delta_mult;
      eig::Affine3f tnsl(eig::Translation2f(delta));

      
    }

  };
} // namespace met::detail