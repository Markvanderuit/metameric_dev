#include <metameric/components/views/detail/arcball.hpp>

namespace met::detail {
  Arcball::Arcball(ArcballCreateInfo info)
  : m_is_mutated(true),
    m_fov_y(info.fov_y),
    m_near_z(info.near_z),
    m_far_z(info.far_z),
    m_aspect(info.aspect),
    m_zoom(info.dist),
    m_eye(info.e_eye.matrix().normalized()), 
    m_center(info.e_center), 
    m_up(info.e_up),
    m_zoom_delta_mult(info.zoom_delta_mult),
    m_ball_delta_mult(info.ball_delta_mult),
    m_move_delta_mult(info.move_delta_mult) {
    met_trace();
    update();
  }
  
  void Arcball::update() const {
    met_trace();
    
    m_view = eig::lookat_rh(m_zoom * (m_eye - m_center) + m_center, m_center, m_up);
    m_proj = eig::perspective_rh_no(m_fov_y, m_aspect, m_near_z, m_far_z);
    m_full = m_proj * m_view;
    
    m_is_mutated = false;
  }

  // Before next update_matrices() call, set distance delta
  void Arcball::set_zoom_delta(float delta) {
    met_trace();

    guard(delta != 0);
    m_is_mutated = true;

    m_zoom = std::max(m_zoom + delta * m_zoom_delta_mult, 0.01f);
  }

  void Arcball::set_ball_delta(eig::Array2f delta) {
    met_trace();

    guard(!delta.isZero());
    m_is_mutated = true;

    eig::Array2f delta_angle = delta
                              * m_ball_delta_mult 
                              * eig::Array2f(-2, 1) 
                              * std::numbers::pi_v<float>;
    
    // Extract required vectors
    eig::Vector3f right_v = -m_view.matrix().transpose().col(0).head<3>();
    eig::Vector3f view_v  =  m_view.matrix().transpose().col(2).head<3>();

    // Handle view=up edgecase
    if (view_v.dot(m_up.matrix()) * delta_angle.sign().y() >= 0.99f)
      delta_angle.y() = 0.f;

    // Describe camera rotation around pivot on _separate_ axes
    eig::Affine3f rot(eig::AngleAxisf(delta_angle.y(), right_v)
                    * eig::AngleAxisf(delta_angle.x(), m_up.matrix()));
    
    // Apply rotation
    m_eye = m_center + rot * (m_eye - m_center);
  }
  
  void Arcball::set_move_delta(eig::Array3f delta) {
    met_trace();

    guard(!delta.isZero());
    m_is_mutated = true;

    // Extract required camera vectors
    eig::Array3f right_v   = -m_view.matrix().transpose().col(0).head<3>();
    eig::Array3f up_v      =  m_view.matrix().transpose().col(1).head<3>();
    eig::Array3f forward_v =  m_view.matrix().transpose().col(2).head<3>();

    // Define translation across camera vectors
    eig::Translation3f transform(right_v   * 2.f * delta.x()
                               + up_v      * 2.f * delta.y()
                               + forward_v * 2.f * delta.z());

    // Translate entire of camera by specified amount
    m_center = transform * m_center.matrix();
    m_eye    = transform * m_eye.matrix();
  }

  Ray Arcball::generate_ray(eig::Vector2f screen_pos) const {
    met_trace();

    if (m_is_mutated)
      update();
      
    const float tan = std::tanf(m_fov_y * .5f);
    const auto  mat = m_view.inverse(); // camera-to-world

    eig::Vector2f s = (screen_pos.array() - .5f) * 2.f;
    eig::Vector3f o = mat * eig::Vector3f::Zero();
    eig::Vector3f d = (mat * eig::Vector3f(s.x() * tan * m_aspect, 
                                            s.y() * tan, 
                                            -1) - o).normalized();
    
    return { o, d };
  }
} // namespace met::detail