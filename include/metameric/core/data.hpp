#pragma once

#include <metameric/core/spectrum.hpp>
#include <metameric/core/tree.hpp>
#include <metameric/core/texture.hpp>
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace met {
  /* Color states in which application can exist */
  enum class AppColorMode { eDark, eLight };

  /* Save states in which project data can exist */
  enum class SaveFlag {
    eUnloaded, // Project is not currently loaded
    eNew,      // Project has no previous save, is newly created
    eSaved,    // Project has previous save, and has not been modified
    eUnsaved,  // Project has previous save, and has been modified
  };

  /* Wrapper object to hold information for project instantiation */
  struct ProjectCreateInfo {
    struct ImageData {
      Texture2d3f image;
      uint cmfs, illuminant;
    };

    // Default constructor to fill in standard illuminants/cmfs
    ProjectCreateInfo();
    
    // Input uplifting information
    std::vector<ImageData> images; // Input images with known color systems
    uint n_vertices;               // Intended nr. of vertices for convex hull estimation
    
    // Input spectral information
    std::vector<std::pair<std::string, Spec>> illuminants;
    std::vector<std::pair<std::string, CMFS>> cmfs;
  };

  /* Wrapper object to hold all saveable project data */
  struct ProjectData {
  public: /* internal data structures */
    // Data structure for a single vertex of the project's convex hull mesh
    struct Vert {
      Colr colr_i; // The expected vertex color under a primary color system
      uint csys_i; // Index of the selected primary color system
      std::vector<Colr> colr_j; // The expected vertex colors under secondary color systems
      std::vector<uint> csys_j; // Indices of the selected secondary color systems
    };

    // Set of indices of cmfs/illuminants together describing a stored color system
    struct CSys { uint cmfs, illuminant; };

    // Data structure for a triangle element of the project's convex hull mesh
    using Elem = eig::Array3u;
    
  public: /* public data */
    // Convex hull data structure used for rgb->spectral uplifting
    std::vector<Vert> gamut_verts;   // Gamut vertex data  
    std::vector<Elem> gamut_elems;   // Gamut element data, forming a convex hull
    std::vector<CSys> color_systems; // Stored color system data using the below illuminants/cmfs

    // Named user- or program-provided illuminants and color matching functions
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
    ProjectData project_data;
    fs::path    project_path;
    SaveFlag    project_save = SaveFlag::eUnloaded; 

    // Misc application data
    Texture2d3f   loaded_texture;    // Primary RGB texture image extracted from project data
    Basis         loaded_basis;      // Set of basis functions obtained through PCA of measured spectra
    Spec          loaded_basis_mean; // Set of basis functions obtained through PCA of measured spectra
    BasisTreeNode loaded_tree_root;  // Basis function tree structure, loaded from disk
    AppColorMode  color_mode;        // Application theming

  public: /* create/load/save methods */
    void create(ProjectCreateInfo &&info); // Create project from info object
    void load(const fs::path &path);       // Load project data from path
    void save(const fs::path &path);       // Save project data to path
    void unload();                         // Unload project data
  
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
    void redo();                  // Step forward one modification
    void undo();                  // Step back one modification

  public: /* project solve functions */
    void gen_convex_hull(uint n_vertices);
    void gen_constraints_from_images(std::span<const ProjectCreateInfo::ImageData> images);
  };
} // namespace met