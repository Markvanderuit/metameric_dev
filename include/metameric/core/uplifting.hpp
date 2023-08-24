#pragma once

#include <metameric/core/detail/scene.hpp>
#include <metameric/core/math.hpp>
#include <vector>

namespace met {  
  /* Tesselated spectral uplifting representation and data layout;
     kept separate from Scene object, given its importance to the codebase */
  struct Uplifting {
    // A mesh structure defines how constraints are connected; e.g. as points
    // on a convex hull with generalized barycentrics for the interior, or points 
    // throughout color space with a delaunay tesselation connecting the interior
    enum class Type {
      eConvexHull, eDelaunay      
    } type = Type::eDelaunay;

    // The mesh structure connects a set of user-configured constraints; There are three types:
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
      uint         object_elem_i;    // Index of object element where constraint is located
      eig::Array3f object_elem_bary; // Barycentric coordinates inside object element

      // If type == Type::eMeasurement, this holds a measured spectral constraint
      Spec measurement;
    };
    
    // Shorthands for the mesh structure; note, with the delaunay mesh connecting 
    // elements are generated on the fly, they are only stored for the convex hull
    using Vert = Constraint;
    using Elem = eig::Array3u;

    uint              basis_i = 0; // Index of used underlying basis
    std::vector<Vert> verts;       // Vertices of uplifting mesh
    std::vector<Elem> elems;       // Elements of uplifting mesh
  };
  
  namespace detail {
    struct UpliftingState : public ComponentStateBase<Uplifting> {
      struct ConstraintState : public ComponentStateBase<Uplifting::Constraint> {
        using Base = Uplifting::Constraint;
        using ComponentStateBase<Base>::m_stale;
        
        ComponentState<Base::Type>   type;
        ComponentState<Colr>         colr_i;
        ComponentState<uint>         csys_i;
        VectorState<Colr>            colr_j;
        VectorState<uint>            csys_j;
        ComponentState<uint>         object_i;
        ComponentState<uint>         object_elem_i;
        ComponentState<eig::Array3f> object_elem_bary;
        ComponentState<Spec>         measurement;

        virtual 
        bool update_state(const Base &o) override {
          return m_stale = (type.update_state(o.type)
                        || colr_i.update_state(o.colr_i)
                        || csys_i.update_state(o.csys_i)
                        || colr_j.update_state(o.colr_j)
                        || csys_j.update_state(o.csys_j)
                        || object_i.update_state(o.object_i)
                        || object_elem_i.update_state(o.object_elem_i)
                        || object_elem_bary.update_state(o.object_elem_bary)
                        || measurement.update_state(o.measurement));
        }
      };

      using Base = Uplifting;
      using ComponentStateBase<Base>::m_stale;

      ComponentState<Base::Type>    type;
      ComponentState<uint>          basis_i;
      VectorState<Base::Constraint, 
                  ConstraintState>  verts;
      VectorState<Base::Elem>       elems;

    public:
      virtual 
      bool update_state(const Base &o) override {
        return m_stale = (type.update_state(o.type)
                       || basis_i.update_state(o.basis_i)
                       || verts.update_state(o.verts)
                       || elems.update_state(o.elems));
      }
    };
  } // namespace detail
} // namespace met