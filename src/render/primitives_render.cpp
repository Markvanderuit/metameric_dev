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

#include <metameric/core/distribution.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/render/primitives_render.hpp>

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

    IntegrationRenderPrimitive::IntegrationRenderPrimitive()
    : m_sampler_state_i(0) {
      met_trace_full();
      for (uint i = 0; i < m_sampler_state_buffs.size(); ++i) {
        auto &buff = m_sampler_state_buffs[i];
        auto &mapp = m_sampler_state_mapps[i];
        std::tie(m_sampler_state_buffs[i], m_sampler_state_mapps[i])
          = gl::Buffer::make_flusheable_object<SamplerState>();
      }
    }

    void IntegrationRenderPrimitive::reset(const Sensor &sensor, const Scene &scene) {
      met_trace_full();

      // Reset iter counter
      m_iter = 0;

      // Reset current sample count
      m_spp_curr   = 0;
      m_pixel_curr = 0;
      
      // Push sample count to next available buffer and add sync object for flush operation
      m_sampler_state_i = (m_sampler_state_i + 1) % m_sampler_state_buffs.size();
      m_sampler_state_mapps[m_sampler_state_i]->spp_per_iter       = m_spp_per_iter;
      m_sampler_state_mapps[m_sampler_state_i]->spp_curr           = m_spp_curr;
      m_sampler_state_mapps[m_sampler_state_i]->pixel_curr         = m_pixel_curr;
      m_sampler_state_mapps[m_sampler_state_i]->pixel_checkerboard = m_pixel_checkerboard;
      m_sampler_state_buffs[m_sampler_state_i].flush();
      m_sampler_state_syncs[m_sampler_state_i] = gl::sync::Fence(gl::sync::time_s(1));

      // Generate sampling distribution for wavelengths
      {
        // Get scene observer, and list of (scaled) active emitter SPDs in the scene
        CMFS observer = scene.primary_observer();
        auto emitters = scene.components.emitters
                      | vws::filter([](const auto &comp) { return comp.value.is_active; });
        auto illums = emitters
                    | vws::transform([ ](const auto &comp) { return comp.value.illuminant_i; })
                    | vws::transform([&](uint i) { return *scene.resources.illuminants[i];   })
                    | view_to<std::vector<Spec>>();
        auto scales = emitters
                    | vws::transform([](const auto &comp) { return comp.value.illuminant_scale; })
                    | view_to<std::vector<float>>();
    
        // Generate average over scaled distributions
        m_wavelength_distr = 1.f;
        if (!illums.empty()) {
          m_wavelength_distr = 0.f;
          for (uint i = 0; i < illums.size(); ++i)
            m_wavelength_distr += illums[i] * scales[i];
        }
        m_wavelength_distr /= m_wavelength_distr.maxCoeff();

        // Scale by observer, then add defensive sampling for very small values
        m_wavelength_distr *= observer.array().rowwise().sum();
        m_wavelength_distr += (Spec(1) - m_wavelength_distr) * 0.01f;
      }
      
      // Push sampling distribution to buffer
      m_wavelength_distr_buffer = to_std140(Distribution(cnt_span<float>(m_wavelength_distr)));
    }

    void IntegrationRenderPrimitive::advance_sampler_state() {
      met_trace_full();

      m_iter++;

      if (m_pixel_checkerboard) {
        // Advance current sample count every two iterations
        m_pixel_curr = (m_pixel_curr + 1) % 2;
        if (m_pixel_curr == 0)
          m_spp_curr += m_spp_per_iter;
      } else {
        // Advance current sample count by previous nr. of taken samples
        m_spp_curr += m_spp_per_iter;
      }

      // Push sample count to next available buffer and add sync object for flush operation
      m_sampler_state_i = (m_sampler_state_i + 1) % m_sampler_state_buffs.size();
      m_sampler_state_mapps[m_sampler_state_i]->spp_per_iter       = m_spp_per_iter;
      m_sampler_state_mapps[m_sampler_state_i]->spp_curr           = m_spp_curr;
      m_sampler_state_mapps[m_sampler_state_i]->pixel_curr         = m_pixel_curr;
      m_sampler_state_mapps[m_sampler_state_i]->pixel_checkerboard = m_pixel_checkerboard;
      m_sampler_state_buffs[m_sampler_state_i].flush();
      m_sampler_state_syncs[m_sampler_state_i] = gl::sync::Fence(gl::sync::time_s(1));
    }

    const gl::Buffer & IntegrationRenderPrimitive::get_sampler_state() {
      met_trace_full();

      // Block if flush operation has not completed
      if (auto &sync = m_sampler_state_syncs[m_sampler_state_i]; sync.is_init())
        sync.cpu_wait();
      
      return m_sampler_state_buffs[m_sampler_state_i];
    }

    const gl::Texture2d4f & IntegrationRenderPrimitive::render(const Sensor &sensor, const Scene &scene) {
      met_trace();
      return m_film;
    }
  } // namespace detail
  
  PathRenderPrimitive::PathRenderPrimitive(PathRenderPrimitiveInfo info)
  : detail::IntegrationRenderPrimitive(),
    m_cache_handle(info.cache_handle) {
    met_trace_full();

    // Initialize program object, if it doesn't yet exist
    std::tie(m_cache_key, std::ignore) = m_cache_handle.getw<gl::ProgramCache>().set({ 
      .type       = gl::ShaderType::eCompute,
      .glsl_path  = "shaders/render/primitive_render_path.comp",
      .spirv_path = "shaders/render/primitive_render_path.comp.spv",
      .cross_path = "shaders/render/primitive_render_path.comp.json",
      .spec_const = {{ 0u, info.max_depth          },
                     { 1u, info.rr_depth           },
                     { 2u, info.enable_alpha       },
                     { 3u, uint(info.enable_debug) }}
    });

    // Assign sampler configuration
    m_iter               = 0;
    m_spp_curr           = 0;
    m_spp_max            = info.spp_max;
    m_spp_per_iter       = info.spp_per_iter;
    m_pixel_curr         = 0;
    m_pixel_checkerboard = info.pixel_checkerboard;

    // Linear texture sampler
    m_sampler = {{ .min_filter = gl::SamplerMinFilter::eLinear, .mag_filter = gl::SamplerMagFilter::eLinear }};
  }

  void PathRenderPrimitive::reset(const Sensor &sensor, const Scene &scene) {
    met_trace_full();
    IntegrationRenderPrimitive::reset(sensor, scene);

    // Rebuild target texture if necessary
    if (!m_film.is_init() || !m_film.size().isApprox(sensor.film_size)) {
      m_film = {{ .size = sensor.film_size.max(1).eval() }};

      // Set dispatch size
      if (m_pixel_checkerboard) {
        auto dispatch_ndiv  = ceil_div((m_film.size() / eig::Array2u(2, 1)).eval(), 16u);
        m_dispatch.groups_x = dispatch_ndiv.x();
        m_dispatch.groups_y = dispatch_ndiv.y();
      } else {
        auto dispatch_ndiv  = ceil_div(m_film.size(), 16u);
        m_dispatch.groups_x = dispatch_ndiv.x();
        m_dispatch.groups_y = dispatch_ndiv.y();
      }
    }

    // Set film to black
    m_film.clear();
  }

  const gl::Texture2d4f &PathRenderPrimitive::render(const Sensor &sensor, const Scene &scene) {
    met_trace_full();
    
    // If the film object is stale, run a reset()
    if (!m_film.is_init() || !m_film.size().isApprox(sensor.film_size))
      reset(sensor, scene);

    // Return early if sample count has reached specified maximum
    guard(has_next_sample_state(), m_film);

    // Draw relevant program from cache
    auto &program = m_cache_handle.getw<gl::ProgramCache>().at(m_cache_key);

    // Bind required resources to their corresponding targets
    program.bind();
    program.bind("b_film",                m_film);
    program.bind("b_buff_sensor_info",    sensor.buffer());
    program.bind("b_buff_sampler_state",  get_sampler_state());
    program.bind("b_buff_object_info",    scene.components.objects.gl.object_info);
    program.bind("b_buff_emitter_info",   scene.components.emitters.gl.emitter_info);
    program.bind("b_buff_envmap_info",    scene.components.emitters.gl.emitter_envm_info);
    program.bind("b_buff_coef_info",      scene.components.upliftings.gl.texture_coef.buffer());
    program.bind("b_buff_brdf_info",      scene.components.objects.gl.texture_brdf.buffer());
    program.bind("b_buff_wvls_distr",     get_wavelength_distr());
    program.bind("b_buff_emitters_distr", scene.components.emitters.gl.emitter_distr_buffer);
    program.bind("b_illm_1f",             scene.resources.illuminants.gl.spec_texture, m_sampler);
    program.bind("b_bsis_1f",             scene.components.upliftings.gl.texture_basis, m_sampler);
    program.bind("b_brdf_2f",             scene.components.objects.gl.texture_brdf.texture(), m_sampler);
    program.bind("b_coef_4f",             scene.components.upliftings.gl.texture_coef.texture(), m_sampler);
    program.bind("b_cmfs_3f",             scene.resources.observers.gl.cmfs_texture, m_sampler);
    
    if (!scene.resources.meshes.empty()) {
      program.bind("b_buff_blas_info", scene.resources.meshes.gl.blas_info);
      program.bind("b_buff_blas_node", scene.resources.meshes.gl.blas_nodes);
      program.bind("b_buff_blas_prim", scene.resources.meshes.gl.blas_prims);
    }

    // Dispatch compute shader
    gl::sync::memory_barrier( gl::BarrierFlags::eImageAccess   | gl::BarrierFlags::eTextureFetch  |
                              gl::BarrierFlags::eUniformBuffer | gl::BarrierFlags::eStorageBuffer | 
                              gl::BarrierFlags::eBufferUpdate                                     );
    gl::dispatch_compute(m_dispatch);

    // Advance sampler state for next render() call
    advance_sampler_state();

    return m_film;
  }
} // namespace met