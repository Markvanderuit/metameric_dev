#pragma once

#include <metameric/core/spectrum.hpp>
#include <metameric/core/texture.hpp>
#include <functional>

namespace met {
  /* Save states for project data */
  enum class ProjectSaveState {
    eUnloaded, // Project is not currently loaded by application
    eNew,      // Project has no previous save, is newly created
    eSaved,    // Project has previous save, and has not been modified
    eUnsaved,  // Project has previous save, and has been modified
  };

  /* Mesh structure types for project data */
  enum class ProjectMeshingType {
    // Points on a convex hull, 
    // with generalized barycentric coordinates to determine interior values
    eConvexHull, 

    // Points throughout color space, 
    // with a delaunay tetrahedralization to determine interior values
    eDelaunay      
  };

  /* Wrapper object to hold information for project instantiation */
  struct ProjectCreateInfo {
    // Internal image data structure
    struct ImageData {
      Texture2d3f image;
      uint cmfs, illuminant;
    };

    // Default constructor to fill in standard illuminants/cmfs
    ProjectCreateInfo();
    
    // Input images with known color systems
    std::vector<ImageData> images; 

    // Input uplifting information
    uint n_exterior_samples;  // Intended nr. of exterior (convex hull) samples
    uint n_interior_samples;  // Intended nr. of interior (image data) samples given input fitting
    
    // Input spectral information
    std::vector<std::pair<std::string, Spec>> illuminants;
    std::vector<std::pair<std::string, CMFS>> cmfs;

    // Input project type information
    ProjectMeshingType meshing_type;
  };

  /* Wrapper object to hold all saveable project data */
  struct ProjectData {
  public: /* project data structures */
    // Data structure for a single vertex of the project's convex hull mesh
    struct VertexData {
      Colr              colr_i; // The expected vertex color under a primary color system
      uint              csys_i; // Index of the selected primary color system
      std::vector<Colr> colr_j; // The expected vertex colors under secondary color systems
      std::vector<uint> csys_j; // Indices of the selected secondary color systems
    };

    // Set of indices of cmfs/illuminants together describing a stored color system
    struct CSys { 
      uint cmfs;
      uint illuminant;

      friend auto operator<=>(const CSys &, const CSys &) = default;
    };

    // Shorthands used throughout
    using InfoType = ProjectCreateInfo;
    using Vert     = VertexData;
    using Elem     = eig::Array3u;
    
  public: /* public data */
    // Project format information; e.g. convex hull with generalized barycentric coordinates
    ProjectMeshingType meshing_type;

    // Convex hull data structure used for rgb->spectral uplifting
    std::vector<Vert> verts; 
    std::vector<Elem> elems;

    // Named user- or program-provided illuminants and color matching functions
    std::vector<CSys> color_systems; // Stored color system data using the below illuminants/cmfs
    std::vector<std::pair<std::string, Spec>> illuminants;
    std::vector<std::pair<std::string, CMFS>> cmfs;

  public: /* public methods */
    // Obtain spectral data of a certain color system
    ColrSystem csys(uint i) const;
    ColrSystem csys(CSys m) const;
    
    // Obtain a pretty-printed name describing a certain color system
    std::string csys_name(uint i) const { return csys_name(color_systems[i]); }
    std::string csys_name(CSys m) const { return fmt::format("{}, {}", cmfs[m.cmfs].first, illuminants[m.illuminant].first); }
  };

  /* Wrapper to hold all major application data */
  struct ApplicationData {
  public: /* public data */
    // Saved project data
    ProjectData      project_data;
    fs::path         project_path;
    ProjectSaveState project_save = ProjectSaveState::eUnloaded; 
    
    // Misc application data
    Texture2d3f loaded_texture;     // F32 RGB image extracted from project data
    AlMesh  loaded_mesh;        // Indexed mesh data supplied by user at runtime for viewing
    Basis       loaded_basis;       // Basis functions obtained through PCA of measured spectra

  public: /* project management; create/load/save/clear */
    void create(ProjectCreateInfo &&info); // Create project from info object
    void load(const fs::path &path);       // Load project data from path
    void save(const fs::path &path);       // Save project data to path
    void clear();                         // Unload project data
  
  public: /* project history and data modification */
    struct ProjectMod {
      // Short name of performed action for undo/redo view
      std::string name;

      // Applied modification (and its reverse), stored in a function capture
      std::function<void(ProjectData &)> redo, undo;
    };
    
    std::vector<ProjectMod> mods;       // Stack of project data modifications
    int                     mod_i = -1; // Index of current last modification for undo/redo 

    void touch(ProjectMod &&mod); // Submit a modification to project data
    void redo_mod();              // Step forward one modification
    void undo_mod();              // Step back one modification
    void clear_mods();            // Clear entire modification state       

  public: /* application theming, not exactly important */
    enum class ColorMode { eDark, eLight } color_mode;
  };
} // namespace met