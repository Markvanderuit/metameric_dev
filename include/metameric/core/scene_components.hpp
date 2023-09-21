#pragma once

#include <metameric/core/detail/scene.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <vector>

namespace met {
  /* Color system representation; a simple referral to CMFS and illuminant data */
  struct ColrSystemComponent {
    uint observer_i   = 0;
    uint illuminant_i = 0;
    uint n_scatters   = 0;

    friend auto operator<=>(const ColrSystemComponent &, const ColrSystemComponent &) = default;
  };

  /* Emitter representation;  just a simple point light for now */
  struct EmitterComponent {
    bool         is_active    = true; // Is drawn in viewport
    eig::Array3f p            = 1.f; // point light position
    float        multiplier   = 1.f; // power multiplier
    uint         illuminant_i = 0;   // index to spectral illuminant

    inline 
    bool operator==(const EmitterComponent &o) const {
      guard(std::tie(is_active, multiplier, illuminant_i) 
         == std::tie(o.is_active, o.multiplier, o.illuminant_i), false);
      return p.isApprox(o.p);
    }
  };

  /* Object representation; 
     A shape represented by a surface mesh, material data, 
     and an accompanying uplifting to handle spectral data. */
  struct Object {
    // Is drawn in viewport
    bool is_active = true;

    // Indices to underlying mesh, and applied spectral uplifting
    uint mesh_i, uplifting_i;

    // Material data, packed with object; either a specified value, or a texture index
    std::variant<Colr,  uint> diffuse;
    std::variant<float, uint> roughness;
    std::variant<float, uint> metallic;
    std::variant<float, uint> opacity;
    std::variant<Colr,  uint> normals;

    // Position/rotation/scale are captured in an affine transform
    eig::Affine3f trf;

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

  /* Scene settings data layout. */
  struct Settings {
    // Texture render size; input res, 2048x2048, 1024x1024, or 512x512
    enum class TextureSize { eFull, eHigh, eMed, eLow } texture_size;

    friend auto operator<=>(const Settings &, const Settings &) = default;
  };

  /* Spectral uplifting data layout;
     Mostly a tesselation of a color space, with constraints on the tesselation's
     vertices describing spectral behavior. Kept separate from Scene object,
     given its centrality to the codebase. */
  struct Uplifting {
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
      Colr              colr_i; // Expected color under a primary color system 
      uint              csys_i; // Index referring to the primary color system
      std::vector<Colr> colr_j; // Expected colors under secondary color systems
      std::vector<uint> csys_j; // Indices of the secondary color systems
      
      // If type == Type::eColorOnMesh, these determine mesh location
      uint         object_i;         // Index of object to which constraint belongs
      uint         object_elem_i;    // Index of element where constraint is located on object
      eig::Array3f object_elem_bary; // Barycentric coordinates inside element

      // If type == Type::eMeasurement, this holds a measured spectral constraint
      Spec measurement;
    };
    
    // Shorthands for the mesh structure; note, with the delaunay mesh connecting 
    // elements are generated on the fly, they are only stored for the convex hull
    using Vert = Constraint;
    using Elem = eig::Array3u;

    // The mesh structure defines how constraints are connected; e.g. as points
    // on a convex hull with generalized barycentrics for the interior, or points 
    // throughout color space with a delaunay tesselation connecting the interior
    enum class Type {
      eConvexHull, eDelaunay      
    } type = Type::eDelaunay;

    uint              csys_i  = 0; // Index of primary color system for input texture
    uint              basis_i = 0; // Index of used underlying basis
    std::vector<Vert> verts;       // Vertices of uplifting mesh
    std::vector<Elem> elems;       // Elements of uplifting mesh
  };
  
  namespace detail {
    /* Overload of ComponentState for Object */
    struct ObjectState : public detail::ComponentStateBase<Object> {
      using Base = Object;
      using ComponentStateBase<Base>::m_mutated;
      
      detail::ComponentState<bool>               is_active;
      detail::ComponentState<uint>               mesh_i;
      detail::ComponentState<uint>               uplifting_i;
      detail::ComponentStateVariant<Colr,  uint> diffuse;
      detail::ComponentStateVariant<float, uint> roughness;
      detail::ComponentStateVariant<float, uint> metallic;
      detail::ComponentStateVariant<float, uint> opacity;
      detail::ComponentStateVariant<Colr,  uint> normals;
      detail::ComponentState<eig::Affine3f>      trf;

    public:
      virtual bool update(const Base &o) override {
        return m_mutated = (
          is_active.update(o.is_active)     |
          mesh_i.update(o.mesh_i)           |
          uplifting_i.update(o.uplifting_i) |
          diffuse.update(o.diffuse)         |
          roughness.update(o.roughness)     |
          metallic.update(o.metallic)       |
          opacity.update(o.opacity)         |
          normals.update(o.normals)         |
          trf.update(o.trf)
        );
      }
    };

    /* Overload of ComponentState for Settings */
    struct SettingsState : public ComponentStateBase<Settings> {
      using Base = Settings;
      using ComponentStateBase<Base>::m_mutated;

      ComponentState<Base::TextureSize> texture_size;

    public:
      virtual 
      bool update(const Base &o) override {
        return m_mutated = texture_size.update(o.texture_size);
      }
    };
    
    /* Overload of ComponentState for Uplifting */
    struct UpliftingState : public ComponentStateBase<Uplifting> {
      struct ConstraintState : public ComponentStateBase<Uplifting::Constraint> {
        using Base = Uplifting::Constraint;
        using ComponentStateBase<Base>::m_mutated;
        
        ComponentState<Base::Type>   type;

        ComponentState<Colr>         colr_i;
        ComponentState<uint>         csys_i;
        ComponentStateVector<Colr>   colr_j;
        ComponentStateVector<uint>   csys_j;

        ComponentState<uint>         object_i;
        ComponentState<uint>         object_elem_i;
        ComponentState<eig::Array3f> object_elem_bary;
        
        ComponentState<Spec>         measurement;

        virtual 
        bool update(const Base &o) override {
          return m_mutated = (type.update(o.type)
                           || colr_i.update(o.colr_i)
                           || csys_i.update(o.csys_i)
                           || colr_j.update(o.colr_j)
                           || csys_j.update(o.csys_j)
                           || object_i.update(o.object_i)
                           || object_elem_i.update(o.object_elem_i)
                           || object_elem_bary.update(o.object_elem_bary)
                           || measurement.update(o.measurement));
        }
      };

      using Base = Uplifting;
      using ComponentStateBase<Base>::m_mutated;

      ComponentState<Base::Type> type;
      ComponentState<uint>       csys_i;
      ComponentState<uint>       basis_i;
      ComponentStateVector<
        Base::Constraint, 
        ConstraintState>         verts;
      ComponentStateVector<
        Base::Elem>              elems;

    public:
      virtual 
      bool update(const Base &o) override {
        return m_mutated = (
          type.update(o.type)       | 
          csys_i.update(o.csys_i)   |
          basis_i.update(o.basis_i) | 
          verts.update(o.verts)     | 
          elems.update(o.elems)
        );
      }
    };
  } // namespace detail
} // namespace met