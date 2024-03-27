#include <metameric/core/components.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/scene.hpp>

namespace met {
  namespace detail {
    // Helper for surface() calls; invalid return for non-surface constraints
    static SurfaceInfo invalid_visitor_return_si = SurfaceInfo::invalid();
  } // namespace detail
 
  bool Object::operator==(const Object &o) const {
    guard(std::tie(is_active, transform, mesh_i, uplifting_i) == 
          std::tie(o.is_active, o.transform, o.mesh_i, o.uplifting_i), false);
    /* guard(std::tie(roughness, metallic, opacity) == 
          std::tie(o.roughness, o.metallic, o.opacity), false); */
    guard(diffuse.index() == o.diffuse.index() /* && 
          normals.index() == o.normals.index() */, false);
    switch (diffuse.index()) {
      case 0: guard(std::get<Colr>(diffuse).isApprox(std::get<Colr>(o.diffuse)), false); break;
      case 1: guard(std::get<uint>(diffuse) == std::get<uint>(o.diffuse), false); break;
    }
    /* switch (normals.index()) {
      case 0: guard(std::get<Colr>(normals).isApprox(std::get<Colr>(o.normals)), false); break;
      case 1: guard(std::get<uint>(normals) == std::get<uint>(o.normals), false); break;
    } */
    return true;
  }

  bool Emitter::operator==(const Emitter &o) const {
    return std::tie(type, is_active, transform, illuminant_i, illuminant_scale) 
        == std::tie(o.type, o.is_active, o.transform, o.illuminant_i, o.illuminant_scale);
  }

  /* bool Uplifting::Vertex::has_surface() const {
    return std::visit(overloaded {
      [](const is_surface_constraint auto &c) { return true; },
      [&](const auto &) { return false; }
    }, constraint);
  } */

  /* bool Uplifting::Vertex::has_mismatching() const {
    return std::visit(overloaded {
      [](const DirectColorConstraint &c)     { return c.has_mismatching(); },
      [](const DirectSurfaceConstraint &c)   { return c.has_mismatching(); },
      [](const IndirectSurfaceConstraint &c) { return c.has_mismatching(); },
      [&](const auto &)                      { return false; }
    }, constraint);
  } */

  
  bool Uplifting::operator==(const Uplifting &o) const {
    return std::tie(csys_i, basis_i) == std::tie(o.csys_i, o.basis_i) && rng::equal(verts, o.verts);
  }

  /* SurfaceInfo &Uplifting::Vertex::surface() {
    return std::visit(overloaded {
      [](is_surface_constraint auto &c) -> SurfaceInfo & { return c.surface; },
      [&](auto &) -> SurfaceInfo & { return detail::invalid_visitor_return_si; }
    }, constraint);
  } */

  /* const SurfaceInfo &Uplifting::Vertex::surface() const {
    return std::visit(overloaded {
      [](const is_surface_constraint auto &c) -> const SurfaceInfo & { return c.surface; },
      [&](const auto &) -> const SurfaceInfo & { return detail::invalid_visitor_return_si; }
    }, constraint);
  } */

