#pragma once

#include <metameric/core/detail/scene.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <vector>

namespace met {  
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
    // Fine-grained state tracker helper, so the pipeline can push parts of
    // data where necessary
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
        return m_mutated = (type.update(o.type)
                         || csys_i.update(o.csys_i)
                         || basis_i.update(o.basis_i)
                         || verts.update(o.verts)
                         || elems.update(o.elems));
      }
    };
  } // namespace detail
} // namespace met