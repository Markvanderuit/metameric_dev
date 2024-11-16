#include <metameric/core/components/emitter.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/scene.hpp>

namespace met::detail {
  SceneGLHandler<met::Emitter>::SceneGLHandler() {
    met_trace_full();

    // Alllocate up to a number of objects and obtain writeable/flushable mapping
    // for regular emitters and an envmap
    std::tie(emitter_info, m_em_info_map) = gl::Buffer::make_flusheable_object<EmBufferLayout>();
    std::tie(emitter_envm_info, m_envm_info_data) = gl::Buffer::make_flusheable_object<EnvBufferLayout>();
  }

  void SceneGLHandler<met::Emitter>::update(const Scene &scene) {
    met_trace_full();
    
    const auto &emitters = scene.components.emitters;
    guard(!emitters.empty() && emitters);

    // Set appropriate component count, then flush change to buffer
    m_em_info_map->size = static_cast<uint>(emitters.size());
    emitter_info.flush(sizeof(uint));

    // Write updated components to mapping
    for (uint i = 0; i < emitters.size(); ++i) {
      const auto &[emitter, state] = emitters[i];
      guard_continue(state);

      m_em_info_map->data[i] = {
        .trf              = emitter.transform.affine().matrix(),
        .type             = static_cast<uint>(emitter.type),
        .is_active        = emitter.is_active,
        .illuminant_i     = emitter.illuminant_i,
        .illuminant_scale = emitter.illuminant_scale
      };

      // Flush change to buffer; most changes to objects are local,
      // so we flush specific regions instead of the whole
      emitter_info.flush(sizeof(EmBlockLayout), sizeof(EmBlockLayout) * i + sizeof(uint));
    } // for (uint i)

    /* // Build per-wavelength sampling distributions over emitter's
    // relative spectral output, weighted by approximate emitter surface area
    eig::Matrix<float, met_max_emitters, wavelength_samples> spectral_distr = 0.f;
    #pragma omp parallel for
    for (int i = 0; i < emitters.size(); ++i) {
      const auto &[emitter, state] = emitters[i];
      guard_continue(emitter.is_active);

      // Get relative spectral output of emitter
      Spec s = scene.resources.illuminants[emitter.illuminant_i].value() 
             * emitter.illuminant_scale;
      
      // Multiply by either approx. surface area, or hemisphere
      switch (emitter.type) {
        case Emitter::Type::eSphere:
          s *= 
            .5f *
            4.f * std::numbers::pi_v<float> * std::pow(emitter.transform.scaling.x(), 2.f);
          break;
        case Emitter::Type::eRect:
          s *= 
            emitter.transform.scaling.x() * emitter.transform.scaling.y();
          break;
        default:
          s *= 2.f * std::numbers::pi_v<float>; // half a hemisphere
          break;
      } 

      spectral_distr.row(i) = s;
    }
    
    auto array_distr = DistributionArray<wavelength_samples>(cnt_span<float>(spectral_distr));   
    emitter_distr_buffer = array_distr.to_buffer_std140(); */

    // Build sampling distribution over emitter's relative output
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

    auto distr = Distribution(cnt_span<float>(emitter_distr));   
    emitter_distr_buffer = distr.to_buffer_std140();

    // Store information on first co  nstant emitter, if one is present and active;
    // we don't support multiple environment emitters
    m_envm_info_data->envm_is_present = false;
    for (uint i = 0; i < emitters.size(); ++i) {
      const auto &[emitter, state] = emitters[i];
      if (emitter.is_active && emitter.type == Emitter::Type::eConstant) {
        m_envm_info_data->envm_is_present = true;
        m_envm_info_data->envm_i          = i;
        break;
      }
    }
    emitter_envm_info.flush();
  }
} // namespace met::detail