#include <metameric/core/distribution.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/pipeline_new/task_gen_uplifting_data.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/misc/task_lambda.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/utility.hpp>
#include <omp.h>
#include <execution>
#include <numbers>

// TODO remove
#include <metameric/components/views/task_uplifting_viewer.hpp>

namespace met {
  // Nr. of points on the color system boundary; lower means more space available for constraints
  constexpr uint n_system_boundary_samples = 64;
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  GenUpliftingDataTask:: GenUpliftingDataTask(uint uplifting_i)
  : m_uplifting_i(uplifting_i) { }

  bool GenUpliftingDataTask::is_active(SchedulerHandle &info) {
    met_trace();
    
    // Get external resources
    const auto &e_scene     = info.global("scene").getr<Scene>();
    const auto &e_uplifting = e_scene.components.upliftings[m_uplifting_i];

     // Force on first run, then make dependent on uplifting only
    return is_first_eval() || e_uplifting;
  }

  void GenUpliftingDataTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Initialize buffers to hold packed delaunay tesselation data; these buffers are used by
    // the gen_object_data task to generate barycentric weights for the objects' textures
    info("tesselation_data").init<gl::Buffer>({ .size = sizeof(MeshDataLayout),                             .flags = buffer_create_flags });
    info("tesselation_pack").init<gl::Buffer>({ .size = sizeof(MeshPackLayout) * max_supported_constraints, .flags = buffer_create_flags });
    info("tesselation_coef").init<gl::Buffer>({ .size = sizeof(SpecCoefLayout) * max_supported_constraints, .flags = buffer_create_flags });
    m_tesselation_data_map = info("tesselation_data").getw<gl::Buffer>().map_as<MeshDataLayout>(buffer_access_flags).data();
    m_tesselation_pack_map = info("tesselation_pack").getw<gl::Buffer>().map_as<MeshPackLayout>(buffer_access_flags);
    m_tesselation_coef_map = info("tesselation_coef").getw<gl::Buffer>().map_as<SpecCoefLayout>(buffer_access_flags);

    // Initialize buffer to hold packed spectral data; this buffer is copied over to a texture
    // in <scene.components.upliftings.gl.*> for fast access during rendering
    m_buffer_spec_pack     = {{ .size = sizeof(SpecPackLayout) * max_supported_constraints, .flags = buffer_create_flags  }};
    m_buffer_spec_pack_map = m_buffer_spec_pack.map_as<SpecPackLayout>(buffer_access_flags);

    // Specify spectrum cache, for plotting of generated constraint spectra
    info("constraint_spectra").set<std::vector<Spec>>({});
    info("constraint_coeffs").set<std::vector<Basis::vec_type>>({});

