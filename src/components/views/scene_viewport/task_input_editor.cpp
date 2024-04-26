#include <metameric/core/matching.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/components/views/scene_viewport/task_input_editor.hpp>
#include <metameric/components/pipeline_new/task_gen_uplifting_data.hpp>
#include <oneapi/tbb/concurrent_vector.h>
#include <algorithm>
#include <bitset>
#include <execution>
#include <numeric>

namespace met {
  static constexpr float selector_near_distance = 12.f;
  static constexpr uint  indirect_query_spp     = 16384;

  RayRecord MeshViewportEditorInputTask::eval_ray_query(SchedulerHandle &info, const Ray &ray) {
    met_trace_full();

    // Prepare sensor buffer
    m_ray_sensor.origin    = ray.o;
    m_ray_sensor.direction = ray.d;
    m_ray_sensor.flush();

    // Run raycast primitive, block for results
    m_ray_prim.query(m_ray_sensor, info.global("scene").getr<Scene>());
    return m_ray_prim.data();
  }

  std::span<const PathRecord> MeshViewportEditorInputTask::eval_path_query(SchedulerHandle &info, uint spp) {
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

  void MeshViewportEditorInputTask::build_indirect_constraint(SchedulerHandle &info, const ConstraintRecord &cs, IndirectSurfaceConstraint &cstr) {
    met_trace_full();

    // Get handles, shared resources, modified resources
    const auto &e_scene = info.global("scene").getr<Scene>();

    // Perform path query and obtain path samples
    // If no paths were found, clear data so the constraint becomes invalid for MMV/spectrum generation
    auto paths = eval_path_query(info, indirect_query_spp);
    if (paths.empty()) {
      cstr.colr   = 0.f;
      cstr.powers = { };
      return;
    }
    fmt::print("Found {} paths through query\n", paths.size());
  
    // Collect handles to all uplifting tasks
    std::vector<TaskHandle> uplf_handles;
    rng::transform(vws::iota(0u, static_cast<uint>(e_scene.components.upliftings.size())), 
      std::back_inserter(uplf_handles), [&](uint i) { return info.task(std::format("gen_upliftings.gen_uplifting_{}", i)); });
    
    // Collect handle to relevant uplifting taslk
    const auto &e_uplf_task = uplf_handles[cs.uplifting_i].realize<GenUpliftingDataTask>();

    // Collect camera cmfs
    CMFS cmfs = e_scene.resources.observers[e_scene.components.observer_i.value].value();
    cmfs = (cmfs.array())
         / (cmfs.array().col(1) * wavelength_ssize).sum();
    cmfs = (models::xyz_to_srgb_transform * cmfs.matrix().transpose()).transpose();

    // Helper structs
    struct SeparationRecord {
      uint         power;  // nr. of times constriant reflectance appears along path
      eig::Array4f wvls;   // integration wavelengths 
      eig::Array4f values; // remainder of incident radiance, without constraint reflectance
    };
    struct CompactTetrRecord {
      float        r_weight;
      eig::Array4f remainder;
    };

    // Compact paths into R^P + aR', which likely means separating them instead
    tbb::concurrent_vector<SeparationRecord> tbb_paths;
    tbb_paths.reserve(paths.size());
    #pragma omp parallel for
    for (int i = 0; i < paths.size(); ++i) {
      const PathRecord &path = paths[i];

      // Filter vertices along path for which the uplifting data is relevant to our current constraint
      auto verts = path.data
                 | vws::take(std::max(static_cast<int>(path.path_depth) - 1, 0)) // 1. drop padded memory
                 | vws::transform([&](const auto &vt) {                          // 2. generate surface info at vertex position
                   return e_scene.get_surface_info(vt.p, vt.record); })          
                 | vws::filter([&](const auto &si) {                             // 3. drop vertices on other uplifting structures
                   return si.object.uplifting_i == cs.uplifting_i; })            
                 | vws::transform([&](const auto &si) {                          // 4. generate uplifting tetrahedron from uplifting task
                   return e_uplf_task.query_tetrahedron(si.diffuse); })          
                 | vws::transform([&](const auto &tetr) {                        // 5. find index of vertex that is linked to our constraint
                   auto it = rng::find(tetr.indices, cs.vertex_i);               //    and return together with tetrahedron
                   uint i  = std::distance(tetr.indices.begin(), it);            
                   return std::pair { tetr, i }; })
                 | vws::filter([&](const auto &pair) {                           // 6. drop tetrahedra that don't touch our selected constraint
                   return pair.second < 4; })                                    
                 | vws::transform([&](const auto &pair) {                        // 7. return compact representation; weight of r and summed remainder
                   auto [tetr, i] = pair;
                   CompactTetrRecord rc = { .r_weight = tetr.weights[i], .remainder = 0.f };
                   for (uint j = 0; j < 4; ++j) {
                     guard_continue(j != i);
                     rc.remainder += tetr.weights[j] * sample_spectrum(path.wavelengths, tetr.spectra[j]);
                   }
                   return rc; 
                 }) | rng::to<std::vector<CompactTetrRecord>>();                 // 8. finalize to vector
      
      {
        // Get the relevant constraint's current reflectance
        // at the path's wavelengths (notably, this data is from last frame)
        eig::Array4f r = sample_spectrum(path.wavelengths, e_uplf_task.query_constraint(cs.vertex_i));
        
        // We get the "fixed" part of the path's throughput, by stripping all reflectances of
        // the constrained vertices through division; on div-by-0, we set a component to 0
        eig::Array4f back = path.L;
        for (const auto &vt : verts) {
          eig::Array4f rdiv = r * vt.r_weight + vt.remainder;
          back = (rdiv > 0.0001f).select(back / rdiv, 0.f); // we clamp small values to zero, to prevent fireflies from mucking up a measurement
        }
        
        // We them iterate all permutations of the current constraint vertices
        // and collapse reflectances into this value
        std::vector<SeparationRecord> permutations;
        uint n_perms = std::pow(2u, static_cast<uint>(verts.size()));
        for (uint i = 0; i < std::pow(2u, static_cast<uint>(verts.size())); ++i) {
          auto bits = std::bitset<32>(i);
          SeparationRecord sr = { .power = 0, .wvls = path.wavelengths, .values = back };
          for (uint j = 0; j < verts.size(); ++j) {
            if (bits[j]) {
              sr.power++;
              sr.values *= verts[j].r_weight;
            } else {
              sr.values *= verts[j].remainder;
            }
          }
          tbb_paths.push_back(sr);
        }
      }
    }

    // Copy tbb over to single vector block
    std::vector<SeparationRecord> paths_finalized(range_iter(tbb_paths));
    fmt::print("Separated into {} path permutations\n", paths_finalized.size());

    // Make space in constraint available, up to maximum power
    // and set everything to zero for now
    cstr.powers.resize(1 + rng::max_element(paths_finalized, {}, &SeparationRecord::power)->power);
    rng::fill(cstr.powers, Spec(0));

    // Reduce spectral data into its respective power bracket and divide by sample count
    // TODO parallel reduction
    for (const auto &path : paths_finalized)
      accumulate_spectrum(cstr.powers[path.power], path.wvls, path.values);
    for (auto &power : cstr.powers) {
      power *= 0.25f * static_cast<float>(wavelength_samples);
      power /= static_cast<float>(indirect_query_spp);
      power = power.cwiseMax(0.f);
      if ((power <= 5e-3).all())
        power = 0.f;
    }

    for (auto &power : cstr.powers) {
      fmt::print("{}\n", power);
    }

    // Obtain underlying reflectance
    Spec r = e_uplf_task.query_constraint(cs.vertex_i);
    
    // Generate a default color value by passing through this reflectance
    IndirectColrSystem csys = {
      .cmfs   = e_scene.resources.observers[e_scene.components.observer_i.value].value(),
      .powers = cstr.powers
    };
    cstr.colr = csys(r);
  }

  bool MeshViewportEditorInputTask::is_active(SchedulerHandle &info) {
    met_trace();
    return info.parent()("is_active").getr<bool>();
  }

  void MeshViewportEditorInputTask::init(SchedulerHandle &info) {
    met_trace();

    // List of surface constraints that need drawing or are selected
    info("active_constraints").set<std::vector<ConstraintRecord>>({ });
    info("selected_constraint").set<ConstraintRecord>(ConstraintRecord::invalid());

    // Record selection item; by default no selection
    info("selection").set<ConstraintRecord>(ConstraintRecord::invalid());

    m_ray_prim  = {{ .cache_handle = info.global("cache") }};
    m_path_prim = {{ .cache_handle = info.global("cache") }};
  }

  void MeshViewportEditorInputTask::eval(SchedulerHandle &info) {
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
    for (const auto &[i, comp] : enumerate_view(e_scene.components.upliftings)) {
      const auto &uplifting = comp.value; 
      for (const auto &[j, vert] : enumerate_view(uplifting.verts)) {
        // Only constraints currently being edited are shown
        auto str = std::format("scene_components_editor.mmv_editor_{}_{}", i, j);
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
      // Extract surface information from surface constraint
      auto si = vert.constraint | visit {
        [](const is_surface_constraint auto &cstr) { return cstr.surface; },
        [](const auto &cstr) { return SurfaceInfo::invalid(); }
      };

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
      ImGui::Text(e_vert.name.c_str());
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
    auto si = e_vert.surface();

    // Register gizmo use start; cache current vertex position
    if (m_gizmo.begin_delta(e_arcball, eig::Affine3f(eig::Translation3f(si.p)))) {
      m_gizmo_prev = e_scene.uplifting_vertex(i_cs);

      // Right at the start, we set the indirect constraint set into an invalid state
      // to prevent overhead from constraint generation
      auto &e_vert = info.global("scene").getw<Scene>().uplifting_vertex(i_cs);
      e_vert.constraint | visit_single([&](IndirectSurfaceConstraint &cstr) { 
        cstr.powers.clear();
        cstr.colr = 0.f;
      });
    }

    // Register continuous gizmo use
    if (auto [active, delta] = m_gizmo.eval_delta(); active) {
      // Apply world-space delta to constraint position
      si.p = delta * si.p;

      // Get screen-space position
      eig::Vector2f p_screen = eig::world_to_screen_space(si.p, e_arcball.full());
      
      // Do a raycast, snapping the world position to the nearest surface
      // on a surface hit, and update the local SurfaceInfo object to accomodate
      m_ray_result = eval_ray_query(info, e_arcball.generate_ray(p_screen));
      si = (m_ray_result.record.is_valid() && m_ray_result.record.is_object())
         ? e_scene.get_surface_info(m_ray_result.get_position(), m_ray_result.record)
         : SurfaceInfo { .p = si.p };

      // Store surface data and extracted color in  constraint
      auto &vert = info.global("scene").getw<Scene>().uplifting_vertex(i_cs);
      vert.constraint | visit_single { [&](is_surface_constraint auto &cstr) { cstr.surface = si;     }};
      vert.constraint | visit_single { [&](is_colr_constraint auto &cstr) { cstr.colr_i = si.diffuse; }};
    }

    // Register gizmo use end; apply current vertex position to scene save state
    if (m_gizmo.end_delta()) {
      // Right at the end, we build the indirect surface constraint HERE
      // as it secretly uses previous frame data to fill in some details, and is
      // way more costly per frame than I'd like to admit
      auto vert = e_vert; // copy of vertex
      vert.constraint | visit_single([&](IndirectSurfaceConstraint &cstr) { 
          cstr.surface = si;
          build_indirect_constraint(info, i_cs, cstr);
      });

      // Save result
      info.global("scene").getw<Scene>().touch({
        .name = "Move surface constraint",
        .redo = [vert = vert, i_cs](auto &scene) { scene.uplifting_vertex(i_cs) = vert; },
        .undo = [vert = m_gizmo_prev, i_cs](auto &scene) { scene.uplifting_vertex(i_cs) = vert; }
      });
    }
  }
} // namespace met