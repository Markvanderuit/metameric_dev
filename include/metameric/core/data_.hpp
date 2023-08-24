  #pragma once

#include <metameric/core/io.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/texture.hpp>
#include <functional>
#include <variant>

namespace met {  
  /* Tesselated spectral uplifting representation and data layout;
     kept separate from Scene object, given its importance to the codebase */
  struct Uplifting {
    // A mesh structure defines how constraints are connected; e.g. as points
    // on a convex hull with generalized barycentrics for the interior, or points 
    // throughout color space with a delaunay tesselation connecting the interior
    enum class Type {
      eConvexHull, eDelaunay      
    } meshing_type;

    // The mesh structure connects a set of user-configured constraints;
    // these can be either spectral measurements, or color values across color systems
    struct Constraint {
      enum class Type {
        eColorSystem, eMeasurement
      } type = Type::eColorSystem;

      // If type == Type::eColorSystem, these are the color constraints,
      // else, these are generated from the measurement where necessary
      Colr              colr_i; // Expected color under a primary color system 
      uint              csys_i; // Index referring to the primary color system
      std::vector<Colr> colr_j; // Expected colors under secondary color systems
      std::vector<uint> csys_j; // Indices of the secondary color systems
      
      // If type == Type::eMeasurement, this holds the spectral constraint
      // else, this is generated from color constraint values where necessary
      Spec spec;
    };

  public: /* Object data stores */
    // Shorthands
    using Vert = Constraint;
    using Elem = eig::Array3u;

    uint              basis_i = 0; // Index of used underlying basis
    std::vector<Vert> verts;       // Vertices of uplifting mesh
    std::vector<Elem> elems;       // Elements of uplifting mesh
    
  public: /* Helper methods */
    // ...
  };

  /* Simple indexed scene; no graph, just a library of objects and their dependencies;
     responsible for most program data */
  struct Scene {
    // Generic wrapper for an arbitrary named component active in the scene
    template <typename Ty>
    struct Component {
      bool        is_active = true;
      std::string name;
      Ty          data;

      friend auto operator<=>(const Component &, const Component &) = default;
    };

    // Generic wrapper for an arbitrary named "loaded" resource used in the scene
    template <typename Ty>
    struct Resource {
      std::string name;
      fs::path    path;
      Ty          data;

      friend auto operator<=>(const Resource &, const Resource &) = default;
    };
    
  public: /* Scene data layout */
    // Object representation; couldn't be simpler
    struct Object {
      // Indices to an underlying mesh+material, and an applied spectral uplifting
      uint mesh_i, material_i, uplifting_i;

      // Object position/rotation/scale are captured in an affine transform
      eig::Affine3f trf;
    };
    
    // Material representation; generic PBR layout; components either hold a direct
    // value, or indices to corresponding textures
    struct Material {
      std::variant<Colr,  uint> diffuse;
      std::variant<float, uint> roughness;
      std::variant<float, uint> metallic;
      std::variant<float, uint> opacity;
    };
    
    // Point-light representation
    struct Emitter {
      eig::Array3f p            = 1.f; // point light position
      float        multiplier   = 1.f; // power multiplier
      uint         illuminant_i = 0;   // index to spectral illuminant
    };

    // A simplistic color system, described by indices to corresponding CMFS/illuminant data
    struct ColrSystem {
      uint observer_i   = 0;
      uint illuminant_i = 0;
      uint n_scatters   = 0; 

      friend auto operator<=>(const ColrSystem &, const ColrSystem &) = default;
    };

    // Spectral basis functions, offset by the basis mean
    struct Basis {
      Spec mean;
      eig::Matrix<float, wavelength_samples, wavelength_bases> functions;
    };

  public: /* Scene data stores */
    // Resource shorthands
    using Texture3f = Texture2d3f;
    using Texture1f = Texture2d1f;
    using Mesh      = AlignedMeshData;

    // Miscellaneous
    uint observer_i = 0; // Primary observer index; simple enough for now

    // Scene objects, directly visible or referred to in the scene
    std::vector<Component<Object>>     objects;
    std::vector<Component<Emitter>>    emitters;
    std::vector<Component<Material>>   materials;
    std::vector<Component<Uplifting>>  upliftings;
    std::vector<Component<ColrSystem>> colr_systems;

    // Scene resources, primarily referred to by components in the scene
    std::vector<Resource<Mesh>>        meshes;
    std::vector<Resource<Texture3f>>   textures_3f;
    std::vector<Resource<Texture1f>>   textures_1f;
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