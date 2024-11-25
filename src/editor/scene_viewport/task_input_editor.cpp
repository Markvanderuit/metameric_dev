#include <metameric/core/ranges.hpp>
#include <metameric/scene/detail/query.hpp>
#include <metameric/editor/scene_viewport/task_input_editor.hpp>
#include <algorithm>
#include <bitset>
#include <execution>
#include <numeric>

namespace met {
  static constexpr float selector_near_distance = 12.f;
  static constexpr uint  indirect_query_spp     = 16384; // 65536;

  RayRecord ViewportEditorInputTask::eval_ray_query(SchedulerHandle &info, const Ray &ray) {
    met_trace_full();

    // Prepare sensor buffer
    m_ray_sensor.origin    = ray.o;
    m_ray_sensor.direction = ray.d;
    m_ray_sensor.flush();

    // Run raycast primitive, block for results
    m_ray_prim.query(m_ray_sensor, info.global("scene").getr<Scene>());
    return m_ray_prim.data();
  }

  std::span<const PathRecord> ViewportEditorInputTask::eval_path_query(SchedulerHandle &info, uint spp) {
    met_trace_full();

    // Get handles, shared resources, modified resources
    const auto &e_scene   = info.global("scene").getr<Scene>();
    const auto &e_arcball = info.relative("viewport_input_camera")("arcball").getr<detail::Arcball>();
    const auto &io        = ImGui::GetIO();
    
    // Compute viewport offset and size, minus ImGui's tab bars etc
    eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                               + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
    eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                               - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
                
    // Update pixel sensor
    m_path_sensor.proj_trf  = e_arcball.proj().matrix();
    m_path_sensor.view_trf  = e_arcball.view().matrix();
    m_path_sensor.film_size = viewport_size.cast<uint>();
    m_path_sensor.pixel     = eig::window_to_pixel(io.MousePos, viewport_offs, viewport_size);
    m_path_sensor.flush();               

    // Perform path primitive, block for results
    m_path_prim.query(m_path_sensor, e_scene, spp);
    return m_path_prim.data();
  }

  void ViewportEditorInputTask::build_indirect_constraint(SchedulerHandle &info, const ConstraintRecord &cs, NLinearConstraint &cstr) {
    met_trace_full();

    // Get shared resources
    const auto &e_scene = info.global("scene").getr<Scene>();
    const auto &e_uplft = e_scene.components.upliftings.gl.uplifting_data[cs.uplifting_i];

    // Perform path query and obtain path samples
    // If no paths were found, clear data so the constraint becomes invalid for MMV/spectrum generation
    auto paths = eval_path_query(info, indirect_query_spp);
    if (paths.empty()) {
      cstr.colr_j = 0.f;
      cstr.powr_j = { };
      return;
    }

    fmt::print("Path query sampled {} light paths\n", paths.size());

    // Get the current reflectance distribution used during rendering operations for this constraint
    Spec spec = e_uplft.interior[cs.vertex_i].spec;

    // Helper struct; a path sample is restructured into a collection of r^p * c_p
    struct PowerSeriesSample {
      uint         power;  // nr. of times constraint reflectance appears along path
      eig::Array4f wvls;   // integration wavelengths 
      eig::Array4f values; // remainder of incident radiance, without constraint reflectance multiplied against it
    };

    // Compact paths into R^P + aR', which likely means separating them instead
    std::vector<PowerSeriesSample> power_series_samples;
    power_series_samples.reserve(paths.size());
    #pragma omp parallel for
    for (int i = 0; i < paths.size(); ++i) {
      const PathRecord &path = paths[i];
      
      // Perform a reconstruction of the uplifting behavior along this path, for the path's
      // wavelength, this specific uplifting and this specific constraint only
      auto reconstruction = detail::query_path_reconstruction(e_scene, path, cs);

      // Get the "fixed" part of the path's throughput, by taking the constraint's current refl.
      // and dividing it out of the assembled energy (note; this data is from last frame)
      eig::Array4f back = path.L;
      eig::Array4f refl = sample_spectrum(path.wvls, spec);
      for (const auto &vt : reconstruction) {
        eig::Array4f rdiv = refl * vt.a + vt.remainder; // r_i = a_i * r + w_i makes for a nice rewrite
        back = (rdiv > 0.0001f).select(back / rdiv, back); // we ignore small values to prevent fireflies from mucking up a measurement
      }
      
      // We them iterate all permutations of the current constraint vertices
      // and collapse reflectances into a simple power number, and multiply
      // energy against the remainder reflectances
      for (uint i = 0; i < std::pow(2u, static_cast<uint>(reconstruction.size())); ++i) {
        auto bits = std::bitset<32>(i);
        PowerSeriesSample sr = { .power = 0, .wvls = path.wvls, .values = back };
        for (uint j = 0; j < reconstruction.size(); ++j) {
          if (bits[j]) {
            sr.power++;
            sr.values *= reconstruction[j].a;          // a_i
          } else {
            sr.values *= reconstruction[j].remainder; // w_i
          }
        }

        #pragma omp critical
        {
          power_series_samples.push_back(sr);
        }
      }
    }

    fmt::print("Path query resulted in {} reflectance permutations\n", power_series_samples.size());

    // Make space in constraint available, up to maximum power and set everything to zero for now
    cstr.powr_j.resize(1 + rng::max_element(power_series_samples, {}, &PowerSeriesSample::power)->power);
    rng::fill(cstr.powr_j, Spec(0));

    // Reduce power series samples into their respective power bracket and divide by sample count
    for (const auto &path : power_series_samples)
      accumulate_spectrum(cstr.powr_j[path.power], path.wvls, path.values);
    for (auto &power : cstr.powr_j) {
      power *= .25f * static_cast<float>(wavelength_samples) / static_cast<float>(indirect_query_spp);
      if (power = power.cwiseMax(0.f); (power <= 1e-3).all())
        power = 0.f;
    }

    // Generate a default color value by passing through the newly generated power series constraint
    IndirectColrSystem csys = { .cmfs = e_scene.primary_observer(), .powers = cstr.powr_j };
    cstr.colr_j = csys(spec);
  }

