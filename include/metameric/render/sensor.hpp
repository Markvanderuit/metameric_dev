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
#include <small_gl/buffer.hpp>

namespace met {
  // GL-side representation of a very simple sensor that, 
  // if sampled, returns rays across the film
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
    static_assert(sizeof(UnifLayout) == 208);

    gl::Buffer  m_unif;
    UnifLayout *m_unif_map;
    
  public:
    const gl::Buffer &buffer() const {
      return m_unif;
    }

    // Call to flush updated sample/camera settings
    void flush();
  };

  // GL-side representation of a very simple sensor that, 
  // if sampled, returns rays originating in a single pixel
  struct PixelSensor {
    // Underlying camera transforms
    eig::Matrix4f proj_trf = eig::Matrix4f::Identity();
    eig::Matrix4f view_trf = eig::Matrix4f::Identity();

    // Target film resolution
    eig::Array2u  film_size = { 1, 1 };

    // Target pixel
    eig::Array2u pixel;

  private:
    struct UnifLayout {
      alignas(16) eig::Matrix4f full_trf; 
      alignas(16) eig::Matrix4f proj_trf; 
      alignas(16) eig::Matrix4f view_trf; 
      alignas(8)  eig::Array2u  film_size; 
      alignas(8)  eig::Array2u  pixel;
    };
    static_assert(sizeof(UnifLayout) == 208);

    gl::Buffer  m_unif;
    UnifLayout *m_unif_map;

  public:
    const gl::Buffer &buffer() const {
      return m_unif;
    }

    // Call to flush updated sample/camera settings
    void flush();
  };

  // GL-side representation of a very simple sensor that, 
  // if sampled, returns a single specified ray
  struct RaySensor {
    // Query path starting ray
    eig::Vector3f origin, direction;

  private:
    struct alignas(16) UnifLayout {
      eig::AlVector3f origin;
      eig::AlVector3f direction;
    };
    static_assert(sizeof(UnifLayout) == 32);

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