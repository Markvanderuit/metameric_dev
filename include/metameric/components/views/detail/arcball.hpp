#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/record.hpp>
#include <metameric/core/utility.hpp>
#include <numbers>

namespace met::detail {
  struct ArcballInfo {
    float fov_y  = 45.f * std::numbers::pi_v<float> / 180.f;
    float near_z = 0.001f;
    float far_z  = 1000.f;
    float aspect = 1.f;
    float dist   = 1.f;

    eig::Array3f e_eye    = { 1, 0, 0 };
    eig::Array3f e_center = { 0, 0, 0 };
    eig::Array3f e_up     = { 0, 1, 0 };

    // Multipliers to scrolling/movement deltas
    float        zoom_delta_mult = 1.f; 
    eig::Array2f ball_delta_mult = 1.f; 
    eig::Array3f move_delta_mult = 1.f;
  };

  /* 
    Arcball camera with zoom support.
    src: https://asliceofrendering.com/camera/2019/11/30/ArcballCamera/
  */
  class Arcball {
    eig::Array3f m_eye;
    eig::Array3f m_center;
    eig::Array3f m_up;
    float        m_zoom;
    float        m_zoom_delta_mult;
    eig::Array2f m_ball_delta_mult;
    eig::Array3f m_move_delta_mult;
    float        m_fov_y;
    float        m_near_z;
    float        m_far_z;
    float        m_aspect;
    
    // Recompute output matrices
    void update() const; 

    // Mutable output matrices and auxiliary data
    mutable bool              m_is_mutated; // Flag to test for necessity of update
    mutable eig::Affine3f     m_view;
    mutable eig::Projective3f m_proj;
    mutable eig::Projective3f m_full;

  public: // Public members
    using InfoType = ArcballInfo;

    Arcball(ArcballInfo info = { });

    // Data accessors 
    const eig::Affine3f & view() const { 
      met_trace();
      if (m_is_mutated)
        update();
      return m_view; 
    }
    const eig::Projective3f & proj() const { 
      met_trace();
      if (m_is_mutated)
        update();
      return m_proj; 
    }
    const eig::Projective3f & full() const { 
      met_trace();
      if (m_is_mutated)
        update();
      return m_full; 
    }

    // Misc accessors
    eig::Array3f eye_pos() const { 
      met_trace();
      if (m_is_mutated)
        update();
      return m_center + m_zoom * m_eye; 
    }

    eig::Array3f eye_dir() const { 
      met_trace();
      if (m_is_mutated)
        update();
      return m_eye.matrix().normalized().eval(); 
    }

  public: // View control functions
    void set_fov_y(float fov_y) {
      m_fov_y = fov_y;
      m_is_mutated = true;
    }

    void set_near_z(float near_z) {
      m_near_z = near_z;
      m_is_mutated = true;
    }

    void set_far_z(float far_z) {
      m_far_z = far_z;
      m_is_mutated = true;
    }

    void set_aspect(float aspect) {
      m_aspect = aspect;
      m_is_mutated = true;
    }

    void set_zoom(float zoom) {
      m_zoom = zoom;
      m_is_mutated = true;
    }

    void set_eye(eig::Array3f eye) {
      m_eye = eye;
      m_is_mutated = true;
    }

    void set_center(eig::Array3f center) {
      m_center = center;
      m_is_mutated = true;
    }

  public: // Camera control functions
    void set_zoom_delta(float        delta); // Apply delta to camera zoom
    void set_ball_delta(eig::Array2f delta); // Appply delta to camera arcball-rotate
    void set_move_delta(eig::Array3f delta); // Apply delta to camera move

  public: // Misc
    float fov_y() const { return m_fov_y; }
    float near_z() const { return m_near_z; }
    float far_z() const { return m_far_z; }
    float aspect() const { return m_aspect; }

    Ray generate_ray(eig::Vector2f screen_pos) const;
  };
} // namespace met::detail