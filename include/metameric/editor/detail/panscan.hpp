// Copyright (C) 2024 Mark van de Ruit, Delft University of Technology.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/utility.hpp>
#include <numbers>

namespace met::detail {
  struct PanscanInfo {
    float near_z        =-1000.f;
    float far_z         = 1000.f;
    float scale         = 1.f;
    eig::Array2f aspect = 1.f;

    eig::Vector3f e_eye    = { 0, 0,-1 };
    eig::Vector3f e_center = { 0, 0, 0 };
    eig::Vector3f e_up     = { 0, 1, 0 };

    // Multipliers to scrolling/movement deltas
    eig::Array2f pos_delta_mult   = 1.f; 
    float        scale_delta_mult = 1.f; 
    float        scale_delta_curv = 1.f;
  };

  class Panscan {
    eig::Affine3f     m_view;
    eig::Projective3f m_orth;
    eig::Projective3f m_full;
    eig::Vector3f     m_eye;
    eig::Vector3f     m_center;
    eig::Vector3f     m_up;
    float             m_scale;
    eig::Array2f      m_pos_delta_mult;
    float             m_scale_delta_mult;
    float             m_scale_delta_curv;

  public:
    using InfoType = PanscanInfo;

    /* constr */

    Panscan(PanscanInfo info = { })
    : m_scale(info.scale),
      m_near_z(info.near_z),
      m_far_z(info.far_z),
      m_aspect(info.aspect),
      m_eye(info.e_eye.matrix().normalized()), 
      m_center(info.e_center), 
      m_up(info.e_up),
      m_scale_delta_mult(info.scale_delta_mult),
      m_scale_delta_curv(info.scale_delta_curv),
      m_pos_delta_mult(info.pos_delta_mult)
    {
      update_matrices();
    }

    /* public data members; call update_matrices() after changing */

    float m_near_z;
    float m_far_z;
    eig::Array2f m_aspect;

    /* public matrix accessors; call after update_matrices() */
     
    eig::Affine3f           & view()       { return m_view; };
    eig::Projective3f       & full()       { return m_full; };
    eig::Projective3f       & orth()       { return m_orth; };
    const eig::Affine3f     & view() const { return m_view; };
    const eig::Projective3f & full() const { return m_full; };
    const eig::Projective3f & orth() const { return m_orth; };

    void update_matrices() {
      met_trace();

      m_view = eig::lookat_rh(m_eye, m_center, m_up);
      m_orth = eig::ortho(-m_scale * m_aspect.x(), m_scale * m_aspect.x(), 
                          -m_scale * m_aspect.y(), m_scale * m_aspect.y(), 
                           m_near_z, m_far_z);
      m_full = m_orth * m_view;
    }

    // Before next update_matrices() call, set scaling delta
    void set_scale_delta(float scale_delta) {
      met_trace();

      float delta = scale_delta * m_scale_delta_mult;
      float curv = expf(1.f + m_scale * m_scale_delta_curv) * delta;

      float diff;
      if (delta > 0.f) {
        diff = curv;
      } else {
        float prev_scale = std::max(m_scale + curv, 0.0001f);
        diff = expf(1.f + prev_scale * m_scale_delta_curv) * delta;
      }

      m_scale = std::clamp(m_scale + diff, 0.0001f, 100.f);
    }

    // Before next update_matrices() call, set positional delta
    void set_pos_delta(eig::Array2f pos_delta) {
      met_trace();

      guard(!pos_delta.isZero());

      // Describe u/v vectors on camera plane
      eig::Vector3f f = (m_center - m_eye).normalized();
      eig::Vector3f s = f.cross(m_up).normalized();
      eig::Vector3f u = s.cross(f);

      // Describe 2-dimensional translation on camera plane
      eig::Array2f delta = pos_delta * m_scale * m_pos_delta_mult;
      eig::Affine3f transl(eig::Translation3f(s *-delta.x()) 
                         * eig::Translation3f(u * delta.y()));

      // Apply translation
      m_center = transl * m_center;
      m_eye    = transl * m_eye;
    }
  };
} // namespace met::detail