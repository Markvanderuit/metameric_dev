#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/core/constraints.hpp>
#include <metameric/core/detail/scene_components.hpp>

namespace met {
  /* Scene settings data layout. */
  struct Settings {
    // Selected viewport renderer; the rgb renderers are hacked in just for debugging
    enum class RendererType { 
      ePath,        // Spectral render, up to fixed path length
      ePathRGB,     // RGB fallback, up to fixed path length
      eDirect,      // Spectral render, direct light only
      eDirectRGB,   // RGB fallback, direct light only
      eDebug,      // Spectral render, queries a value (eg albedo) and returns
      eDebugRGB    // RGB fallback, queries a value (eg albedo) and returns
    } renderer_type = RendererType::ePath;

    // Clamped texture sizes in atlas; input res, 2048x2048, 1024x1024, or 512x512
    enum class TextureSize { 
      eFull, eHigh, eMed, eLow 
    } texture_size = TextureSize::eHigh;

    // View component linked to scene viewport
    uint view_i = 0;

    // Render scaling used for scene viewport
    float view_scale   = .5f;  

  public: // Boilerplate  
    auto operator<=>(const Settings &) const = default;

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

  /* Camera and render settings data layout; a simple description
     of how to render the current scene, either to screen or film. */
  struct View {
    uint         observer_i   = 0;    // Referral to underlying CMFS
    Transform    camera_trf;          // Transform applied to scene camera
    float        camera_fov_y = 45.f; // Vertical field of view
    eig::Array2u film_size    = 256;  // Pixel count of film target

  public: // Boilerplate
    bool operator==(const View &o) const;
  };

  /* Object representation; 
     A shape represented by a surface mesh, material data, 
     and an accompanying uplifting to handle spectral reflectance. */
  struct Object {
    // Emitter type; only very basic BRDFs are supported
    enum class BRDFType {
      eNull       = 0,  // null is empty; object does not interact with scene
      eDiffuse    = 1,  // diffuse is lambertian
      eMirror     = 2,  // mirror is perfect specular reflector
      ePrincipled = 3,  // very very partial implementation of mprincipled brdf
    };
    
  public:
    // Scene properties
    bool      is_active = true;
    Transform transform;

    // Indices to underlying mesh/uplifting
    uint mesh_i;
    uint uplifting_i;

    // Material data is packed with object; 
    // some values are variant of a specified value, or a texture index
    BRDFType                  brdf_type;
    std::variant<Colr,  uint> diffuse;

  public: // Boilerplate
    bool operator==(const Object &o) const;
  };

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

  /* Spectral uplifting data layout;
     Mostly a tesselation of a color space, with constraints on the tesselation's
     vertices describing spectral behavior. Kept separate from Scene object,
     given its centrality to the codebase. */
  struct Uplifting {
    struct Vertex {
      using cnstr_type = std::variant<MeasurementConstraint,   DirectColorConstraint,
                                      DirectSurfaceConstraint, IndirectSurfaceConstraint>;
                                      
      std::string name;             // Associated name
      cnstr_type  constraint;       // Underlying, user-specified constraint
      bool        is_active = true; // Whether the constraint is used in the scene

    public: // Public methods
      // Realize a spectral metamer, which forms the vertex' position in the uplifting tesselation,
      // and attempts to satisfy the vertex' attached constraint
      MismatchSample realize(const Scene &scene, const Uplifting &uplifting) const;
      
      // Realize N spectral metamers on the constraint's current mismatch boundary, 
      // w.r.t. the last internal constraint, which is a "free variable"
      std::vector<MismatchSample> realize_mismatch(const Scene &scene, const Uplifting &uplifting, uint seed, uint n) const;

      // Set/get the color value of the last constraint; this is the "free variable"
      // which the mismatch boundary encloses
      void set_mismatch_position(const Colr &c);
      Colr get_mismatch_position() const;

      // Test whether this vertex' constraint would generate the exact same mismatch
      // boundary as another, prior constraint. This way, we can avoid regenerating volumes
      // if only the "free variable" differs
      bool has_equal_mismatching(const cnstr_type &other) const;

      // Test whether this vertex' position in the tesselation can jitter to avoid
      // minor roundtrip error to the uplifting's color system, or whether this error
      // is intentional as the base linear constraint may be disabled 
      bool is_position_shifting() const;

      // Get vertex' position in the tesselation
      Colr get_vertex_position() const;

    public: // Constraint-specific boilerplate; depend on which constraint is used
      bool operator==(const Vertex &o) const = default;
      bool has_mismatching(const Scene &scene, const Uplifting &uplifting) const; // Does the underlying constraint allow for mismatching?
      bool has_surface()                                                   const; // Does the underlying constraint expose surface data?
      const SurfaceInfo &surface() const;
      void set_surface(const SurfaceInfo &sr);
      std::vector<SurfaceInfo> surfaces() const;
    };

  public: // Public members
    uint                observer_i   = 0; // Index of primary color system observer data
    uint                illuminant_i = 0; // Index of primary color system illuminant data
    uint                basis_i      = 0; // Index of underlying basis function data
    std::vector<Vertex> verts;            // All vertex constraints on mesh

  public: // Boilerplate
    bool operator==(const Uplifting &o) const;
  };
} // namespace met

// Custom std::format overloads for some types
namespace std {
  // Format Object::BRDFType, wich is an enum class
  template <>
  struct std::formatter<met::Object::BRDFType> : std::formatter<string_view> {
    auto format(const met::Object::BRDFType& ty, std::format_context& ctx) const {
      std::string s;
      switch (ty) {
        case met::Object::BRDFType::eNull       : s = "null";        break;
        case met::Object::BRDFType::eDiffuse    : s = "diffuse";     break;
        case met::Object::BRDFType::eMirror     : s = "mirror";      break;
        case met::Object::BRDFType::ePrincipled : s = "principled";  break;
      };
      return std::formatter<std::string_view>::format(s, ctx);
    }
  };

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

  // Format Settings::RendererType, wich is an enum class
  template <>
  struct std::formatter<met::Settings::RendererType> : std::formatter<string_view> {
    auto format(const met::Settings::RendererType& ty, std::format_context& ctx) const {
      std::string s;
      switch (ty) {
        case met::Settings::RendererType::ePath      : s = "path";         break;
        case met::Settings::RendererType::ePathRGB   : s = "path (rgb)";   break;
        case met::Settings::RendererType::eDirect    : s = "direct";       break;
        case met::Settings::RendererType::eDirectRGB : s = "direct (rgb)"; break;
        case met::Settings::RendererType::eDebug     : s = "debug";        break;
        case met::Settings::RendererType::eDebugRGB  : s = "debug (rgb)";  break;
      };
      return std::formatter<std::string_view>::format(s, ctx);
    }
  };
} // namespace std