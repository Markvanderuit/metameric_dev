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
#include <algorithm>
#include <numbers>
#include <execution>

namespace met {
  bool Emitter::operator==(const Emitter &o) const {
    met_trace();
    
    guard(std::tie(type, spec_type, is_active, transform, illuminant_i, illuminant_scale) == 
          std::tie(o.type, o.spec_type, o.is_active, o.transform, o.illuminant_i, o.illuminant_scale),
          false);

    guard(color.index() == o.color.index(), false);
    switch (color.index()) {
      case 0: guard(std::get<Colr>(color).isApprox(std::get<Colr>(o.color)), false); break;
      case 1: guard(std::get<uint>(color) == std::get<uint>(o.color), false); break;
    }

    return true;
  }

  namespace detail {  
    // Helper to pack color/uint variant to a uvec2
    inline
    eig::Array2u pack_material_3f(const std::variant<Colr, uint> &v) {
      met_trace();
      std::array<uint, 2> u;
      if (v.index()) {
        u[0] = std::get<1>(v);
        u[1] = 0x00010000;
      } else {
        Colr c = std::get<0>(v);
        u[0] = detail::pack_half_2x16(c.head<2>());
        u[1] = detail::pack_half_2x16({ c.z(), 0 });
      }
      return { u[0], u[1] };
    }

    inline
    gl::Buffer to_std140(const Distribution &d) {
      met_trace_full();
      std::vector<eig::Array4f> data;
      data.push_back(eig::Array4f(d.sum()));
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
            .flags            = (emitter.is_active ? 0x80000000 : 0)
                              | ((static_cast<uint>(emitter.spec_type) & 0x00FF) <<  8)
                              | ((static_cast<uint>(emitter.type     ) & 0x00FF)      ),
            .illuminant_scale = emitter.illuminant_scale,
            .illuminant_i     = emitter.illuminant_i
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
        auto emitter_cdf = Distribution(cnt_span<float>(emitter_distr));
        emitter_distr_buffer = to_std140(emitter_cdf);

        // Store information on first constant emitter, if one is present and active;
        // we don't support multiple environment emitters (I mean how would that even look...)
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

        // Check if an envmap is present, if it is uplifting an image,
        // and then generate an alias table for sampling
        if (m_envm_info_data->envm_is_present) {
          const auto &em = emitters[m_envm_info_data->envm_i];
          if (em->spec_type == Emitter::SpectrumType::eColr && std::holds_alternative<uint>(em->color)) {
            uint image_i = std::get<uint>(em->color);
            
            // Get envmap image as triplets of floats
            Image image  = scene.resources.images[image_i]->convert({
              .pixel_frmt = Image::PixelFormat::eRGB,
              .pixel_type = Image::PixelType::eFloat,
              .color_frmt = Image::ColorFormat::eLRGB
            });
            
            // Compute per-pixel luminance as weight, weighted by inclination
            std::vector<float> weights(image.size().prod());
            float inclination_factor  = std::numbers::pi_v<float> / static_cast<float>(image.size().y());
            float inclination_summand = .5f * inclination_factor;
            #pragma omp parallel for
            for (int y = 0; y < image.size().y(); ++y) {
              for (int x = 0; x < image.size().x(); ++x) {
                int i = y * image.size().x() + x;
                float inclination = inclination_factor * static_cast<float>(y) + inclination_summand;
                weights[i] = std::sinf(inclination) * luminance(image.data<Colr>()[i]); 
              }
            }

            // Construct alias table over weights
            AliasTable table(weights);

            // Do stuff with the table lol
          }
        }
      }
      
      // Generate sync object for gpu wait
      if (emitters)
        m_fence = gl::sync::Fence(gl::sync::time_ns(1));
    }
  } // namespace detail
} // namespace met