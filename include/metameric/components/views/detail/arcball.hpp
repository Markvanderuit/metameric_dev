#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/utility.hpp>
#include <numbers>

namespace met::detail {
  struct ArcballCreateInfo {
    float fov_y       = glm::radians(45.f);
    float near_z      = 0.001f;
    float far_z       = 1000.f;
    float aspect      = 1.f;
    float dist        = 1.f;
    glm::vec3 eye     = glm::vec3(1, 0, 0);
    glm::vec3 center  = glm::vec3(0, 0, 0);
    glm::vec3 up      = glm::vec3(0, 1, 0);

    eig::Array3f e_eye    = { 1, 0, 0 };
    eig::Array3f e_center = { 0, 0, 0 };
    eig::Array3f e_up     = { 0, 1, 0 };
  };

  /* 
    Arcball camera with zoom support.
    src: https://asliceofrendering.com/camera/2019/11/30/ArcballCamera/
  */
  struct Arcball {
  private:
    /* private data members */

    glm::mat4 m_view;
    glm::mat4 m_proj;
    glm::mat4 m_full;

    eig::Affine3f     m_e_view;
    eig::Projective3f m_e_proj;
    eig::Projective3f m_e_full;

  public:
    using InfoType = ArcballCreateInfo;


    /* constr */
    Arcball(ArcballCreateInfo info = {})
    : m_fov_y(info.fov_y), m_near_z(info.near_z), m_far_z(info.far_z), 
      m_aspect(info.aspect), m_dist(info.dist), m_eye(info.eye),
      m_center(info.center), m_up(info.up),
      m_e_eye(info.e_eye), m_e_center(info.e_center), m_e_up(info.e_up) {
      update_matrices();
    }

    /* public data members */

    glm::vec3 m_eye;
    glm::vec3 m_center;
    glm::vec3 m_up;

    eig::Array3f m_e_eye;
    eig::Array3f m_e_center;
    eig::Array3f m_e_up;

    float m_fov_y;
    float m_near_z;
    float m_far_z;
    float m_aspect;
    float m_dist;

    /* publicly accessible matrices */

    // Access view matrix
    glm::mat4 & view() { return m_view; }

    // Access projection matrix
    glm::mat4 & proj() { return m_proj; }

    // Obtain full camera matrix
    glm::mat4 & full() { return m_full; }

    eig::Affine3f     & e_view() { return m_e_view; }
    eig::Projective3f & e_proj() { return m_e_proj; }
    eig::Projective3f & e_full() { return m_e_full; }

    void update_matrices() {
      m_view = glm::lookAt(m_dist * (m_eye - m_center) + m_center, 
                           m_center, 
                           m_up);
      m_e_view = eig::lookat_rh(m_dist * (m_e_eye - m_e_center) + m_e_center,
                                m_e_center,
                                m_e_up);
      
      m_proj = glm::perspective(m_fov_y, m_aspect, m_near_z, m_far_z);
      m_e_proj = eig::perspective_rh_no(m_fov_y, m_aspect, m_near_z, m_far_z);

      m_full = m_proj * m_view;
      m_e_full = m_e_proj * m_e_view;
    }

    void update_dist_delta(float dist_delta) {
      if (m_dist + dist_delta > 0.01f) {
        m_dist += dist_delta;
      }
    }

    void e_update_pos_delta(eig::Array2f pos_delta) {
      guard(!pos_delta.isZero());

      eig::Array2f delta_angle = pos_delta 
                               * eig::Array2f(-2, 1) 
                               * std::numbers::pi_v<float>;
      
      // Extract required vectors
      eig::Vector3f view_v  =  m_e_view.matrix().transpose().col(2).head<3>();
      eig::Vector3f right_v = -m_e_view.matrix().transpose().col(0).head<3>();

      // Handle view=up edgecase
      if (view_v.dot(m_e_up.matrix()) * delta_angle.sign().y() >= 0.99f) {
        delta_angle.y() = 0.f;
      }

      // Describe camera rotation around pivot on _separate_ axes
      eig::Affine3f rot;
      rot = eig::AngleAxisf(delta_angle.y(), right_v.matrix())
          * eig::AngleAxisf(delta_angle.x(), m_e_up.matrix());
      
      // Apply rotation
      m_e_eye = m_e_center + rot * (m_e_eye - m_e_center);
      fmt::print("eig {}\n", m_e_eye);
    }

    // Update trackball internal information with new mouse delta, expected [-1, 1]
    void update_pos_delta(glm::vec2 pos_delta) {
      guard(pos_delta != glm::vec2(0));

      // Homogeneous versions of camera eye, pivot center
      glm::vec4 eye_hom = glm::vec4(m_eye, 1.f);
      glm::vec4 cen_hom = glm::vec4(m_center, 1.f);

      // Calculate amount of rotation in radians
      glm::vec2 delta_angle = pos_delta * glm::vec2(-2.f, 1.f) * glm::pi<float>();

      // Extract required vectors for later
      glm::vec3 view_v = glm::transpose(m_view)[2];
      glm::vec3 right_v = -glm::transpose(m_view)[0];
      
      // Prevent view=up edgecase
      if (glm::dot(view_v, m_up) * glm::sign(delta_angle.y) >= 0.99f) {
        delta_angle.y = 0.f;
      }

      // Rotate camera around pivot on _separate_ axes
      glm::mat4 rot = glm::rotate(delta_angle.y, right_v)
                    * glm::rotate(delta_angle.x, m_up);

      // Apply rotation and recompute matrices
      m_eye = glm::vec3(cen_hom + rot * (eye_hom - cen_hom));
      fmt::print("glm {}\n", glm::to_string(m_eye));
    }
  };
} // namespace met::detail