  bool ViewportEditorInputTask::is_active(SchedulerHandle &info) {
    met_trace();
    return info.parent()("is_active").getr<bool>();
  }

  void ViewportEditorInputTask::init(SchedulerHandle &info) {
    met_trace();

    // List of surface constraints that need drawing or are selected
    info("active_constraints").set<std::vector<ConstraintRecord>>({ });
    info("selected_constraint").set<ConstraintRecord>(ConstraintRecord::invalid());

    // Record selection item; by default no selection
    info("selection").set<ConstraintRecord>(ConstraintRecord::invalid());

    m_ray_prim  = {{ .cache_handle = info.global("cache") }};
    m_path_prim = {{ .cache_handle = info.global("cache") }};
  }

  void ViewportEditorInputTask::eval(SchedulerHandle &info) {
    met_trace();

    // Get handles, shared resources, modified resources
    const auto &e_scene   = info.global("scene").getr<Scene>();
    const auto &e_arcball = info.relative("viewport_input_camera")("arcball").getr<detail::Arcball>();
    const auto &io        = ImGui::GetIO();
    const auto &i_cs      = info("selection").getr<ConstraintRecord>();

    // If a constraint was deleted, reset and avoid further input
    if (i_cs.is_valid()) {
      if (e_scene.components.upliftings.is_resized() && !is_first_eval()) {
        info("selection").set(ConstraintRecord::invalid());
        return;
      }
      const auto &e_uplifting = e_scene.components.upliftings[i_cs.uplifting_i];
      if (e_uplifting.state.verts.is_resized() && !is_first_eval()) {
        info("selection").set(ConstraintRecord::invalid());
        return;
      }
    }

    // Determine active constraint vertices for the viewport
    auto &i_active_constraints = info("active_constraints").getw<std::vector<ConstraintRecord>>();
    i_active_constraints.clear();
    for (const auto &[i, uplifting] : enumerate_view(e_scene.components.upliftings)) {
      for (const auto &[j, vert] : enumerate_view(uplifting->verts)) {
        // Only constraints currently being edited are shown
        auto str = fmt::format("scene_components_editor.mmv_editor_{}_{}", i, j);
        guard_continue(info.task(str).is_init());
        
        // Only active surface constraints are shown
        guard_continue(vert.is_active && vert.has_surface());

        // Push back relevant constraints
        i_active_constraints.push_back({ .uplifting_i = i, .vertex_i = j });
      } // for (j, vert)
    } // for (i, comp)

    // If window is not active, escape and avoid further input
    guard(ImGui::IsItemHovered());

    // Gather relevant constraints together with enumeration data
    auto active_verts = i_active_constraints 
                      | vws::transform([&](ConstraintRecord cs) { return std::pair { cs, e_scene.uplifting_vertex(cs) }; });

    // Compute viewport offset and size, minus ImGui's tab bars etc
    eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                               + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
    eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                               - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());

