#pragma once

#include <metameric/core/detail/scene_components.hpp>
#include <metameric/core/constraints.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/matching.hpp>
#include <metameric/core/spectrum.hpp>
#include <vector>

namespace met {
  // Concept; some components have active flags to enable/disable them in the scene
  template <typename Ty>
  concept has_active_value = requires (Ty ty) { { ty.is_active } -> std::same_as<bool&>; };

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

    friend
    auto operator<=>(const ColorSystem &, const ColorSystem &) = default;
  };

  /* Object representation; 
     A shape represented by a surface mesh, material data, 
     and an accompanying uplifting to handle spectral data. */
  struct Object {
    using state_type = detail::ObjectState;

    // Scene properties
    bool      is_active = true;
    Transform transform;

    // Indices to underlying mesh/surface
    uint mesh_i;
    uint uplifting_i;

    // Material data, packed with object; either a specified value, or a texture index
    std::variant<Colr,  uint> diffuse;
    /* std::variant<Colr,  uint> normals;
    std::variant<float, uint> roughness;
    std::variant<float, uint> metallic;
    std::variant<float, uint> opacity; */
    
  public: // Boilerplate
    bool operator==(const Object &o) const;
  };
  static_assert(has_active_value<Object>);

  /* Emitter representation; just a simple point light for now */
  struct Emitter {
    // Emitter type; only very basic primitives are supported
    enum class Type { 
      eConstant = 0, 
      ePoint    = 1, 
      eSphere   = 2, 
      eRect     = 3
    };

  public:
    // Specific emitter type
    Type type = Type::eSphere;
    
    // Scene properties
    bool      is_active = true;
    Transform transform;

    // Spectral data references a scene resource 
    uint  illuminant_i     = 0;    // index to spectral illuminant
    float illuminant_scale = 1.f;  // power multiplier

  public: // Boilerplater
    bool operator==(const Emitter &o) const;
  };
  static_assert(has_active_value<Emitter>);

  /* Spectral uplifting data layout;
     Mostly a tesselation of a color space, with constraints on the tesselation's
     vertices describing spectral behavior. Kept separate from Scene object,
     given its centrality to the codebase. */
  struct Uplifting {
    using state_type = detail::UpliftingState;
  
    struct Vertex {
      using state_type = detail::VertexState;

      // The vertex stores one or more constraints of the following types
      using cnstr_type = std::variant<
        MeasurementConstraint,        // At most 1
        DirectColorConstraint,        // At most 1
        DirectSurfaceConstraint,      // At most 1
        IndirectSurfaceConstraint     // Several can be solved for at the same time
      >;
      
    public:
      // Associated name
      std::string name;
      
      // Whether the constraint is used in the scene
      bool is_active = true;

      // Underlying, user-specified constraint
      cnstr_type constraint;

    public:
      // Generate a spectrum and corresponding color, which
      // forms the vertex' position in the uplifting tesselation
      std::tuple<Colr, Spec, Basis::vec_type> realize(const Scene &scene, const Uplifting &uplifting) const;

    public: // underlying accessors if constraint satisifes is metamerism_constraint/is_surface_constraint
      bool has_mismatching() const; // Does the underlying constraint allow for mismatching?
      bool has_surface()     const; // Does the underlying constraint expose surface data?
      const SurfaceInfo &surface() const;
            SurfaceInfo &surface();

    public:
      bool operator==(const Vertex &o) const = default; // Default comparator
    };

  public:
    uint                csys_i  = 0; // Index of primary color system
    uint                basis_i = 0; // Index of used underlying basis
    std::vector<Vertex> verts;      // Vertex constraints on mesh

  public: // Boilerplate
    bool operator==(const Uplifting &o) const;
  };
  static_assert(has_active_value<Uplifting::Vertex>);
} // namespace met

// Custom std::format overloads for some types
namespace std {
  // Format Emitter::Type, wich is an enum class
  template <>
  struct std::formatter<met::Emitter::Type> : std::formatter<string_view> {
    auto format(const met::Emitter::Type& ty, std::format_context& ctx) const {
      std::string s;
      switch (ty) {
        case met::Emitter::Type::eConstant : s = "constant"; break;
        case met::Emitter::Type::ePoint    : s = "point"; break;
        case met::Emitter::Type::eRect     : s = "rect"; break;
        case met::Emitter::Type::eSphere   : s = "sphere"; break;
      };
      return std::formatter<std::string_view>::format(s, ctx);
    }
  };

  // Format Uplifting::Vertex::cstr_type, which is a std::variant
  template <>
  struct std::formatter<met::Uplifting::Vertex::cnstr_type> : std::formatter<string_view> {
    auto format(const met::Uplifting::Vertex::cnstr_type& constraint, std::format_context& ctx) const {
      auto s = constraint | met::visit { [&](const auto &arg) { return std::format("{}", arg); } };
      return std::formatter<std::string_view>::format(s, ctx);
    }
  };
} // namespace std