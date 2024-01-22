#pragma once

#include <metameric/core/math.hpp>
#include <small_gl/buffer.hpp>

namespace met {
  struct Sensor {
    // Underlying camera transforms
    eig::Matrix4f proj_trf = eig::Matrix4f::Identity();
    eig::Matrix4f view_trf = eig::Matrix4f::Identity();

    // Target film resolution
    eig::Array2u  film_size = { 1, 1 };
    
  private:
    struct UnifLayout {
      alignas(16) eig::Matrix4f full_trf; 
      alignas(16) eig::Matrix4f proj_trf; 
      alignas(16) eig::Matrix4f view_trf; 
      alignas(8)  eig::Array2u  film_size; 
    };

    gl::Buffer  m_unif;
    UnifLayout *m_unif_map;
    
  public:
    const gl::Buffer &buffer() const {
      return m_unif;
    }

    // Call to flush updated sample/camera settings
    void flush();
  };

  struct PathQuery {
    // Query path starting ray
    eig::Vector3f origin, direction;

    // Target output size; nr of resulting query paths
    uint n_paths;

  private:
    struct UnifLayout {
      alignas(16) eig::Vector3f origin;
      alignas(16) eig::Vector3f direction;
      alignas(4)  uint          n_paths; 
    };

    gl::Buffer  m_unif;
    UnifLayout *m_unif_map;

  public:
    const gl::Buffer &buffer() const {
      return m_unif;
    }

    // Call to flush updated sample/camera settings
    void flush();
  };
} // namespace met