    // Determine nearest constraint to the mouse in screen-space
    ConstraintRecord cs_nearest = ConstraintRecord::invalid();
    for (const auto &[cs, vert] : active_verts) {
      // Extract surface information from surface constraint; assume these are valid at this point
      auto si = vert.surface();

      // Get screen-space position; test distance and continue if we are too far away
      eig::Vector2f p_screen = eig::world_to_window_space(si.p, e_arcball.full(), viewport_offs, viewport_size);
      guard_continue((p_screen - eig::Vector2f(io.MousePos)).norm() <= selector_near_distance);
      
      // The first surviving constraint is a mouseover candidate
      cs_nearest = cs;
      break;
    } // for (cs, vert)

    // If a nearest selection is found, show tooltip with the vertex' constraint name
    if (cs_nearest.is_valid()) {
      const auto &e_vert = info.global("scene").getr<Scene>().uplifting_vertex(cs_nearest);
      ImGui::BeginTooltip();
      ImGui::Text("%s", e_vert.name.c_str());
      ImGui::EndTooltip();
    }

    // On mouse click, and non-use of the gizmo, assign the nearest constraint
    // as the active selection
    if (io.MouseClicked[0] && (!m_gizmo.is_over() || !i_cs.is_valid()))
      info("selection").getw<ConstraintRecord>() = cs_nearest;
    
    // Reset variables on lack of active selection, and return early
    if (!i_cs.is_valid()) {
      m_gizmo.set_active(false);
      return;
    }

    // Extract surface information from scurrent vertex
    const auto &e_vert = e_scene.uplifting_vertex(i_cs);

    // Register gizmo use start; cache current vertex position
    if (m_gizmo.begin_delta(e_arcball, eig::Affine3f(eig::Translation3f(e_vert.surface().p)))) {
      m_gizmo_prev_v = e_vert;
      m_gizmo_curr_p = e_vert.surface();

      // Right at the start, we set the indirect constraint set into an invalid state
      // to prevent overhead from constraint generation
      auto &e_vert = info.global("scene").getw<Scene>().uplifting_vertex(i_cs);
      e_vert.constraint | visit_single([&](IndirectSurfaceConstraint &cstr) { 
        cstr.cstr_j.back().powr_j.clear();
        cstr.cstr_j.back().colr_j = 0.f;
      });
    }

    // Register continuous gizmo use
    if (auto [active, delta] = m_gizmo.eval_delta(); active) {
      // Apply world-space delta to constraint position
      m_gizmo_curr_p.p = (delta * m_gizmo_curr_p.p).eval();

      // Get screen-space position
      eig::Vector2f p_screen = eig::world_to_screen_space(m_gizmo_curr_p.p, e_arcball.full());
      
      // Do a raycast, snapping the world position to the nearest surface
      // on a surface hit, and update the local SurfaceInfo object to accomodate
      m_ray_result = eval_ray_query(info, e_arcball.generate_ray(p_screen));
      auto gizmo_cast_p 
         = (m_ray_result.record.is_valid() && m_ray_result.record.is_object())
         ? detail::query_surface_info(e_scene, m_ray_result)
         : SurfaceInfo { .p = m_gizmo_curr_p.p };

      // Store surface data and extracted color in  constraint
      info.global("scene").getw<Scene>()
          .uplifting_vertex(i_cs).set_surface(gizmo_cast_p);
    }

    // Register gizmo use end; apply current vertex position to scene save state
    if (m_gizmo.end_delta()) {
      // Get screen-space position
      eig::Vector2f p_screen = eig::world_to_screen_space(m_gizmo_curr_p.p, e_arcball.full());

      // Do a raycast, snapping the world position to the nearest surface
      // on a surface hit, and update the local SurfaceInfo object to accomodate
      m_ray_result = eval_ray_query(info, e_arcball.generate_ray(p_screen));
      auto gizmo_cast_p 
         = (m_ray_result.record.is_valid() && m_ray_result.record.is_object())
         ? detail::query_surface_info(e_scene, m_ray_result)
         : SurfaceInfo { .p = m_gizmo_curr_p.p };

      // Right at the end, we build the indirect surface constraint HERE
      // as it secretly uses previous frame data to fill in some details, and is
      // way more costly per frame than I'd like to admit
      auto gizmo_curr_v = e_vert; // copy of vertex
      gizmo_curr_v.set_surface(gizmo_cast_p);
      gizmo_curr_v.constraint | visit_single([&](IndirectSurfaceConstraint &c) { 
          build_indirect_constraint(info, i_cs, c.cstr_j.back());
      });

      // Save result
      info.global("scene").getw<Scene>().touch({
        .name = "Move surface constraint",
        .redo = [v = gizmo_curr_v,   i_cs](auto &scene) { scene.uplifting_vertex(i_cs) = v; },
        .undo = [v = m_gizmo_prev_v, i_cs](auto &scene) { scene.uplifting_vertex(i_cs) = v; }
      });
    }
  }
} // namespace met