  std::pair<Colr, Spec> Uplifting::Vertex::realize(const Scene &scene, const Uplifting &uplifting) {
    met_trace();

    // Output values
    Colr c = 0.f;
    Spec s = 0.f;

    // Return zero constraint for inactive parts
    if (!is_active || constraints.empty())
      return { c, s };

    // Color system spectra within which the 'uplifted' texture is defined
    CMFS csys_i = scene.csys(uplifting.csys_i).finalize();

    if (has_freedom()) {
      // Identify the type of constraint
      std::visit(overloaded {
        [&](const IndirectSurfaceConstraint &cstr) {
          if (cstr.has_mismatching()) {
            // Return zero constraint for invalid surfaces
            if (!cstr.surface.is_valid()) {
              c = 0.f;
              s = 0.f;
              return;
            }

            // Surface diffuse is constraint position
            c = cstr.surface.diffuse;

            // Generate color system spectra from power series
            IndirectColrSystem csys = {
              .cmfs   = scene.resources.observers[scene.components.observer_i.value].value(),
              .powers = cstr.powers
            };
            auto refl_systems = csys.finalize();
                    
            // Generate a metamer satisfying the constraint set
            s = generate_spectrum(GenerateIndirectSpectrumInfo {
              .basis        = scene.resources.bases[uplifting.basis_i].value(),
              .base_system  = csys_i,
              .base_signal  = cstr.surface.diffuse,
              .refl_systems = refl_systems,
              .refl_signal  = cstr.colr
            });
          }
          else 
          { // We attempt to fill in a default spectrum, which is necessary to establish the initial system
            // Return zero constraint for invalid surfaces
            if (!cstr.surface.is_valid()) {
              c = 0.f;
              s = 0.f;
              return;
            }
            
            // Surface diffuse is constraint position
            c = cstr.surface.diffuse;

            // Gather all relevant color system spectra and corresponding color signals
            std::vector<CMFS> systems = { csys_i };
            std::vector<Colr> signals = { c      };

            // Generate a metamer satisfying the system+signal constraint set
            s = generate_spectrum(GenerateSpectrumInfo {
              .basis   = scene.resources.bases[uplifting.basis_i].value(),
              .systems = systems,
              .signals = signals
            });
          }
        }, 
        [&](const auto &) { }
      }, constraints.front());
    } else {
      // Identify the type of constraint
      std::visit(overloaded {
        [&](const DirectColorConstraint &cstr) {
          // The specified color becomes our vertex color
          c = cstr.colr_i;

          // Gather all relevant color system spectra and corresponding color signals
          std::vector<CMFS> systems = { csys_i };
          std::vector<Colr> signals = { c      };
          rng::transform(cstr.csys_j, std::back_inserter(systems), 
            [&](uint csys_j) { return scene.csys(csys_j).finalize(); });
          rng::copy(cstr.colr_j, std::back_inserter(signals));

          // Generate a metamer satisfying the system+signal constraint set
          s = generate_spectrum(GenerateSpectrumInfo {
            .basis   = scene.resources.bases[uplifting.basis_i].value(),
            .systems = systems,
            .signals = signals
          });
        },
        [&](const DirectSurfaceConstraint &cstr) {
          // Return zero constraint for invalid surfaces
          if (!cstr.has_surface()) {
            c = 0.f;
            s = 0.f;
            return;
          }

          // Color is obtained from surface information
          c = cstr.surface.diffuse;

          // Gather all relevant color system spectra and corresponding color signals
          std::vector<CMFS> systems = { csys_i };
          std::vector<Colr> signals = { c      };
          rng::transform(cstr.csys_j, std::back_inserter(systems), 
            [&](uint csys_j) { return scene.csys(csys_j).finalize(); });
          rng::copy(cstr.colr_j, std::back_inserter(signals));

          // Generate a metamer satisfying the system+signal constraint set
          s = generate_spectrum(GenerateSpectrumInfo {
            .basis   = scene.resources.bases[uplifting.basis_i].value(),
            .systems = systems,
            .signals = signals
          });
        },
        [&](const IndirectSurfaceConstraint &cstr) {
          if (cstr.has_mismatching()) {
            // Return zero constraint for invalid surfaces
            if (!cstr.surface.is_valid()) {
              c = 0.f;
              s = 0.f;
              return;
            }

            // Surface diffuse is constraint position
            c = cstr.surface.diffuse;

            // Generate color system spectra from power series
            IndirectColrSystem csys = {
              .cmfs   = scene.resources.observers[scene.components.observer_i.value].value(),
              .powers = cstr.powers
            };
            auto refl_systems = csys.finalize();
                    
            // Generate a metamer satisfying the constraint set
            s = generate_spectrum(GenerateIndirectSpectrumInfo {
              .basis        = scene.resources.bases[uplifting.basis_i].value(),
              .base_system  = csys_i,
              .base_signal  = cstr.surface.diffuse,
              .refl_systems = refl_systems,
              .refl_signal  = cstr.colr
            });
          }
          else 
          { // We attempt to fill in a default spectrum, which is necessary to establish the initial system
            // Return zero constraint for invalid surfaces
            if (!cstr.surface.is_valid()) {
              c = 0.f;
              s = 0.f;
              return;
            }
            
            // Surface diffuse is constraint position
            c = cstr.surface.diffuse;

            // Gather all relevant color system spectra and corresponding color signals
            std::vector<CMFS> systems = { csys_i };
            std::vector<Colr> signals = { c      };

            // Generate a metamer satisfying the system+signal constraint set
            s = generate_spectrum(GenerateSpectrumInfo {
              .basis   = scene.resources.bases[uplifting.basis_i].value(),
              .systems = systems,
              .signals = signals
            });
          }
        },
        [&](const MeasurementConstraint &cstr) {
          // The specified spectrum becomes our metamer
          s = cstr.measure;

          // The metamer's color under the uplifting's color system becomes our vertex color
          c = (csys_i.transpose() * s.matrix()).eval();
        },
        [&](const auto &) { }
      }, constraints.front());
    }

    return { c, s };
  }

  bool Uplifting::Vertex::has_mismatching() const {
    met_trace();
    return rng::any_of(constraints, [](const auto &cstr) { 
      return std::visit([](const is_metameric_constraint auto &v) { return v.has_mismatching(); }, cstr);
    });
  }

  bool Uplifting::Vertex::has_freedom() const {
    met_trace();
    return !constraints.empty() && rng::all_of(constraints, [](const auto &cstr) { 
      return std::visit([](const is_metameric_constraint auto &v) { return v.has_freedom(); }, cstr);
    });
  }
} // namespace met