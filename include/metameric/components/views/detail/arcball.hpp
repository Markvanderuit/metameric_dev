#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/utility.hpp>
#include <numbers>

namespace met::detail {
  struct ArcballCreateInfo {
    float fov_y  = 45.f * std::numbers::pi_v<float> / 180.f;
    float near_z = 0.001f;
    float far_z  = 1000.f;
    float aspect = 1.f;
    float dist   = 1.f;

    eig::Array3f e_eye    = { 1, 0, 0 };
    eig::Array3f e_center = { 0, 0, 0 };
    eig::Array3f e_up     = { 0, 1, 0 };

    // Multipliers to scrolling/movement deltas
    float        dist_delta_mult = 1.f; 
    eig::Array2f pos_delta_mult  = 1.f; 
  };

  /* 
    Arcball camera with zoom support.
    src: https://asliceofrendering.com/camera/2019/11/30/ArcballCamera/
  */
  struct Arcball {
  private:
    eig::Affine3f     m_view;
    eig::Projective3f m_proj;
    eig::Projective3f m_full;
    eig::Array3f      m_eye;
    eig::Array3f      m_center;
    eig::Array3f      m_up;
    float             m_dist;
    float             m_dist_delta_mult;
    eig::Array2f      m_pos_delta_mult;

  public:
    using InfoType = ArcballCreateInfo;

    /* constr */
    Arcball(ArcballCreateInfo info = { })
    : m_fov_y(info.fov_y),
      m_near_z(info.near_z),
      m_far_z(info.far_z),
      m_aspect(info.aspect),
      m_dist(info.dist),
      m_eye(info.e_eye), 
      m_center(info.e_center), 
      m_up(info.e_up),
      m_pos_delta_mult(info.pos_delta_mult),
      m_dist_delta_mult(info.dist_delta_mult) 
    {
      update_matrices();
    }

    /* public data members; call update_matrices() after changing */

    float m_fov_y;
    float m_near_z;
    float m_far_z;
    float m_aspect;

    /* public matrix accessors; call after update_matrices() */
    
    eig::Affine3f     & view() { return m_view; } // Access view transform
    eig::Projective3f & proj() { return m_proj; } // Access projection transform    
    eig::Projective3f & full() { return m_full; } // Access full proj * view transform

    /* update functions */

    // Re-compute view, projection and full transforms
    void update_matrices() {
      m_view = eig::lookat_rh(m_dist * (m_eye - m_center) + m_center, m_center, m_up);
      m_proj = eig::perspective_rh_no(m_fov_y, m_aspect, m_near_z, m_far_z);
      m_full = m_proj * m_view;
    }

    // Before next update_matrices() call, set distance delta
    void set_dist_delta(float dist_delta) {
      m_dist = std::max(m_dist + dist_delta * m_dist_delta_mult, 0.01f);
    }

    // Before next update_matrices() call, set positional delta
    void set_pos_delta(eig::Array2f pos_delta) {
      guard(!pos_delta.isZero());

      eig::Array2f delta_angle = pos_delta
                               * m_pos_delta_mult 
                               * eig::Array2f(-2, 1) 
                               * std::numbers::pi_v<float>;
      
      // Extract required vectors
      eig::Vector3f right_v = -m_view.matrix().transpose().col(0).head<3>();
      eig::Vector3f view_v  =  m_view.matrix().transpose().col(2).head<3>();

      // Handle view=up edgecase
      if (view_v.dot(m_up.matrix()) * delta_angle.sign().y() >= 0.99f) {
        delta_angle.y() = 0.f;
      }

      // Describe camera rotation around pivot on _separate_ axes
      eig::Affine3f rot(eig::AngleAxisf(delta_angle.y(), right_v)
                      * eig::AngleAxisf(delta_angle.x(), m_up.matrix()));
      
      // Apply rotation
      m_eye = m_center + rot * (m_eye - m_center);
    }
  };
} // namespace met::detail