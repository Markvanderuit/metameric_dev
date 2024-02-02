#pragma once

#include <metameric/core/scene_components.hpp>

namespace met::detail {
  /* Overload of ComponentState for Object */
  struct ObjectState : public detail::ComponentStateBase<Object> {
    using Base = Object;
    using ComponentStateBase<Base>::m_mutated;
    
    detail::ComponentState<decltype(Base::is_active)>    is_active;
    detail::ComponentState<decltype(Base::transform)>    transform;
    detail::ComponentState<decltype(Base::mesh_i)>       mesh_i;
    detail::ComponentState<decltype(Base::uplifting_i)>  uplifting_i;
    detail::ComponentState<decltype(Base::diffuse)>      diffuse;
    detail::ComponentState<decltype(Base::normals)>      normals;
    detail::ComponentState<decltype(Base::roughness)>    roughness;
    detail::ComponentState<decltype(Base::metallic)>     metallic;
    detail::ComponentState<decltype(Base::opacity)>      opacity;

  public:
    virtual
    bool update(const Base &o) override {
      return m_mutated = (
        is_active.update(o.is_active)     |
        transform.update(o.transform)     |
        mesh_i.update(o.mesh_i)           |
        uplifting_i.update(o.uplifting_i) |
        diffuse.update(o.diffuse)         |
        roughness.update(o.roughness)     |
        metallic.update(o.metallic)       |
        opacity.update(o.opacity)         |
        normals.update(o.normals)
      );
    }
  };

  /* Overload of ComponentState for Settings */
  struct SettingsState : public ComponentStateBase<Settings> {
    using Base = Settings;
    using ComponentStateBase<Base>::m_mutated;

    ComponentState<decltype(Base::texture_size)> texture_size;

  public:
    virtual 
    bool update(const Base &o) override {
      return m_mutated = texture_size.update(o.texture_size);
    }
  };
  
  /* Overload of ComponentState for Uplifting */
  struct UpliftingState : public ComponentStateBase<Uplifting> {
    /* struct ConstraintState : public ComponentStateBase<Uplifting::Constraint> {
      using Base = Uplifting::Constraint;
      using ComponentStateBase<Base>::m_mutated;
      
      ComponentState<decltype(Base::type)>                type;
      ComponentState<decltype(Base::colr_i)>              colr_i;
      ComponentStates<decltype(Base::colr_j)::value_type> colr_j;
      ComponentStates<decltype(Base::csys_j)::value_type> csys_j;
      ComponentState<decltype(Base::object_i)>            object_i;
      ComponentState<decltype(Base::object_elem_i)>       object_elem_i;
      ComponentState<decltype(Base::object_elem_bary)>    object_elem_bary;
      ComponentState<decltype(Base::measurement)>         measurement;

      virtual 
      bool update(const Base &o) override {
        return m_mutated = (
          type.update(o.type)                         |
          colr_i.update(o.colr_i)                     |
          colr_j.update(o.colr_j)                     |
          csys_j.update(o.csys_j)                     |
          object_i.update(o.object_i)                 |
          object_elem_i.update(o.object_elem_i)       |
          object_elem_bary.update(o.object_elem_bary) |
          measurement.update(o.measurement)
        );
      }
    }; */

    using Base = Uplifting;
    using ComponentStateBase<Base>::m_mutated;

    ComponentState<decltype(Base::type)>               type;
    ComponentState<decltype(Base::csys_i)>             csys_i;
    ComponentState<decltype(Base::basis_i)>            basis_i;
    ComponentStates<decltype(Base::verts)::value_type> verts;

  public:
    virtual 
    bool update(const Base &o) override {
      return m_mutated = (
        type.update(o.type)       | 
        csys_i.update(o.csys_i)   |
        basis_i.update(o.basis_i) | 
        verts.update(o.verts)
      );
    }
  };
} // namespace met::detail