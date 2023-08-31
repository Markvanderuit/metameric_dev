#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/image.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/uplifting.hpp>
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
       couldn't be simpler. A shape represented by a surface mesh, a surface
       material, and an accompanying uplifting to handle spectral data. */
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
    
    /* Material representation; 
       generic PBR layout; components either hold a direct value, or indices referring
       to corresponding texture resources */
    struct Material {
      std::variant<Colr,  uint> diffuse;
      std::variant<float, uint> roughness;
      std::variant<float, uint> metallic;
      std::variant<float, uint> opacity;
      std::variant<Colr, uint>  normals;

      inline 
      bool operator==(const Material &o) const {
        // Comparison can be a little unwieldy due to the different variant permutations
        // and Eigen's lack of a single-component operator==(); we can abuse memory instead
        bool r = std::tie(roughness, metallic, opacity) == std::tie(o.roughness, o.metallic, o.opacity);
        guard(r && diffuse.index() == o.diffuse.index() && normals.index() == o.normals.index(), false);
        switch (diffuse.index()) {
          case 0: r &= std::get<Colr>(diffuse).isApprox(std::get<Colr>(o.diffuse));
          case 1: r &= std::get<uint>(diffuse) == std::get<uint>(o.diffuse);
        }
        switch (normals.index()) {
          case 0: r &= std::get<Colr>(normals).isApprox(std::get<Colr>(o.normals));
          case 1: r &= std::get<uint>(normals) == std::get<uint>(o.normals);
        }
        return r;
      }
    };
    
    /* Emitter representation; 
       just a simple point light for now */
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

  public: // Scene data
    // Miscellaneous
    detail::Component<uint> observer_i; // Primary observer index; simple enough for now

    // Scene objects, directly visible or edited in the scene
    detail::ComponentVector<Object>     objects;
    detail::ComponentVector<Emitter>    emitters;
    detail::ComponentVector<Material>   materials;
    detail::ComponentVector<Uplifting,
                detail::UpliftingState> upliftings;
    detail::ComponentVector<ColrSystem> colr_systems;

    // Scene resources, primarily referred to by components in the scene
    detail::ResourceVector<AlMeshData>  meshes;
    detail::ResourceVector<DynamicImage>images;
    detail::ResourceVector<Spec>        illuminants;
    detail::ResourceVector<CMFS>        observers;
    detail::ResourceVector<Basis>       bases;

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