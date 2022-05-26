#pragma once

#include <metameric/core/utility.hpp>
#include <iostream>

namespace met::detail {
  struct ArcballCreateInfo {
    float fov_y  = glm::radians(45.f);
    float near_z = 0.001f;
    float far_z  = 1000.f;
    float aspect = 1.f;
    float dist   = 1.f;
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
    glm::vec3 m_eye    = glm::vec3(1, 0, 0);
    glm::vec3 m_center = glm::vec3(0, 0, 0);
    glm::vec3 m_up     = glm::vec3(0, 1, 0);

  public:
    /* constr */
    Arcball(ArcballCreateInfo info = {})
    : m_fov_y(info.fov_y), m_near_z(info.near_z), m_far_z(info.far_z), 
      m_aspect(info.aspect), m_dist(info.dist) {
      update_matrices();
    }

    /* public data members */

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
    glm::mat4 full() const { return m_proj * m_view; }

    void update_matrices() {
      m_view = glm::lookAt(m_dist * m_eye, m_center, m_up);
      m_proj = glm::perspective(m_fov_y, m_aspect, m_near_z, m_far_z);
    }

    void update_dist_delta(float dist_delta) {
      if (m_dist + dist_delta > 0.01f) {
        m_dist += dist_delta;
      }
      std::cout << m_dist << std::endl;
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
    }
  };
} // namespace met::detail