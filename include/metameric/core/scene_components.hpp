#pragma once

#include <metameric/core/detail/scene_components.hpp>
#include <metameric/core/detail/scene_components_fwd.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <vector>

namespace met {
  /* Scene settings data layout. */
  struct Settings {
    using state_type = detail::SettingsState;

    // Texture render size; input res, 2048x2048, 1024x1024, or 512x512
    enum class TextureSize { eFull, eHigh, eMed, eLow } texture_size;

    friend auto operator<=>(const Settings &, const Settings &) = default;

  public: // Helper methods to apply stored settings
    inline
    eig::Array2u apply_texture_size(const eig::Array2u &size) const {
      switch (texture_size) {
        case Settings::TextureSize::eHigh: return size.cwiseMin(2048u);
        case Settings::TextureSize::eMed:  return size.cwiseMin(1024u);
        case Settings::TextureSize::eLow:  return size.cwiseMin(512u);
        default:                           return size;
      }
    }
  };

  /* Color system representation; a simple referral to CMFS and illuminant data */
  struct ColorSystem {
    uint observer_i   = 0;
    uint illuminant_i = 0;
    uint n_scatters   = 0;

    friend
    auto operator<=>(const ColorSystem &, const ColorSystem &) = default;
  };

  /* Object representation; 
     A shape represented by a surface mesh, material data, 
     and an accompanying uplifting to handle spectral data. */
  struct Object {
    using state_type = detail::ObjectState;

    // Is drawn in viewport
    bool is_active = true;

    // Indices to underlying mesh, and applied spectral uplifting
    uint mesh_i, uplifting_i;

    // Material data, packed with object; either a specified value, or a texture index
    std::variant<Colr,  uint> diffuse;
    std::variant<Colr,  uint> normals;
    std::variant<float, uint> roughness;
    std::variant<float, uint> metallic;
    std::variant<float, uint> opacity;

    // Position/rotation/scale are captured in an affine transform
    eig::Affine3f trf;
    
  public:
    inline 
    bool operator==(const Object &o) const {
      guard(std::tie(is_active, mesh_i, uplifting_i) == std::tie(o.is_active, o.mesh_i, o.uplifting_i), false);
      guard(std::tie(roughness, metallic, opacity) == std::tie(o.roughness, o.metallic, o.opacity), false);
      guard(diffuse.index() == o.diffuse.index() && normals.index() == o.normals.index(), false);
      switch (diffuse.index()) {
        case 0: guard(std::get<Colr>(diffuse).isApprox(std::get<Colr>(o.diffuse)), false); break;
        case 1: guard(std::get<uint>(diffuse) == std::get<uint>(o.diffuse), false); break;
      }
      switch (normals.index()) {
        case 0: guard(std::get<Colr>(normals).isApprox(std::get<Colr>(o.normals)), false); break;
        case 1: guard(std::get<uint>(normals) == std::get<uint>(o.normals), false); break;
      }
      return trf.isApprox(o.trf);
    }
  };

  /* Emitter representation; just a simple point light for now */
  struct Emitter {
    enum class Type { eConstant, ePoint, eArea };

    // Is drawn in viewport
    bool         is_active    = true;

    eig::Array3f p            = 1.f;  // point light position
    float        multiplier   = 1.f;  // power multiplier
    uint         illuminant_i = 0;    // index to spectral illuminant

    inline 
    bool operator==(const Emitter &o) const {
      guard(std::tie(is_active, multiplier, illuminant_i) 
         == std::tie(o.is_active, o.multiplier, o.illuminant_i), false);
      return p.isApprox(o.p);
    }
  };

  /* Spectral uplifting data layout;
     Mostly a tesselation of a color space, with constraints on the tesselation's
     vertices describing spectral behavior. Kept separate from Scene object,
     given its centrality to the codebase. */
  struct Uplifting {
    using state_type = detail::UpliftingState;

    // The mesh structure connects a set of user-configured constraints; 
    // there are three types and they can be used intermittently:
    // - Color values across different systems, primary color sampled from a color space position
    // - Color values across different systems; primary color sampled from an object surface position
    // - Spectral measurements, based on artist-provided data
    struct Constraint {
      enum class Type {
        eColor, eColorOnMesh, eMeasurement
      } type = Type::eColor;

      // If type == Type::eColor, these are the color constraints
      Colr              colr_i; // Expected color under primary color system 
      std::vector<Colr> colr_j; // Expected colors under secondary color systems
      std::vector<uint> csys_j; // Indices of the secondary color systems
      
      // If type == Type::eColorOnMesh, these determine mesh location
      uint         object_i;         // Index of object to which constraint belongs
      uint         object_elem_i;    // Index of element where constraint is located on object
      eig::Array3f object_elem_bary; // Barycentric coordinates inside element

      // If type == Type::eMeasurement, this holds a measured spectral constraint
      Spec measurement;
    };

    // The mesh structure defines how constraints are connected; e.g. as points
    // on a convex hull with generalized barycentrics for the interior, or points 
    // throughout color space with a delaunay tesselation connecting the interior
    enum class Type {
      eConvexHull, eDelaunay      
    } type = Type::eDelaunay;

    uint                    csys_i  = 0; // Index of primary color system
    uint                    basis_i = 0; // Index of used underlying basis
    std::vector<Constraint> verts;       // Vertex constraints on uplifting mesh
  };
} // namespace met