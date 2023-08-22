#pragma once

#include <metameric/core/spectrum.hpp>
#include <metameric/core/texture.hpp>
#include <functional>
#include <variant>

namespace met {
  /* Save states for project data */
  enum class ProjectSaveState {
    eUnloaded, // Project is not currently loaded by application
    eNew,      // Project is newly created
    eSaved,    // Project has previous save, and has not been modified
    eUnsaved,  // Project has previous save, and has been modified
  };
  
  // FWD
  struct Scene;
  struct Uplifting;
  struct Project;

  /* Tesselated spectral uplifting representation and data layout;
     kept separate from Scene object, given its importance to the codebase */
  struct Uplifting {
  public: /* object data layout */
    // Mesh structure type defining how constraints are connected
    enum class MeshingType {
      // Points on a convex hull, 
      // with generalized barycentric coordinates to determine interior values
      eConvexHull, 

      // Points throughout color space, 
      // with a delaunay tetrahedralization to determine interior values
      eDelaunay      
    };

    // Constraint types, upheld by the uplifting
    enum class ConstraintType {
      // Color constraints, artist-provided or generated;
      // can be modified for metameric behavior
      eColorSystem,

      // Spectral measure, artist-provided;
      // is fixed, immutable
      eMeasurement
    };

    // The spectral uplifting consists primarily of user-configured constraints;
    // these can be either spectral measurements, or color values across color systems
    struct Constraint {
      ConstraintType type = ConstraintType::eColorSystem;
      
      // If ConstraintType::eMeasurement, this is the spectral constraint,
      // else this is generated from color constraint values
      Spec spec;

      // If ConstraintType::eColorSystem, these are the color constraints
      // else, these are generated from the measurement
      Colr              colr_i; // Expected color under a primary color system 
      uint              csys_i; // Index referring to the primary color system
      std::vector<Colr> colr_j; // Expected colors under secondary color systems
      std::vector<uint> csys_j; // Indices of the secondary color systems
    };

    // Set of indices of cmfs/illuminants together describing a stored color system
    struct ColorSystem { 
      uint cmfs_i       = 0;
      uint illuminant_i = 0;
      uint n_scatters   = 0; 

      friend auto operator<=>(const ColorSystem &, const ColorSystem &) = default;
    };

    using CSys = ColorSystem;
    using Vert = Constraint;
    using Elem = eig::Array3u;

  public: /* object data stores */
    MeshingType meshing_type;
    
    std::vector<Vert> verts; 
    std::vector<Elem> elems;
    std::vector<CSys> color_systems;
    
  public: /* Helper methods */
  };

  /* Simple indexed scene; no graph, just a library of objects and their dependencies;
     responsible for most program data */
  struct Scene {
    using Texture3f = Texture2d3f;
    using Texture1f = Texture2d1f;
    using Mesh      = AlignedMeshData;
    
    // Generic wrapper for an arbitrary named scene component
    template <typename Ty>
    struct SceneComponent {
      std::string name;
      Ty          data;
    };
    
  public: /* scene data layout */
    // Object representation; couldn't be simpler
    struct Object {
      // Objects hold indices to an underlying mesh, an underlying material, 
      // and a applied spectral uplifting
      uint mesh_i, material_i, uplifting_i;

      // Object position/rotation/scale captured in affine transform
      eig::Affine3f trf;
    };
    
    // Material representation; generic PBR layout
    struct Material {
      // Material components hold either a direct value, 
      // or indices to corresponding textures
      std::variant<Colr,  uint> diffuse;
      std::variant<float, uint> roughness;
      std::variant<float, uint> metallic;
      std::variant<float, uint> opacity;
    };
    
    // Point-light representation; position and a corresponding illuminant
    struct Emitter {
      eig::Array3f p            = 1.f;
      float        multiplier   = 1.f; // power multiplier
      uint         illuminant_i = 0;   // index to spectral illuminant
    };

  public: /* scene component stores */
    uint observer_i = 0; // Primary observer index; simple enough for now

    // Spectral objects, primarily for uplifting
    std::vector<SceneComponent<Uplifting>> upliftings;
    std::vector<SceneComponent<Spec>>      illuminants;
    std::vector<SceneComponent<CMFS>>      observers;

    // Scene objects, visible or referred in scene
    std::vector<SceneComponent<Object>>    objects;
    std::vector<SceneComponent<Emitter>>   emitters;
    std::vector<SceneComponent<Material>>  materials;

    // Data objects, primarily referred in scene
    std::vector<SceneComponent<Mesh>>      meshes;
    std::vector<SceneComponent<Texture3f>> textures_3f;
    std::vector<SceneComponent<Texture1f>> textures_1f;
  };
} // namespace met