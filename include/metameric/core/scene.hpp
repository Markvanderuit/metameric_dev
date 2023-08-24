#pragma once

#include <metameric/core/io.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/core/uplifting.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/scene.hpp>
#include <variant>

namespace met {  
  /* Simple indexed scene; no graph, just a library of objects and their dependencies;
     responsible for most program data */
  struct Scene {
    /* Scene component.
       Wrapper for scene components. Stores an active flag, name,
       the component's data, and a specializable state tracker
       to detect internal changes to the component's data. */
    template <typename Ty, 
              typename State = detail::ComponentState<Ty>> 
              requires (std::derived_from<State, detail::ComponentStateBase<Ty>>)
    struct Component {
      bool        is_active = true;
      std::string name;
      Ty          value;
      State       state;

      friend 
      auto operator<=>(const Component &, const Component &) = default;
    };

    /* Scene resource.
       Wrapper for scene resources. Has a much coarser state
       tracking built in by encapsulating resource access in data() function,
       to prevent caching duplicates of large resources. */
    template <typename Ty>
    class Resource {
      bool m_stale = true; // Cache flag for tracking write-accesses to resource data
      Ty   m_value = { };  // Hidden resource data

    public:
      std::string name;
      fs::path    path;

    public:
      void set_stale(bool b) { m_stale = b; }
      bool is_stale() const { return m_stale; }

      constexpr
      const Ty &get() const { return m_value; }
      constexpr
      Ty &get() { set_stale(true); return m_value; }

      friend 
      auto operator<=>(const Resource &, const Resource &) = default;
    };
    
  public: /* Scene data layout */
    // Object representation; couldn't be simpler
    struct Object {
      // Indices to an underlying mesh+material, and an applied spectral uplifting
      uint mesh_i, material_i, uplifting_i;

      // Object position/rotation/scale are captured in an affine transform
      eig::Affine3f trf;

      inline 
      bool operator==(const Object &o) const {
        return trf.isApprox(o.trf)
            && std::tie(mesh_i, material_i, uplifting_i) 
            == std::tie(o.mesh_i, o.material_i, o.uplifting_i);
      }
    };
    
    // Material representation; generic PBR layout; components either hold a direct
    // value, or indices to corresponding textures
    struct Material {
      std::variant<Colr,  uint> diffuse;
      std::variant<float, uint> roughness;
      std::variant<float, uint> metallic;
      std::variant<float, uint> opacity;

      inline 
      bool operator==(const Material &o) const {
        // Comparison can be a little unwieldy due to the different variant permutations
        // and Eigen's lack of a single-component operator==(); we can abuse memory instead
        bool r = std::tie(roughness, metallic, opacity)
              == std::tie(o.roughness, o.metallic, o.opacity);
        guard(r && diffuse.index() == o.diffuse.index(), false);
        switch (diffuse.index()) {
          case 0: return r && std::get<Colr>(diffuse).isApprox(std::get<Colr>(o.diffuse));
          case 1: return r && std::get<uint>(diffuse) == std::get<uint>(o.diffuse);
          default: return r;
        }
      }
    };
    
    // Point-light representation
    struct Emitter {
      eig::Array3f p            = 1.f; // point light position
      float        multiplier   = 1.f; // power multiplier
      uint         illuminant_i = 0;   // index to spectral illuminant

      inline 
      bool operator==(const Emitter &o) const {
        return p.isApprox(o.p)
            && std::tie(multiplier, illuminant_i) 
            == std::tie(o.multiplier, o.illuminant_i);
      }
    };

    // A simplistic color system, described by indices to corresponding CMFS/illuminant data
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

    // Spectral basis functions, offset by the basis mean
    struct Basis {
      Spec mean;
      eig::Matrix<float, wavelength_samples, wavelength_bases> functions;

      inline
      bool operator==(const Basis &o) const {
        return mean.isApprox(o.mean) && functions.isApprox(o.functions);
      }
    };

  public: /* Scene data stores */
    // Miscellaneous
    Component<uint> observer_i; // Primary observer index; simple enough for now

    // Scene objects, directly visible or edited in the scene
    std::vector<Component<Object>>     objects;
    std::vector<Component<Emitter>>    emitters;
    std::vector<Component<Material>>   materials;
    std::vector<Component<Uplifting,
              detail::UpliftingState>> upliftings;
    std::vector<Component<ColrSystem>> colr_systems;

    // Scene resources, primarily referred to by components in the scene
    std::vector<Resource<AlMeshData>>  meshes;
    std::vector<Resource<Texture2d3f>> textures_3f;
    std::vector<Resource<Texture2d1f>> textures_1f;
    std::vector<Resource<Spec>>        illuminants;
    std::vector<Resource<CMFS>>        observers;
    std::vector<Resource<Basis>>       bases;

  public: /* Scene helper functions */
    // Obtain the spectral data of a certain color system
    met::ColrSystem get_csys(uint i)       const;
    met::ColrSystem get_csys(ColrSystem c) const;

    // Obtain the spectral data of a certain emitter
    met::Spec get_emitter_spd(uint i)    const;
    met::Spec get_emitter_spd(Emitter e) const;

    // Obtain a pretty-printed name of a certain color system
    std::string get_csys_name(uint i)       const;
    std::string get_csys_name(ColrSystem c) const;
  };
  
  namespace io {
    Scene load_scene(const fs::path &path);
    void  save_scene(const fs::path &path, const Scene &scene);
  } // namespace io
} // namespace met