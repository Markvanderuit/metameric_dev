#include <metameric/components/tasks/task_gen_spectral_gamut.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/knn.hpp>
#include <metameric/core/linprog.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/pca.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <small_gl/buffer.hpp>
#include <algorithm>
#include <execution>
#include <ranges>

// TODO remove
#include <metameric/components/views/detail/imgui.hpp>


namespace met {
  GenSpectralGamutTask::GenSpectralGamutTask(const std::string &name)
  : detail::AbstractTask(name) { }
  
  void GenSpectralGamutTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Define flags for creation of a persistent, write-only flushable buffer map
    auto create_flags = gl::BufferCreateFlags::eMapWrite 
                      | gl::BufferCreateFlags::eMapPersistent;
    auto map_flags    = gl::BufferAccessFlags::eMapWrite 
                      | gl::BufferAccessFlags::eMapPersistent
                      | gl::BufferAccessFlags::eMapFlush;
    
    // Define color and spectral gamut buffers
    gl::Buffer color_buffer = {{ .size  = 4 * sizeof(AlColr), .flags = create_flags }};
    gl::Buffer spect_buffer = {{ .size  = 4 * sizeof(Spec), .flags = create_flags }};

    // Prepare buffer maps which are written to **every** frame
    auto color_buffer_map = cast_span<AlColr>(color_buffer.map(map_flags));
    auto spect_buffer_map = cast_span<Spec>(spect_buffer.map(map_flags));

    // Submit resources 
    info.insert_resource("color_buffer", std::move(color_buffer));
    info.insert_resource("spectrum_buffer", std::move(spect_buffer));
    info.insert_resource("color_buffer_map", std::move(color_buffer_map));
    info.insert_resource("spectrum_buffer_map", std::move(spect_buffer_map));
  }

  void GenSpectralGamutTask::dstr(detail::TaskDstrInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &i_color_buffer = info.get_resource<gl::Buffer>("color_buffer");
    auto &i_spect_buffer = info.get_resource<gl::Buffer>("spectrum_buffer");

    // Unmap buffers
    i_color_buffer.unmap();
    i_spect_buffer.unmap();
  }
  
  void GenSpectralGamutTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_app_data         = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &i_color_buffer     = info.get_resource<gl::Buffer>("color_buffer");
    auto &i_spect_buffer     = info.get_resource<gl::Buffer>("spectrum_buffer");
    auto &i_color_buffer_map = info.get_resource<std::span<AlColr>>("color_buffer_map");
    auto &i_spect_buffer_map = info.get_resource<std::span<Spec>>("spectrum_buffer_map");
    auto &e_metamer_mapping  = info.get_resource<MetamerMapping>("gen_spectral_mappings", "metamer_mapping");
    auto &e_mappings         = e_app_data.loaded_mappings;
    auto &e_proj_data        = e_app_data.project_data;
    auto &e_bases            = info.get_resource<BMatrixType>(global_key, "pca_basis");


    // Get relevant application/project data
    auto &knn_grid    = e_app_data.spec_knn_grid;
    auto &color_gamut = e_proj_data.rgb_gamut;
    auto &color_offs  = e_proj_data.rgb_offs;
    auto &spect_gamut = e_proj_data.spec_gamut;
    auto &mapping     = e_app_data.loaded_mappings.at(0);
    
    // Sample spectra at gamut color positions through a PCA solver stel
    // std::transform(std::execution::par_unseq, range_iter(color_gamut), spect_gamut.begin(),
    //   [&](const auto &p) { return generate_spectrum_from_basis(e_pca_bases, mapping.finalize(), p); });

    // TODO Remove
    if (ImGui::Begin("Metamer offset debug")) {
      for (uint i = 0; i < color_offs.size(); ++i) {
        ImGui::SliderFloat3(fmt::format("offs {}", i).c_str(), color_offs[i].data(), -1.f, 1.f);
      }
      ImGui::End();
    }

    // std::array<CMFS, 1> systems = { e_mappings[0].finalize() };
    std::array<CMFS, 2> systems = { e_mappings[0].finalize(), e_mappings[1].finalize() };

    // Generate spectra at gamut color positions
    // std::transform(std::execution::par_unseq, range_iter(color_gamut), spect_gamut.begin(),
    //   [&](const auto &p) { return e_metamer_mapping.generate(p, p + c); });

    #pragma omp parallel for
    for (int i = 0; i < color_gamut.size(); ++i) {
      std::array<Colr, 2> signals = { color_gamut[i], color_gamut[i] + color_offs[i] };
      spect_gamut[i] = generate(e_bases.rightCols(wavelength_bases), systems, signals);
    }

    /* std::transform(std::execution::par_unseq, range_iter(color_gamut), spect_gamut.begin(),
    [&](const auto &p) { 
      // std::array<Colr, 1> signals = { p };
      std::array<Colr, 2> signals = { p, p + c };
      // return generate(e_bases.rightCols(wavelength_bases), { e_mappings[0].finalize() }, { p });
      return generate(e_bases.rightCols(wavelength_bases), systems, signals);
      // return e_metamer_mapping.generate({ p, p + c}); 
    }); */


    // Sample spectra at gamut color positions from the KNN object
    // std::transform(std::execution::par_unseq, range_iter(color_gamut), spect_gamut.begin(),
    //   [&](const auto &p) { return knn_grid.query_1_nearest(p).value; });
    
    // Copy data to gpu buffer maps
    std::ranges::copy(color_gamut, i_color_buffer_map.begin());
    std::ranges::copy(spect_gamut, i_spect_buffer_map.begin());

    // Flush buffers after pushing new data 
    i_color_buffer.flush();
    i_spect_buffer.flush();
  }
} // namespace met
