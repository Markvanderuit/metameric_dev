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

#include <metameric/scene/scene.hpp>
#include <metameric/render/sensor.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/renderbuffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met::detail {
  // Renderer base class
  class BaseRenderPrimitive {
  protected:
    gl::Texture2d4f m_film; // Color render target

  public:
    const gl::Texture2d4f &film() const { return m_film; }

    virtual const gl::Texture2d4f &render(const Sensor &sensor, const Scene &scene) {
      return m_film;
    }

    virtual void reset(const Sensor &sensor, const Scene &scene) {
      // ...
    }
  };

  // Query base class; queries track and return one or more paths or rays
  class BaseQueryPrimitive {
  protected:
    struct QueryUnifLayout {
      uint spp;
    };

    gl::Buffer       m_output;    // Query render target
    gl::Buffer       m_query;     // Query settings
    QueryUnifLayout *m_query_map; // Corresponding mapped buffer

    // Protected constructor to initialize query buffer and map 
    BaseQueryPrimitive();

  public:
    // Return output buffer
    const gl::Buffer &output() const { return m_output; }

    // Take n samples and return output buffer; default-implemented
    virtual const gl::Buffer &query(const RaySensor &sensor, const Scene &scene, uint spp) {
      return m_output;
    }
    
    // Take 1 sample and return output buffer
    const gl::Buffer &query(const RaySensor &sensor, const Scene &scene) {
      return query(sensor, scene, 1);
    }
  };

  // Repeated sampling renderer base class
  class IntegrationRenderPrimitive : public BaseRenderPrimitive {
    constexpr static uint sampler_state_size = 6;
    
    // Struct storing sampler state; modified across frames to count which sample is next
    struct SamplerState {
      alignas(4) uint spp_per_iter;
      alignas(4) uint spp_curr;
      alignas(4) uint pixel_checkerboard;
      alignas(4) uint pixel_curr;
    };
    
    // Rolling set of mapped buffers that track incrementing sampler state over several frames
    std::array<gl::Buffer,      sampler_state_size> m_sampler_state_buffs;
    std::array<SamplerState *,  sampler_state_size> m_sampler_state_mapps;
    std::array<gl::sync::Fence, sampler_state_size> m_sampler_state_syncs;
    uint                                            m_sampler_state_i;

    // Buffer storing CDF for wavelength sampling at path start
    Spec m_wavelength_distr;
    gl::Buffer m_wavelength_distr_buffer;

  protected:
    virtual const gl::Texture2d4f &render(const Sensor &sensor, const Scene &scene) override; // default-implemented
    virtual void reset(const Sensor &sensor, const Scene &scene) override;

    void advance_sampler_state();
    const gl::Buffer &get_sampler_state();
    const gl::Buffer &get_wavelength_distr() { return m_wavelength_distr_buffer; }

    uint m_iter;
    uint m_spp_max;
    uint m_spp_curr;
    uint m_spp_per_iter;
    uint m_pixel_curr;
    bool m_pixel_checkerboard;

    // Protected constructor to initialize sampler state buffers and maps
    IntegrationRenderPrimitive();

  public: // Getters
    bool has_next_sample_state() const {
      return m_spp_max == 0 || m_spp_curr < m_spp_max;
    }
    bool is_pixel_checkerboard() const { return m_pixel_checkerboard; }
    uint iter()                  const { return m_iter;               }
    uint spp_curr()              const { return m_spp_curr;           }
    uint spp_max()               const { return m_spp_max;            }
  };
} // namespace met::detail