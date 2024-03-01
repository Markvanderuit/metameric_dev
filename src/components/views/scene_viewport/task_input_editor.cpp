#include <metameric/components/views/scene_viewport/task_input_editor.hpp>
#include <metameric/components/pipeline_new/task_gen_uplifting_data.hpp>
#include <oneapi/tbb/concurrent_vector.h>
#include <algorithm>
#include <bitset>
#include <execution>

namespace met {
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
  
  void MeshViewportEditorInputTask::eval_indirect_data(SchedulerHandle &info, const InputSelection &is, IndirectSurfaceConstraint &cstr) {
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

    // Perform path query and obtain path samples
    uint n_spp = 16384;
    m_path_prim.query(m_path_sensor, e_scene, n_spp);
    auto paths = m_path_prim.data();

    // If no paths were found, clear data so the constraint becomes invalid for MMV/spectrum generation
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
    const auto &e_uplf_task = uplf_handles[is.uplifting_i].realize<GenUpliftingDataTask>();

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
                   return si.object.uplifting_i == is.uplifting_i; })            
                 | vws::transform([&](const auto &si) {                          // 4. generate uplifting tetrahedron from uplifting task
                   return e_uplf_task.query_tetrahedron(si.diffuse); })          
                 | vws::transform([&](const auto &tetr) {                        // 5. find index of vertex that is linked to our constraint
                   auto it = rng::find(tetr.indices, is.constraint_i);           //    and return together with tetrahedron
                   uint i =  std::distance(tetr.indices.begin(), it);            
                   return std::pair { tetr, i };  })
                 | vws::filter([&](const auto &pair) {                           // 6. drop tetrahedra that don't touch on our selected constraint
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
      
      /* if (verts.empty()) { 
        // This path is either very unlikely (as first hits of each ray **should** hit the constraint itself)
        // or just completely impossible. Either way, ignoring as it hasn't happened in any scene I've tested 
        // If no constraint vertices were encountered, full path throughput is pushed back
        tbb_paths.push_back(SeparationRecord {
          .power  = 0,
          .wvls   = path.wavelengths,
          .values = path.L
        });
      } else  */
      {
        // Get the relevant constraint's current reflectance
        // at the path's wavelengths (notably, this data is from last frame)
        eig::Array4f r = sample_spectrum(path.wavelengths, e_uplf_task.query_constraint(is.constraint_i));

        // We get the "fixed" part of the path's throughput, by stripping all reflectances of
        // the constrained vertices through division; on div-by-0, we set a component to 0
        eig::Array4f back = path.L;
        rng::for_each(verts, [&](const auto &vt) {
          auto rdiv = (r * vt.r_weight + vt.remainder).eval();
          back = (rdiv != 0.f).select(back / rdiv, 0.f);
        });
        
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
    for (auto &power : cstr.powers)
      power /= static_cast<float>(n_spp);

    // Generate a default color value for which a valid reflectance should exist
    {
      // Get camera cmfs
      CMFS cmfs = e_scene.resources.observers[e_scene.components.observer_i.value].value();
      cmfs = (cmfs.array())
            / (cmfs.array().col(1) * wavelength_ssize).sum();
      cmfs = (models::xyz_to_srgb_transform * cmfs.matrix().transpose()).transpose();
      
      // Reconstruct radiance from truncated power series
      Spec r = e_uplf_task.query_constraint(is.constraint_i);
      Spec s = 0.f;
      for (uint i = 0; i < cstr.powers.size(); ++i)
        s += r.pow(static_cast<float>(i)) * cstr.powers[i];
      
      // Recover output color and store in constraint
      cstr.colr = (cmfs.transpose() * s.matrix());
    }
  }

  bool MeshViewportEditorInputTask::is_active(SchedulerHandle &info) {
    met_trace();
    return info.parent()("is_active").getr<bool>();
  }

  void MeshViewportEditorInputTask::init(SchedulerHandle &info) {
    met_trace();

    // Record selection item; by default no selection
    info("selection").set<InputSelection>(InputSelection::invalid());

    m_ray_prim      = {{ .cache_handle = info.global("cache") }};
    m_path_prim     = {{ .cache_handle = info.global("cache") }};
    m_is_gizmo_used = false;
  }

  void MeshViewportEditorInputTask::eval(SchedulerHandle &info) {
    met_trace();

    // Get handles, shared resources, modified resources
    const auto &e_scene      = info.global("scene").getr<Scene>();
    const auto &e_arcball    = info.relative("viewport_input_camera")("arcball").getr<detail::Arcball>();
    const auto &io           = ImGui::GetIO();
    const auto &is_selection = info("selection").getr<InputSelection>();

    // If window is not active, escape and avoid further input
    guard(ImGui::IsItemHovered());

    // Compute viewport offset and size, minus ImGui's tab bars etc
    eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                                + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
    eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());

    // Generate InputSelection for each relevant constraint
    std::vector<InputSelection> viable_selections;
    for (const auto &[i, comp] : enumerate_view(e_scene.components.upliftings)) {
      const auto &uplifting = comp.value; 
      for (const auto &[j, vert] : enumerate_view(uplifting.verts)) {
        guard_continue(vert.is_active);
        guard_continue(std::holds_alternative<DirectSurfaceConstraint>(vert.constraint)
                    || std::holds_alternative<IndirectSurfaceConstraint>(vert.constraint));
        viable_selections.push_back({ .uplifting_i = i, .constraint_i = j });
      }
    }

    // Gather relevant constraints together with enumeration data
    auto viable_verts = viable_selections | vws::transform([&](InputSelection is) { 
      return std::pair { is, e_scene.components.upliftings[is.uplifting_i].value.verts[is.constraint_i] };
    });

    // Determine nearest constraint to the mouse in screen-space
    InputSelection is_nearest = InputSelection::invalid();
    for (const auto &[is, vert] : viable_verts) {
      // Extract surface information from surface constraint
      auto si = std::visit(overloaded {
        [](const SurfaceConstraint auto &cstr) { return cstr.surface; },
        [](const auto &cstr) { return SurfaceInfo::invalid(); }
      }, vert.constraint);

      // Get screen-space position; test distance and continue if we are too far away
      eig::Vector2f p_screen 
        = eig::world_to_window_space(si.p, e_arcball.full(), viewport_offs, viewport_size);
      guard_continue((p_screen - eig::Vector2f(io.MousePos)).norm() <= selector_near_distance);
      
      // The first surviving constraint is a mouseover candidate
      is_nearest = is;
      break;
    }

    // If a nearest selection is found, show tooltip with the vertex' constraint name
    if (is_nearest.is_valid()) {
      const auto &e_vert = info.global("scene").getr<Scene>()
        .get_uplifting_vertex(is_nearest.uplifting_i, is_nearest.constraint_i);
      ImGui::BeginTooltip();
      ImGui::Text(e_vert.name.c_str());
      ImGui::EndTooltip();
    }

    // On mouse click, and non-use of the gizmo, assign the nearest constraint
    // as the active selection
    if (io.MouseClicked[0] && (!ImGuizmo::IsOver() || !is_selection.is_valid())) {
      info("selection").getw<InputSelection>() = is_nearest;
    }
    
    // Reset variables on lack of active selection
    if (!is_selection.is_valid())
      m_is_gizmo_used = false;

    // On an active selection, draw ImGuizmo
    if (is_selection.is_valid()) {
      // Get readable vertex data
      const auto &e_vert = info.global("scene").getr<Scene>()
        .get_uplifting_vertex(is_selection.uplifting_i, is_selection.constraint_i);
      
      // Extract surface information from surface constraint
      auto si = std::visit(overloaded {
        [](const SurfaceConstraint auto &cstr) { return cstr.surface; },
        [](const auto &) { return SurfaceInfo::invalid(); }
      }, e_vert.constraint);

      // ImGuizmo manipulator operates on transforms
      auto trf_vert  = eig::Affine3f(eig::Translation3f(si.p));
      auto trf_delta = eig::Affine3f::Identity();

      // Specify ImGuizmo enabled operation; transl for one vertex, transl/rotate for several
      ImGuizmo::OPERATION op = ImGuizmo::OPERATION::TRANSLATE;

      // Specify ImGuizmo settings for current viewport and insert the gizmo
      ImGuizmo::SetRect(viewport_offs[0], viewport_offs[1], viewport_size[0], viewport_size[1]);
      ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
      ImGuizmo::Manipulate(e_arcball.view().data(), e_arcball.proj().data(), 
        op, ImGuizmo::MODE::LOCAL, trf_vert.data(), trf_delta.data());

      // Register gizmo use start; cache current vertex position
      if (ImGuizmo::IsUsing() && !m_is_gizmo_used) {
        m_gizmo_prev_si = si;
        m_is_gizmo_used = true;
        
        // Right at the start, we set the indirect constraint set into an invalid state
        // to prevent overhead from constraint generation
        std::visit(overloaded { [&](IndirectSurfaceConstraint &cstr) { 
          cstr.powers.clear();
          cstr.colr = 0.f;
        }, [](auto &cstr) { } }, e_vert.constraint);
      }

      // Register continuous gizmo use
      if (ImGuizmo::IsUsing()) {
        // Apply world-space delta to constraint position
        si.p = trf_delta * si.p;

        // Get screen-space position
        eig::Vector2f p_screen = eig::world_to_screen_space(si.p, e_arcball.full());
        
        // Do a raycast, snapping the world position to the nearest surface
        // on a surface hit, and update the local SurfaceInfo object to accomodate
        m_ray_result = eval_ray_query(info, e_arcball.generate_ray(p_screen));
        si = (m_ray_result.record.is_valid() && m_ray_result.record.is_object())
            ? e_scene.get_surface_info(m_ray_result.get_position(), m_ray_result.record)
            : SurfaceInfo { .p = si.p };

        // Get writable vertex data
        auto &e_vert = info.global("scene").getw<Scene>()
          .get_uplifting_vertex(is_selection.uplifting_i, is_selection.constraint_i);

        // Store world-space position in surface constraint
        std::visit(overloaded { [&](SurfaceConstraint auto &cstr) { 
          cstr.surface = si;
        }, [](const auto &cstr) { } }, e_vert.constraint);
      }

      // Register gizmo use end; apply current vertex position to scene savte state
      if (!ImGuizmo::IsUsing() && m_is_gizmo_used) {
        // Right at the end, we build the indirect surface constraint HERE
        // as it secretly uses previous frame data to fill in some details, and is
        // way more costly per frame than I'd like to admit
        auto vert = e_vert; // copy of vertex
        std::visit(overloaded { [&](IndirectSurfaceConstraint &cstr) { 
          cstr.surface = si;
          eval_indirect_data(info, is_selection, cstr);
        }, [](auto &cstr) { } }, vert.constraint);

        m_is_gizmo_used = false;
        info.global("scene").getw<Scene>().touch({
          .name = "Move surface constraint",
          .redo = [si = si, vert = vert, is = is_selection](auto &scene) {
            auto &e_vert = scene.get_uplifting_vertex(is.uplifting_i, is.constraint_i);
            std::visit(overloaded { 
              [&](IndirectSurfaceConstraint &cstr) {
              cstr = std::get<IndirectSurfaceConstraint>(vert.constraint);
              }, [&](SurfaceConstraint auto &cstr) { 
              cstr.surface = si;
            }, [](const auto &cstr) {}}, e_vert.constraint);
          },
          .undo = [si = m_gizmo_prev_si, is = is_selection](auto &scene) {
            auto &e_vert = scene.get_uplifting_vertex(is.uplifting_i, is.constraint_i);
            std::visit(overloaded { [&](SurfaceConstraint auto &cstr) { 
              cstr.surface = si;
            }, [](const auto &cstr) {}}, e_vert.constraint);
          }
        });
      }
    }
  }

} // namespace met