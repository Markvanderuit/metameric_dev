#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/image.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/uplifting.hpp>
#include <metameric/core/settings.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/scene.hpp>
#include <variant>

namespace met {  
  /* Scene data.
     Simple indexed scene; no graph, just a library of objects and 
     their dependencies; responsible for most program data, as well
     as state tracking of dependency modifications */
  struct Scene {
  public: // Scene object classes
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
        return trf.isApprox(o.trf)
            && std::tie(is_active, mesh_i, uplifting_i) == std::tie(o.is_active, o.mesh_i, o.uplifting_i);
      }
    };

    /* Emitter representation; 
       just a simple point light for now */
    struct Emitter {
      bool         is_active    = true; // Is drawn in viewport
      eig::Array3f p            = 1.f; // point light position
      float        multiplier   = 1.f; // power multiplier
      uint         illuminant_i = 0;   // index to spectral illuminant

      inline 
      bool operator==(const Emitter &o) const {
        return p.isApprox(o.p)
            && std::tie(is_active, multiplier, illuminant_i) 
            == std::tie(o.is_active, o.multiplier, o.illuminant_i);
      }
    };

    /* Color system representation; 
       a simple description referring to CMFS and illuminant data */
    struct ColrSystem {
      uint observer_i   = 0;
      uint illuminant_i = 0;
      uint n_scatters   = 0;

      inline
      bool operator==(const ColrSystem &o) const {
        return std::tie(observer_i, illuminant_i, n_scatters) 
            == std::tie(o.observer_i, o.illuminant_i, o.n_scatters);
      }
    };

    /* Spectral basis function representation;
       The basis is offset around its mean */
    struct Basis {
      Spec mean;
      eig::Matrix<float, wavelength_samples, wavelength_bases> functions;

      inline
      bool operator==(const Basis &o) const {
        return mean.isApprox(o.mean) && functions.isApprox(o.functions);
      }

    public: // Serialization
      void to_stream(std::ostream &str) const {
        met_trace();
        io::to_stream(mean,      str);
        io::to_stream(functions, str);
      }

      void fr_stream(std::istream &str) {
        met_trace();
        io::fr_stream(mean,      str);
        io::fr_stream(functions, str);
      }
    };

  public: // State helpers
    struct ObjectState : public detail::ComponentStateBase<Object> {
      using Base = Object;
      using ComponentStateBase<Base>::m_mutated;
      
      detail::ComponentState<bool>               is_active;
      detail::ComponentState<uint>               mesh_i;
      detail::ComponentState<uint>               uplifting_i;
      detail::ComponentVariantState<Colr,  uint> diffuse;
      detail::ComponentVariantState<float, uint> roughness;
      detail::ComponentVariantState<float, uint> metallic;
      detail::ComponentVariantState<float, uint> opacity;
      detail::ComponentVariantState<Colr,  uint> normals;
      detail::ComponentState<eig::Affine3f>      trf;

    public:
      virtual bool update(const Base &o) override {
        m_mutated = (is_active.update(o.is_active)
                 ||  mesh_i.update(o.mesh_i)
                 ||  uplifting_i.update(o.uplifting_i)
                 ||  diffuse.update(o.diffuse)
                 ||  roughness.update(o.roughness)
                 ||  metallic.update(o.metallic)
                 ||  opacity.update(o.opacity)
                 ||  normals.update(o.normals)
                 ||  trf.update(o.trf));
        return m_mutated;
      }
    };
    

  public: // Scene data
    // Miscellaneous
    detail::Component<Settings,
                   detail::SettingsState> settings;   // Miscellaneous settings; e.g. texture size
    detail::Component<uint>               observer_i; // Primary observer index; simple enough for now

    // Scene components, directly visible or influential in the scene
    // On-disk, components are stored in json format
    struct {
      detail::ComponentVector<Object,
                             ObjectState> objects;
      detail::ComponentVector<Emitter>    emitters;
      detail::ComponentVector<Uplifting,
                  detail::UpliftingState> upliftings;
      detail::ComponentVector<ColrSystem> colr_systems;
    } components;

    // Scene resources, primarily referred to by components in the scene
    // On-disk, resources are stored in zlib-compressed binary format
    struct {
      detail::ResourceVector<AlMeshData>  meshes;
      detail::ResourceVector<Image>       images;
      detail::ResourceVector<Spec>        illuminants;
      detail::ResourceVector<CMFS>        observers;
      detail::ResourceVector<Basis>       bases;
    } resources;

  public: // Scene helper functions
    // Obtain the spectral data of a certain color system
    met::ColrSystem get_csys(uint i)       const;
    met::ColrSystem get_csys(ColrSystem c) const;

    // Obtain the spectral data of a certain emitter
    met::Spec get_emitter_spd(uint i)    const;
    met::Spec get_emitter_spd(Emitter e) const;

    // Obtain a pretty-printed name of a certain color system
    std::string get_csys_name(uint i)       const;
    std::string get_csys_name(ColrSystem c) const;
    
  public: // Serialization
    void to_stream(std::ostream &str) const;
    void fr_stream(std::istream &str);
  };
  
  namespace io {
    Scene load_scene(const fs::path &path);
    void  save_scene(const fs::path &path, const Scene &scene);
  } // namespace io
} // namespace met