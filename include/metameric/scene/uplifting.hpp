// Copyright (C) 2024 Mark van de Ruit, Delft University of Technology.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/scene/constraints.hpp>
#include <metameric/core/convex.hpp>
#include <metameric/scene/detail/atlas.hpp>
#include <metameric/scene/detail/utility.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>
#include <deque>

namespace met {
  // Forward declaration
  struct Uplifting;
  struct UpliftingVertex;

  // Spectral sample rates
  constexpr static uint n_uplifting_boundary_samples      = 128u; // Color system boundary samples
  constexpr static uint n_uplifting_mismatch_samples      = 256u; // Metamer mismatch volume samples
  constexpr static uint n_uplifting_mismatch_samples_iter = 16u;  // Above, but per frame total

  // Spectral uplifting data;
  // Formed by a color system whose spectral boundary is found, and whose interior is
  // described through tessellation. Spectral uplifting behavior is imposed on the
  // Is applied to a scene object, uplifting the object's underlying color or texture
  // input before rendering takes place.
  struct Uplifting {
    using Vertex = UpliftingVertex;

  public: // Public members
    uint                observer_i   = 0; // Index of primary color system observer data
    uint                illuminant_i = 0; // Index of primary color system illuminant data
    uint                basis_i      = 0; // Index of underlying basis function data
    std::vector<Vertex> verts;            // All vertex constraints

  public: // Public methods
    // Generate N spectral samples on the color system boundary, using the spherical sampling
    // method of Mackiewicz et al, 2019.
    std::vector<MismatchSample> sample_color_solid(const Scene &scene, uint seed, uint n) const;

  public: // Boilerplate
    bool operator==(const Uplifting &o) const;
  };

  // Spectral uplifting constraint data;
  // Interior vertex for the tessellation; encapsulates std::variant of different constraint types
  // and generates vertex position and associated spectral reflectance through the constraint;
  // some vertices expose SurfaceInfo data user-picked from the scene, which backs the constraint.
  struct UpliftingVertex {
    using cnstr_type = std::variant<
      MeasurementConstraint, DirectColorConstraint, 
      DirectSurfaceConstraint, IndirectSurfaceConstraint
    >;
                                    
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
    bool operator==(const UpliftingVertex &o) const = default;
    
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
  
  namespace detail {
    // Template specialization of SceneGLHandler that provides up-to-date storage
    // for per-object uplifted texture data. This class handles spectral uplifting,
    // texture baking, etc. Probably the biggest class as a result. Relies on a trio
    // of helper classes to generate mismatch data, gl-side data, and bake textures.
    template <>
    struct SceneGLHandler<met::Uplifting> : public SceneGLHandlerBase {
      // Helper object that
      // - iteratively builds mismatch volumes (MMVs) for constraints, eating the cost over several frames
      // - recovers constraint spectra through linear interpolation of the resulting convex structure
      // - exposes the mismatch volume hull data for the editor
      // which, alltogether, is faster and more stable than solving for constraint spectra directly.
      class MetamerBuilder {
        using cnstr_type = typename Uplifting::Vertex::cnstr_type;

        bool  	                   m_did_sample   = false;
        std::deque<MismatchSample> m_samples      = { };
        uint                       m_samples_curr = 0;
        uint                       m_samples_prev = 0;
        std::optional<cnstr_type>  m_cnstr_cache;

        // Insert newly generated MMV boundary samples, and retire old ones
        void insert_samples(std::span<const MismatchSample> new_samples);

      public:
        // Get a spectral sample for the given uplifting constraint over which this MMV is defined;
        // also returns whether the sample is equal to the previous sample
        MismatchSample realize(const Scene &scene, uint uplifting_i, uint vertex_i);

        // Test if the vertex at vertex_i results in the same mismatch region
        // as the current sample set for a cached constraint
        bool supports_vertex(const Scene &scene, uint uplifting_i, uint vertex_i);

        // Set the cached constraint to produce a mismatch volume for a given vertex
        void set_vertex(const Scene &scene, uint uplifting_i, uint vertex_i);

        // Builder has reached the required sample count and should just regurgitate the current result
        bool is_converged() const {
          return (hull.deln.verts.size() - m_samples_prev) >= n_uplifting_mismatch_samples;
        }

        // Builder generated new samples, meaning the output of realize() also changed
        bool did_sample() const {
          return m_did_sample;
        }
        
      public: 
        // Expose generated convex hull structure for editors
        ConvexHull hull;
      };

      // Helper object that
      // - holds per-uplifting generated spectral data, tessellation, mismatch volumes, etc
      // - pushes these to the GL-side for ObjectData::update() to use
      class UpliftingData {
        // Per-object block layout for std140 uniform buffer, mapped for write
        struct alignas(16) BufferBaryBlock {
          eig::Matrix<float, 4, 3> inv; // 3+1, last column is padding
          eig::Matrix<float, 4, 1> sub; // 3+1, last value is padding
        };
        static_assert(sizeof(BufferBaryBlock) == 64);

