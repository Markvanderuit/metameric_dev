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

#include <metameric/scene/scene.hpp>
#include <metameric/core/distribution.hpp>
#include <metameric/core/ranges.hpp>
#include <numbers>

namespace met {
  namespace detail {  
    inline
    gl::Buffer to_std140(const Distribution &d) {
      met_trace_full();
      std::vector<eig::Array4f> data;
      data.push_back(eig::Array4f(d.inv_sum()));
      rng::copy(d.data_func(), std::back_inserter(data));
      rng::copy(d.data_cdf(), std::back_inserter(data));
      return gl::Buffer {{ .data = cnt_span<const std::byte>(data) }};    
    }
    
    SceneGLHandler<met::Emitter>::SceneGLHandler() {
      met_trace_full();

      // Alllocate up to a number of objects and obtain writeable/flushable mapping
      // for regular emitters and an envmap
      std::tie(emitter_info, m_emitter_info_map) = gl::Buffer::make_flusheable_object<BufferLayout>();
      std::tie(emitter_envm_info, m_envm_info_data) = gl::Buffer::make_flusheable_object<EnvBufferLayout>();
    }

    void SceneGLHandler<met::Emitter>::update(const Scene &scene) {
      met_trace_full();
      
      // Destroy old sync object
      m_fence = { };

      // Get relevant resources
      const auto &emitters = scene.components.emitters;

      // Skip entirely; no emitters
      guard(!emitters.empty());
      if (emitters) {
        // Set emitter count
        m_emitter_info_map->n = static_cast<uint>(emitters.size());
        
        // Set per-emitter data
        for (uint i = 0; i < emitters.size(); ++i) {
          const auto &[emitter, state] = emitters[i];
          guard_continue(state);

          m_emitter_info_map->data[i] = {
            .trf              = emitter.transform.affine().matrix(),
            .is_active        = emitter.is_active,
            .type             = static_cast<uint>(emitter.type),
            .illuminant_data  = emitter.illuminant_i,
            .illuminant_scale = emitter.illuminant_scale
          };
        } // for (uint i)

        // Write out changes to buffer
        emitter_info.flush(sizeof(eig::Array4u) + emitters.size() * sizeof(BlockLayout));
        gl::sync::memory_barrier(gl::BarrierFlags::eBufferUpdate       |  
                                  gl::BarrierFlags::eUniformBuffer     |
                                  gl::BarrierFlags::eClientMappedBuffer);
      }

      // Build sampling distribution over emitter's relative output
      if (emitters) {
        std::vector<float> emitter_distr(met_max_emitters, 0.f);
        #pragma omp parallel for
        for (int i = 0; i < emitters.size(); ++i) {
          const auto &[emitter, state] = emitters[i];
          guard_continue(emitter.is_active);

          // Get corresponding observer luminance for output spd under xyz/d65
          Spec  s = scene.resources.illuminants[emitter.illuminant_i].value() * emitter.illuminant_scale;
          float w = luminance(ColrSystem { .cmfs = models::cmfs_cie_xyz, .illuminant = models::emitter_cie_d65 }(s));

          // Multiply by either approx. surface area, or hemisphere
          switch (emitter.type) {
            case Emitter::Type::eSphere:
              w *= 
                .5f *
                4.f * std::numbers::pi_v<float> * std::pow(emitter.transform.scaling.x(), 2.f);
              break;
            case Emitter::Type::eRect:
              w *= 
                emitter.transform.scaling.x() * emitter.transform.scaling.y();
              break;
            default:
              w *= 2.f * std::numbers::pi_v<float>; // half a hemisphere
              break;
          }

          emitter_distr[i] = w;
        }

        // Generate sampling distribution and push to gpu
        emitter_distr_buffer = to_std140(Distribution(cnt_span<float>(emitter_distr)));

        // Store information on first co  nstant emitter, if one is present and active;
        // we don't support multiple environment emitters
        m_envm_info_data->envm_is_present = false;
        for (uint i = 0; i < emitters.size(); ++i) {
          const auto &[emitter, state] = emitters[i];
          if (emitter.is_active && emitter.type == Emitter::Type::eEnviron) {
            m_envm_info_data->envm_is_present = true;
            m_envm_info_data->envm_i          = i;
            break;
          }
        }
        emitter_envm_info.flush();
      }

      // Generate sync object for gpu wait
      if (emitters)
        m_fence = gl::sync::Fence(gl::sync::time_ns(1));
    }
  } // namespace detail
} // namespace met