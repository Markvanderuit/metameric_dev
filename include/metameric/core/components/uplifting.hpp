#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/core/atlas.hpp>
#include <metameric/core/constraints.hpp>
#include <metameric/core/detail/scene_components_utility.hpp>
#include <small_gl/texture.hpp>

namespace met {
  // Spectral uplifting data layout;
  // Describes a tesselation of a color system, with constraints on the tesselation's
  // interior that control uplifted spectrum behavior. Is applied to a scene object,
  // and then uplifts the object's referred color or texture input before rendering.
  struct Uplifting {
    // Interior vertex for the tessellation; encapsulates std::variant of different constraint types
    // and generates vertex position and associated spectral reflectance through the constraint;
    // some vertices expose SurfaceInfo data user-picked from the scene, which backs the constraint.
    struct Vertex {
      using cnstr_type = std::variant<MeasurementConstraint,   DirectColorConstraint,
                                      DirectSurfaceConstraint, IndirectSurfaceConstraint>;
                                      
      std::string name;             // Associated name user can set in front-end
      cnstr_type  constraint;       // Underlying, user-specified constraint
      bool        is_active = true; // Whether the constraint is enabled

    public: // Public methods
      // Get vertex' position in the tesselation
      Colr get_vertex_position() const;

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


    public: // Constraint-specific boilerplate; depend on which constraint is used
      bool operator==(const Vertex &o) const = default;
      
      // Does the underlying constraint allow for mismatching?
      bool has_mismatching(const Scene &scene, const Uplifting &uplifting) const; 

      // Does the underlying constraint expose surface data?
      bool has_surface() const;

      // Access (last or all) underlying surface data (last is active part)
      const SurfaceInfo &surface() const;
      std::span<const SurfaceInfo> surfaces() const;

      // Set surface data
      void set_surface(const SurfaceInfo &sr);
    };

  public: // Members
    uint                observer_i   = 0; // Index of primary color system observer data
    uint                illuminant_i = 0; // Index of primary color system illuminant data
    uint                basis_i      = 0; // Index of underlying basis function data
    std::vector<Vertex> verts;            // All vertex constraints on mesh

  public: // Boilerplate
    bool operator==(const Uplifting &o) const;
  };
  
  namespace detail {
    // Template specialization of SceneGLHandler that provides up-to-date storage
    // for per-object uplifted texture data. Note that this class handles
    // allocation and resizing, but the program pipeline fills in the data before rendering.
    // See task_gen_uplifting_data.hpp and task_gen_object_data.hpp for details.
    template <>
    struct SceneGLHandler<met::Uplifting> : public SceneGLHandlerBase {
      // Atlas textures; each scene object has a patch in the atlas for some material parameters
      TextureAtlas2d4ui texture_coef; // Stores packed linear coefficients representing surface spectral reflectances in basis
      TextureAtlas2d1ui texture_brdf; // Stores packing of other brdf parameters (roughness, metallic at fp16)

      // Array texture; each layer holds one of 12 basis function spectra
      gl::TextureArray1d1f texture_basis;

    public:
      // Class constructor and update function handle GL-side data
      SceneGLHandler();
      void update(const Scene &) override;
    };

    // Template specialization of SceneStateHandler that exposes fine-grained
    // state tracking for object members in the program view
    template <>
    struct SceneStateHandler<Uplifting::Vertex> : public SceneStateHandlerBase<Uplifting::Vertex> {
      SceneStateHandler<decltype(Uplifting::Vertex::name)>       name;
      SceneStateHandler<decltype(Uplifting::Vertex::is_active)>  is_active;
      SceneStateHandler<decltype(Uplifting::Vertex::constraint)> constraint;

    public:
      bool update(const Uplifting::Vertex &o) override {
        met_trace();
        return m_mutated = 
        ( name.update(o.name)
        | is_active.update(o.is_active)
        | constraint.update(o.constraint)
        );
      }
    };

    template <>
    struct SceneStateHandler<Uplifting> : public SceneStateHandlerBase<Uplifting> {
      SceneStateHandler<decltype(Uplifting::observer_i)>              observer_i;
      SceneStateHandler<decltype(Uplifting::illuminant_i)>            illuminant_i;
      SceneStateHandler<decltype(Uplifting::basis_i)>                 basis_i;
      SceneStateVectorHandler<decltype(Uplifting::verts)::value_type> verts;

    public:
      bool update(const Uplifting &o) override {
        met_trace();
        return m_mutated = 
        ( observer_i.update(o.observer_i)
        | illuminant_i.update(o.illuminant_i)
        | basis_i.update(o.basis_i) 
        | verts.update(o.verts)
        );
      }
    };
  } // namespace detail
} // namespace met

namespace std {
  // Format Uplifting::Vertex::cstr_type, which is a std::variant
  template <>
  struct std::formatter<met::Uplifting::Vertex::cnstr_type> : std::formatter<string_view> {
    auto format(const met::Uplifting::Vertex::cnstr_type& constraint, std::format_context& ctx) const {
      auto s = constraint | met::visit { [&](const auto &arg) { return std::format("{}", arg); } };
      return std::formatter<std::string_view>::format(s, ctx);
    }
  };
} // namespace std