        // All-object block layout for std140 uniform buffer, mapped for write
        struct BufferBaryLayout {
          alignas(4) uint size;
          std::array<BufferBaryBlock, met_max_constraints> data;
        } *m_buffer_bary_map;

        // Per-object block layout for std430 storage buffer, mapped for write
        using BufferCoefBlock = eig::Array<float, wavelength_bases, 4>;
        static_assert(sizeof(BufferCoefBlock) == wavelength_bases * 4 * sizeof(float));

        // All-object block layout for std430 storage buffer, mapped for write
        struct BufferCoefLayout {
          std::array<BufferCoefBlock, met_max_constraints> data;
        } *m_buffer_coef_map;
        
        // Small private state
        bool m_is_first_update;
        uint m_uplifting_i;
        
      public:
        // Helper objects per vertex constraint, to iteratively generate mismatch volume
        // data and produce metamers (tends to be cheaper than solving directly)
        std::vector<MetamerBuilder> metamer_builders;
      
        // Generated spectral data; boundary, interior, and both sets together
        std::vector<MismatchSample> boundary;
        std::vector<MismatchSample> interior;
        std::vector<MismatchSample> boundary_and_interior;

        // R^3 delaunay tessellation resulting from the connected boundary and interior vertices
        AlDelaunay tessellation;

        // Buffers made available for use in update_object_texture
        gl::Buffer buffer_bary; // tetrahedron baycentric data
        gl::Buffer buffer_coef; // tetrahedron coefficient data

      public:
        UpliftingData(uint uplifting_i);
        void update(const Scene &scene);

        // Helper function to find some tetrahedron info, given an input position inside the tesselation
        std::pair<eig::Vector4f, uint> find_enclosing_tetrahedron(const eig::Vector3f &p) const;
      };

      // Helper object that
      // - generates per-object spectral texture data
      // - writes this data to a scene texture atlas
      struct ObjectData {
        // Layout for data written to std140 buffer
        struct BlockLayout { uint object_i; };

        // Objects for texture bake
        std::string  m_program_key;
        gl::Sampler  m_sampler;
        gl::Buffer   m_buffer;
        BlockLayout *m_buffer_map;

        // Small private state
        uint m_object_i;
        bool m_is_first_update;
      
      public:
        ObjectData(const Scene &scene, uint object_i);
        void update(const Scene &scene);
      };

      // Helper object that
      // - generates per-emitter spectral texture data (for uplifted emitters)
      // - writes this data to a scene texture atlas
      struct EmitterData {
        struct BlockLayout { 
          alignas(8) eig::Array2f boundaries;
          alignas(4) uint         emitter_i; 
        };
        static_assert(sizeof(BlockLayout) == 16);
        
        // Objects for texture bake
        std::string  m_program_key;
        gl::Sampler  m_sampler;
        gl::Buffer   m_buffer;
        BlockLayout *m_buffer_map;
        
        // Small amount of state
        uint m_emitter_i;
        bool m_is_first_update;

      public:
        EmitterData(const Scene &scene, uint emitter_i);
        void update(const Scene &scene);
      };

    public:
      // Helpers/caches; these generate some uplifting data and then bake the upliftg into
      // texture atlas patches. They are exposed as some places might access their data
      std::vector<UpliftingData> uplifting_data;
      std::vector<ObjectData>    object_data;
      std::vector<EmitterData>   emitter_data;

      // Atlas textures; each uplifted object/emitter has a patch in the atlas for uplifting
      // coeffs. Stores packed linear coefficients representing spectral functions in basis.
      detail::TextureAtlas2d4f texture_object_coef; 
      detail::TextureAtlas2d4f texture_emitter_coef;
      detail::TextureAtlas2d1f texture_emitter_scle; // Emitters track a single per-pixel scalar for hdr data

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

    // Template specialization of SceneStateHandler that exposes fine-grained
    // state tracking for object members in the program view
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

// Helper output for fmtlib
template<>
struct fmt::formatter<met::Uplifting::Vertex::cnstr_type>{
  template <typename context_ty>
  constexpr auto parse(context_ty& ctx) { 
    return ctx.begin(); 
  }

  template <typename fmt_context_ty>
  constexpr auto format(const met::Uplifting::Vertex::cnstr_type& ty, fmt_context_ty& ctx) const {
    auto s = ty | met::visit { [&](const auto &arg) { return fmt::format("{}", arg); } };
    return fmt::format_to(ctx.out(), "{}", s);
  }
};