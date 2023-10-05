#include <metameric/core/scene.hpp>
#include <metameric/components/views/mesh_viewport/task_draw_uplifted.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/misc/detail/scene.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>

// TODO remove
#include <metameric/components/views/detail/imgui.hpp>
#include <implot.h>
#include <format>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  bool MeshViewportDrawUpliftedTask::is_active(SchedulerHandle &info) {
    met_trace();
    return info.relative("viewport_begin")("is_active").getr<bool>()
       && !info.global("scene").getr<Scene>().components.objects.empty();
  }

  void MeshViewportDrawUpliftedTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Initialize program object
    m_program = {{ .type       = gl::ShaderType::eVertex,
                   .spirv_path = "resources/shaders/views/draw_mesh.vert.spv",
                   .cross_path = "resources/shaders/views/draw_mesh.vert.json" },
                 { .type       = gl::ShaderType::eFragment,
                   .spirv_path = "resources/shaders/views/draw_mesh_uplifted.frag.spv",
                   .cross_path = "resources/shaders/views/draw_mesh_uplifted.frag.json" }};

    // Initialize uniform camera buffer and corresponding mapping
    m_unif_camera_buffer     = {{ .size = sizeof(UnifCameraLayout), .flags = buffer_create_flags }};
    m_unif_camera_buffer_map = m_unif_camera_buffer.map_as<UnifCameraLayout>(buffer_access_flags).data();

    // Initialize draw object
    m_draw = { 
      .type             = gl::PrimitiveType::eTriangles,
      .capabilities     = {{ gl::DrawCapability::eMSAA,      true },
                           { gl::DrawCapability::eDepthTest, true },
                           { gl::DrawCapability::eCullOp,    true }},
      .draw_op          = gl::DrawOp::eFill,
      .bindable_program = &m_program,
    };
  }
    
  void MeshViewportDrawUpliftedTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources 
    const auto &e_scene     = info.global("scene").getr<Scene>();
    const auto &e_objects   = e_scene.components.objects;
    const auto &e_arcball   = info.relative("viewport_input")("arcball").getr<detail::Arcball>();
    const auto &e_objc_data = info("scene_handler", "objc_data").getr<detail::RTObjectData>();
    const auto &e_mesh_data = info("scene_handler", "mesh_data").getr<detail::RTMeshData>();
    const auto &e_txtr_data = info("scene_handler", "txtr_data").getr<detail::RTTextureData>();
    const auto &e_uplf_data = info("scene_handler", "uplf_data").getr<detail::RTUpliftingData>();

    static float wvl = 0.5f;
    if (ImGui::Begin("Test slider")) {
      ImGui::SliderFloat("Wavelength", &wvl, 0.f, 1.f);
    }
    ImGui::End();

    // Push camera matrix to uniform data
    m_unif_camera_buffer_map->camera_matrix = e_arcball.full().matrix();
    m_unif_camera_buffer_map->wvl = wvl;
    m_unif_camera_buffer.flush();
    
    // Set fresh vertex array for draw data if it was updated
    if (is_first_eval() || info("scene_handler", "mesh_data").is_mutated())
      m_draw.bindable_array = &e_mesh_data.array;

    // Assemble appropriate draw data for each object in the scene
    if (is_first_eval() || info("scene_handler", "objc_data").is_mutated()) {
      m_draw.commands.resize(e_objects.size());
      rng::transform(e_objects, m_draw.commands.begin(), [&](const auto &comp) {
        guard(comp.value.is_active, gl::MultiDrawInfo::DrawCommand { });
        const auto &e_mesh_info = e_mesh_data.info.at(comp.value.mesh_i);
        return gl::MultiDrawInfo::DrawCommand {
          .vertex_count = e_mesh_info.elems_size * 3,
          .vertex_first = e_mesh_info.elems_offs * 3
        };
      });
    }

    // Bind required resources to their corresponding targets
    m_program.bind("b_unif",         m_unif_camera_buffer);
    m_program.bind("b_buff_objects", e_objc_data.info_gl);
    m_program.bind("b_buff_uplifts", e_uplf_data.info_gl);
    if (e_txtr_data.info_gl.is_init())
      m_program.bind("b_buff_textures", e_txtr_data.info_gl);
    if (e_txtr_data.atlas_1f.texture().is_init())
      m_program.bind("b_txtr_1f", e_txtr_data.atlas_1f.texture());
    if (e_txtr_data.atlas_3f.texture().is_init())
      m_program.bind("b_txtr_3f", e_txtr_data.atlas_3f.texture());
    if (e_objc_data.atlas_4f.texture().is_init())
      m_program.bind("b_uplf_4f", e_objc_data.atlas_4f.texture());
    if (e_uplf_data.spectra_gl_texture.is_init())
      m_program.bind("b_spec_4f", e_uplf_data.spectra_gl_texture);

    // Dispatch draw call to handle entire scene
    gl::dispatch_multidraw(m_draw);

    // Spectrum debugger
    {
      const auto &i_spectra = info("gen_upliftings.gen_uplifting_0", "spectra").getr<std::vector<Spec>>();

      static uint wavelength_index = 0;
      constexpr static auto plot_flags = ImPlotFlags_NoFrame | ImPlotFlags_NoMenus;
      constexpr static auto plot_y_axis_flags = ImPlotAxisFlags_NoDecorations;
      constexpr static auto plot_x_axis_flags = ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoGridLines;

      auto window_title = std::format("Spectrum selector {}", 0);
      if (ImGui::Begin(window_title.c_str())) {
        uint minv = 0;
        uint maxv = std::max(i_spectra.size(), 1ull) - 1ull;
        ImGui::SliderScalar("Spectrum index", ImGuiDataType_U32, &wavelength_index, &minv, &maxv);

        // Get wavelength values for x-axis in plots
        Spec x_values;
        for (uint i = 0; i < x_values.size(); ++i)
          x_values[i] = wavelength_at_index(i);
        
        // Get wavelength values for y-axis in plot
        Spec s = i_spectra.at(wavelength_index);
        
        // Spawn implot
        if (ImPlot::BeginPlot("Spectrum", { 0., 0.f }, plot_flags)) {
          ImPlot::SetupAxes("Wavelength", "Value", plot_x_axis_flags, plot_y_axis_flags);
          ImPlot::PlotLine("##plot_line", x_values.data(), s.data(), wavelength_samples);
          ImPlot::PlotShaded("##plot_line", x_values.data(), s.data(), wavelength_samples);
          ImPlot::EndPlot();
        }
      }
      ImGui::End();
    }
  }
} // namespace met