    // Specify draw dispatch, as handle for a potential viewer to render the tesselation
    info("tesselation_draw").set<gl::DrawInfo>({});
    info.task(std::format("uplifting_viewport_{}", m_uplifting_i)).init<UpliftingViewerTask>(m_uplifting_i);
    info.task(std::format("uplifting_debugger_{}", m_uplifting_i)).init<LambdaTask>([&](auto &info) {
      if (ImGui::Begin(std::format("Uplifting data ({})", m_uplifting_i).c_str())) {
        const auto &e_scene = info.global("scene").getr<Scene>();
        const auto &[e_uplifting, e_state] 
          = info.global("scene").getw<Scene>().components.upliftings[m_uplifting_i];
        const auto &e_basis = e_scene.resources.bases[e_uplifting.basis_i].value();

        static bool im_show_all_bases = true;
        static bool im_show_all_bound = true;
        static uint im_basis_i = 0;
        static uint im_bound_i = 0;
        
        ImGui::Checkbox("Show all bases", &im_show_all_bases);
        if (im_show_all_bases) {
          ImGui::PlotSpectra("##basis", { },  e_basis.func.colwise() | rng::to<std::vector<Spec>>(), -1.f, 1.f);
        } else {
          constexpr uint im_basis_min = 0, im_basis_max = wavelength_bases - 1;
          ImGui::SliderScalar("Basis index", ImGuiDataType_U32, &im_basis_i, &im_basis_min, &im_basis_max);
          ImGui::PlotSpectrum("##basis", Spec(e_basis.func.col(im_basis_i)), -1.f, 1.f);
        }
        ImGui::Separator();
        ImGui::Checkbox("Show all boundaries", &im_show_all_bound);
        if (im_show_all_bound) {
          ImGui::PlotSpectra("##bounds", { }, m_csys_boundary_spectra, 0., 1.f);
        } else {
          uint im_bound_min = 0, im_bound_max = m_csys_boundary_spectra.size() - 1;
          ImGui::SliderScalar("Boundary index", ImGuiDataType_U32, &im_bound_i, &im_bound_min, &im_bound_max);
          ImGui::PlotSpectrum("##bounds", m_csys_boundary_spectra[im_bound_i]);
        }
      }
      ImGui::End();
    });
  }

  void GenUpliftingDataTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get const external uplifting; note that mutable caches are accessible
    const auto &e_scene = info.global("scene").getr<Scene>();
    const auto &[e_uplifting, e_state] 
      = info.global("scene").getw<Scene>().components.upliftings[m_uplifting_i];

    // Get shared resources
    const auto &e_csys    = e_scene.components.colr_systems[e_uplifting.csys_i];
    const auto &e_basis   = e_scene.resources.bases[e_uplifting.basis_i];
    const auto &e_objects = e_scene.components.objects;
    const auto &e_meshes  = e_scene.resources.meshes;
    const auto &e_images  = e_scene.resources.images;
    
    // Flag tells if the color system spectra have, in any way, been modified
    bool csys_stale = is_first_eval() || e_state.basis_i || e_basis || e_state.csys_i  || e_csys;

    // Flag tells if the resulting tesselation has, in any way, been modified;
    // this is set to true in step 2 if necessary
    bool tssl_stale = is_first_eval();

    // Color system spectra within which the 'uplifted' texture is defined
    auto csys = e_scene.csys(e_csys);

    // 1. Generate color system boundary (spectra)
    if (csys_stale) {
      m_csys_boundary_coeffs = generate_color_system_ocs_coeffs({ .direct_objective = csys,
                                                                  .basis            = e_basis.value(),
                                                                  .seed             = 4,
                                                                  .n_samples        = n_system_boundary_samples });
      m_csys_boundary_spectra.resize(m_csys_boundary_coeffs.size());
      std::transform(std::execution::par_unseq, 
                     range_iter(m_csys_boundary_coeffs), 
                     m_csys_boundary_spectra.begin(), 
                     [&](const auto &coef) { return e_basis.value()(coef); });

      // m_csys_boundary_spectra = generate_color_system_ocs({ .direct_objective = csys,
      //                                                       .basis            = e_basis.value(),
      //                                                       .seed             = 4,
      //                                                       .n_samples        = n_system_boundary_samples });

      // // For each spectrum, generate coefficients for moment-based representation
      // m_csys_boundary_coeffs.resize(m_csys_boundary_spectra.size());
      // std::transform(std::execution::par_unseq, range_iter(m_csys_boundary_spectra), 
      //                m_csys_boundary_coeffs.begin(), spectrum_to_moments);

      // For each spectrum, add a point to the set of tesselation points for later
      m_tesselation_points.resize(m_csys_boundary_spectra.size() + e_uplifting.verts.size());
      std::transform(std::execution::par_unseq, 
        range_iter(m_csys_boundary_spectra), m_tesselation_points.begin(),
        [&](const Spec &s) { return (csys(s)).eval(); });

      fmt::print("Uplifting color system boundary, {} points\n", m_csys_boundary_spectra.size());
    }

    // Resize internal objects storing vertex positions, and corresponding spectra;
    // total nr. of vertices = boundary vertices + inner constraint vertices
    m_tesselation_points.resize(m_csys_boundary_spectra.size() + e_uplifting.verts.size());
    m_tesselation_spectra.resize(m_csys_boundary_spectra.size() + e_uplifting.verts.size());
    m_tesselation_coeffs.resize(m_csys_boundary_spectra.size() + e_uplifting.verts.size());

    // Copy boundary spectra over to vertex spectra, generate corresponding moment coefficients, and update
    std::copy(std::execution::par_unseq, range_iter(m_csys_boundary_spectra), m_tesselation_spectra.begin());
    std::copy(std::execution::par_unseq, range_iter(m_csys_boundary_coeffs),  m_tesselation_coeffs.begin());

    // 2. Generate constraint spectra
    // Iterate over stale constraint data, and generate corresponding spectra
    #pragma omp parallel for
    for (int i = 0; i < e_uplifting.verts.size(); ++i) {
      // We only generate a spectrum if the specific vertex was changed,
      // or the entire underlying color system has changed
      guard_continue(e_state.verts[i] || csys_stale);
      const auto &e_vert = e_uplifting.verts[i];

      // Generate vertex color, attached metamer, and its originating coefficients
      // this is handled in Scene object to keep it away from the pipeline
      auto [c, s, coef] = e_vert.realize(e_scene, e_uplifting);
      
      // Add to set of spectra and coefficients
      m_tesselation_spectra[m_csys_boundary_spectra.size() + i] = s;
      m_tesselation_coeffs[m_csys_boundary_spectra.size() + i]  = coef;

      // We only update vertices in the tesselation if the 'primary' has updated
      // as otherwise we'd trigger re-tesselation on every constraint modification
      Colr prev_c = m_tesselation_points[m_csys_boundary_spectra.size() + i];
      if (!prev_c.isApprox(c)) {
        m_tesselation_points[m_csys_boundary_spectra.size() + i] = c;
        tssl_stale = true; // skipping atomic, as everyone's just setting to true r.n.
      }
    } // for (int i)

    // 3. Generate color system tesselation
    if (tssl_stale) {
      // Generate new tesselation
      m_tesselation = generate_delaunay<AlDelaunay, Colr>(m_tesselation_points);

      // Update packed data for fast per-object delaunay traversal
      std::transform(std::execution::par_unseq, 
                     range_iter(m_tesselation.elems), 
                     m_tesselation_pack_map.begin(), 
      [&](const auto &el) {
        const auto vts = el | index_into_view(m_tesselation.verts);
        MeshPackLayout pack;
        pack.inv.block<3, 3>(0, 0) = (eig::Matrix3f() 
          << vts[0] - vts[3], vts[1] - vts[3], vts[2] - vts[3]
        ).finished().inverse();
        pack.sub.head<3>() = vts[3];
        return pack;
      });

      // Print size of new tesselation
      fmt::print("Uplifting tesselation: {} verts, {} elems\n", 
        m_tesselation.verts.size(), m_tesselation.elems.size());

      // Update packing layout data
      m_tesselation_data_map->elem_offs = max_supported_constraints * m_uplifting_i;
      m_tesselation_data_map->elem_size = m_tesselation.elems.size();

      // Get writeable buffers and flush changes
      auto &i_tesselation_pack = info("tesselation_pack").getw<gl::Buffer>();
      auto &i_tesselation_data = info("tesselation_data").getw<gl::Buffer>();
      i_tesselation_pack.flush(m_tesselation.elems.size() * sizeof(MeshPackLayout));
      i_tesselation_data.flush();
    } // if (tssl_stale)

    // 4. Modify spectral data on the gl-side
    //    Spectra are always changed, so update them either way
    {
      // Assemble packed spectra into mapped buffer. We pack all four spectra of a tetrahedron
      // into a single interleaved object, for fast 4-component texture sampling during rendering
      for (uint i = 0; i < m_tesselation.elems.size(); ++i) {
        const auto &el = m_tesselation.elems[i];
        
        // Data is transposed and reshaped into a [wvls, 4]-shaped object for gpu-side layout
        SpecPackLayout pack;
        for (uint i = 0; i < 4; ++i)
          pack.col(i) = m_tesselation_spectra[el[i]];
        m_buffer_spec_pack_map[i] = pack.transpose().reshaped(wavelength_samples, 4);

        // Moment coefficients are stored directly
        SpecCoefLayout coeffs;
        for (uint i = 0; i < 4; ++i)
          coeffs.col(i) = m_tesselation_coeffs[el[i]];
        m_tesselation_coef_map[i] = coeffs;
      }

      // Flush changes to GL-side 
      m_buffer_spec_pack.flush();
      info("tesselation_coef").getw<gl::Buffer>().flush();
      
      // Do pixel-buffer copy of packed spectra to sampleable texture
      e_scene.components.upliftings.gl.texture_spectra.set(m_buffer_spec_pack, 0, { wavelength_samples, m_tesselation_data_map->elem_size },
                                                                                  { 0,                  m_tesselation_data_map->elem_offs });
    }
    
    // 5. If a viewer task exists, we should supply mesh data for rendering
    {
      auto viewer_name   = std::format("uplifting_viewport_{}", m_uplifting_i);
      auto viewer_handle = info.task(viewer_name);
      
      if (tssl_stale && viewer_handle.is_init()) {
        // Convert delaunay to triangle mesh
        auto mesh = convert_mesh<AlMesh>(m_tesselation);

        // Push mesh data and generate vertex array; we do a full, expensive, inefficient copy. 
        // The viewer is only for debugging anyways
        m_buffer_viewer_verts = {{ .data = cnt_span<const std::byte>(mesh.verts) }};
        m_buffer_viewer_elems = {{ .data = cnt_span<const std::byte>(mesh.elems) }};
        m_buffer_viewer_array = {{
          .buffers  = {{ .buffer = &m_buffer_viewer_verts, .index = 0, .stride = sizeof(eig::Array4f) }},
          .attribs  = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
          .elements = &m_buffer_viewer_elems
        }};
        
        // Expose dispatch draw information to other tasks
        info("tesselation_draw").set<gl::DrawInfo>({
          .type           = gl::PrimitiveType::eTriangles,
          .vertex_count   = (uint) (m_buffer_viewer_elems.size() / sizeof(uint)),
          .capabilities   = {{ gl::DrawCapability::eDepthTest, true },
                             { gl::DrawCapability::eCullOp,   false }},
          .draw_op        = gl::DrawOp::eLine,
          .bindable_array = &m_buffer_viewer_array
        });
      }
    }

    // 6. Expose a copy of generated constraint spectra for visualization
    {
      auto &i_constraint_spectra = info("constraint_spectra").getw<std::vector<Spec>>();
      auto &i_constraint_coeffs  = info("constraint_coeffs").getw<std::vector<Basis::vec_type>>();
      i_constraint_spectra.resize(e_uplifting.verts.size());      
      rng::copy(m_tesselation_spectra | vws::drop(m_csys_boundary_spectra.size()), 
                i_constraint_spectra.begin());
      i_constraint_coeffs.resize(e_uplifting.verts.size());      
      rng::copy(m_tesselation_coeffs  | vws::drop(m_csys_boundary_coeffs.size()), 
                i_constraint_coeffs.begin());
    }
  }

  Spec GenUpliftingDataTask::query_constraint(uint i) const {
    met_trace();
    return m_tesselation_spectra[m_csys_boundary_spectra.size() + i];
  }

  TetrahedronRecord GenUpliftingDataTask::query_tetrahedron(uint i) const {
    met_trace();

    TetrahedronRecord tr = { .weights = 0.f }; // no weights known

    // Find element indices for this tetrahedron, and then fill per-vertex data
    for (auto [i, elem_i] : enumerate_view(m_tesselation.elems[i])) {
      int j = static_cast<int>(elem_i) - static_cast<int>(m_csys_boundary_spectra.size());
      tr.indices[i] = std::max<int>(j, -1);          // Assign constraint index, or -1 if a constraint is a boundary vertex
      tr.spectra[i] = m_tesselation_spectra[elem_i]; // Assign corresponding spectrum
    }

    return tr;
  }

  TetrahedronRecord GenUpliftingDataTask::query_tetrahedron(const Colr &c) const {
    met_trace();

    float result_err = std::numeric_limits<float>::max();
    uint  result_i = 0;
    auto  result_bary = eig::Vector4f(0.f);

    // Find tetrahedron with positive barycentric weights
    for (uint i = 0; i < m_tesselation.elems.size(); ++i) {
      auto inv = m_tesselation_pack_map[i].inv.block<3, 3>(0, 0).eval();
      auto sub = m_tesselation_pack_map[i].sub.head<3>().eval();

      // Compute barycentric weights using packed element data
      eig::Vector3f xyz  = (inv * (c - sub.array()).matrix()).eval();
      eig::Vector4f bary = (eig::Array4f() << xyz, 1.f - xyz.sum()).finished();

      // Compute squared error of potentially unbounded barycentric weights
      float err = (bary - bary.cwiseMax(0.f).cwiseMin(1.f))
              .dot(bary - bary.cwiseMax(0.f).cwiseMin(1.f));

      // Continue if error does not improve
      guard_continue(err < result_err);

      // Store best result
      result_err  = err;
      result_bary = bary;
      result_i    = i;

      // Exit if zero error is reached
      guard_break(result_err > 0.f);
    }

    // Assign weights to return value
    debug::check_expr(result_i < m_tesselation.elems.size());
    TetrahedronRecord tr = { .weights = result_bary };

    // Find element indices for this tetrahedron, and then fill per-vertex data
    for (auto [i, elem_i] : enumerate_view(m_tesselation.elems[result_i])) {
      int j = static_cast<int>(elem_i) - static_cast<int>(m_csys_boundary_spectra.size());
      tr.indices[i] = std::max<int>(j, -1);          // Assign constraint index, or -1 if a constraint is a boundary vertex
      tr.spectra[i] = m_tesselation_spectra[elem_i]; // Assign corresponding spectrum
    }

    return tr;
  }
} // namespace met