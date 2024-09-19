#pragma once

#include <metameric/core/components.hpp>

namespace met::detail {
  // Template specialization of SceneStateHandler that exposes specific state
  // for the members of met::Object.
  template <>
  struct SceneStateHandler<Object> : public SceneStateHandlerBase<Object> {    
    SceneStateHandler<decltype(Object::is_active)>   is_active;
    SceneStateHandler<decltype(Object::transform)>   transform;
    SceneStateHandler<decltype(Object::mesh_i)>      mesh_i;
    SceneStateHandler<decltype(Object::uplifting_i)> uplifting_i;
    SceneStateHandler<decltype(Object::diffuse)>     diffuse;

  public:
    bool update(const Object &o) override {
      met_trace();
      return m_mutated = 
      ( is_active.update(o.is_active)
      | transform.update(o.transform)
      | mesh_i.update(o.mesh_i)
      | uplifting_i.update(o.uplifting_i)
      | diffuse.update(o.diffuse)
      );
    }
  };
  
  // Template specialization of SceneStateHandler that exposes specific state
  // for the members of met::Settings.
  template <>
  struct SceneStateHandler<Settings> : public SceneStateHandlerBase<Settings> {
    SceneStateHandler<decltype(Settings::renderer_type)> renderer_type;
    SceneStateHandler<decltype(Settings::texture_size)>  texture_size;
    SceneStateHandler<decltype(Settings::view_i)>        view_i;
    SceneStateHandler<decltype(Settings::view_scale)>    view_scale;

  public:
    bool update(const Settings &o) override {
      met_trace();
      return m_mutated = 
      ( renderer_type.update(o.renderer_type)
      | texture_size.update(o.texture_size)
      | view_i.update(o.view_i)
      | view_scale.update(o.view_scale)
      );
    }
  };
  
  // Template specialization of SceneStateHandler that exposes specific state
  // for the members of met::Uplifting::Vertex.
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
  
  // Template specialization of SceneStateHandler that exposes specific state
  // for the members of met::Uplifting.
  template <>
  struct SceneStateHandler<Uplifting> : public SceneStateHandlerBase<Uplifting> {
    SceneStateHandler<decltype(Uplifting::csys_i)>                 csys_i;
    SceneStateHandler<decltype(Uplifting::basis_i)>                basis_i;
    SceneStateVectorHandler<decltype(Uplifting::verts)::value_type> verts;

  public:
    bool update(const Uplifting &o) override {
      met_trace();
      return m_mutated = 
      ( csys_i.update(o.csys_i)
      | basis_i.update(o.basis_i) 
      | verts.update(o.verts)
      );
    }
  };

  template <>
  struct SceneStateHandler<ViewSettings> : public SceneStateHandlerBase<ViewSettings> {
    SceneStateHandler<decltype(ViewSettings::observer_i)>   observer_i;
    SceneStateHandler<decltype(ViewSettings::camera_trf)>   camera_trf;
    SceneStateHandler<decltype(ViewSettings::camera_fov_y)> camera_fov_y;
    SceneStateHandler<decltype(ViewSettings::film_size)>    film_size;

  public:
    bool update(const ViewSettings &o) override {
      met_trace();
      return m_mutated = 
      ( observer_i.update(o.observer_i)
      | camera_trf.update(o.camera_trf) 
      | camera_fov_y.update(o.camera_fov_y)
      | film_size.update(o.film_size)
      );
    }
  };


} // namespace met::detail