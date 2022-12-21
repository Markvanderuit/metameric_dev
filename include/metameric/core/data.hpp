#pragma once

#include <metameric/core/spectrum.hpp>
#include <metameric/core/texture.hpp>
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace met {
  /* Save states in which project data can exist */
  enum class SaveFlag {
    eUnloaded, // Project is not currently loaded
    eNew,      // Project has no previous save, is newly created
    eSaved,    // Project has previous save, and has not been modified
    eUnsaved,  // Project has previous save, and has been modified
  };

  struct ProjectCreateInfo {
    struct ImageData {
      Texture2d3f image;
      uint cmfs, illuminant;
    };

    // Default constructor to fill in standard illuminants/cmfs
    ProjectCreateInfo();
    
    // Input uplifting information
    std::vector<ImageData> images; // Input images with known color system mappings
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
      uint mapp_i; // Index of the selected primary color system
      std::vector<Colr> colr_j; // The expected vertex colors under secondary color systems
      std::vector<uint> mapp_j; // Indices of the selected primary color systemss
    };

    // Set of indices of cmfs/illuminants together describing a stored color system
    struct Mapp {  uint cmfs, illuminant; };

    // Data structure for a triangle element of the project's convex hull mesh
    using Elem = eig::Array3u;
    
  public: /* public data */
    // Convex hull data structure used for rgb->spectral uplifting
    std::vector<Vert> gamut_verts;  // Gamut vertex values under specified color system   
    std::vector<Elem> gamut_elems;  // Triangle connections describing a convex hull
    std::vector<Mapp> mappings;

    // Named user- or program-provided illuminants and color matching functions
    std::vector<std::pair<std::string, Spec>> illuminants;
    std::vector<std::pair<std::string, CMFS>> cmfs;

  public: /* public methods */
    // Default constr. provides sensible default values
    ProjectData();

    // Obtain spectral data of a certain stored mapping
    met::Mapp mapping_data(uint i) const;
    met::Mapp mapping_data(Mapp m) const;
    
    // Obtain a pretty-printed name describing a certain stored mapping
    std::string mapping_name(uint i) const { return mapping_name(mappings[i]); }
    std::string mapping_name(Mapp m) const { return fmt::format("{}, {}", cmfs[m.cmfs].first, illuminants[m.illuminant].first); }
  };

  /* Wrapper to hold all major application data */
  struct ApplicationData {
  public: /* public data */
    // Saved project data
    fs::path     project_path;
    ProjectData  project_data;
    SaveFlag     project_save = SaveFlag::eUnloaded; 

    // Unsaved application data
    Texture2d3f         loaded_texture;  // RGB texture image extracted from project data

  public: /* public create/load/save methods */
    void create(ProjectCreateInfo &&info);      // Create project from info object
    void create(Texture2d3f &&texture);         // Create project from texture data
    void create(const fs::path &texture_path);  // Create project from texture at path
    void load(const fs::path &path);            // Load project data from path
    void save(const fs::path &path);            // Save project data to path
    void unload();                              // Unload project data
  
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

  public: /* misc public methods */
    void load_chull_gamut(); // Re-create project gamut based on a convex hull approximation of the input texture
  };
} // namespace met