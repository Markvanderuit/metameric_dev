#include <metameric/core/data.hpp>
#include <metameric/core/io.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/data.hpp>
#include <nlohmann/json.hpp>
#include <omp.h>
#include <algorithm>
#include <chrono>
#include <execution>
#include <mutex>
#include <numbers>
#include <ranges>
#include <random>
#include <unordered_map>

namespace met {
  namespace detail {
    // Given a random vector in RN bounded to [-1, 1], return a vector
    // distributed over a gaussian distribution
    auto inv_gaussian_cdf(const auto &x) {
      met_trace();
      auto y = (-(x * x) + 1.f).max(.0001f).log().eval();
      auto z = (0.5f * y + (2.f / std::numbers::pi_v<float>)).eval();
      return (((z * z - y).sqrt() - z).sqrt() * x.sign()).eval();
    }
    
    // Given a random vector in RN bounded to [-1, 1], return a uniformly
    // distributed point on the unit sphere
    auto inv_unit_sphere_cdf(const auto &x) {
      met_trace();
      return inv_gaussian_cdf(x).matrix().normalized().eval();
    }

    // Generate a set of random, uniformly distributed unit vectors in RN
    template <uint N>
    std::vector<eig::Array<float, N, 1>> gen_unit_dirs(uint n_interior_samples) {
      met_trace();
      
      using ArrayNf = eig::Array<float, N, 1>;
      using SeedTy = std::random_device::result_type;

      // Generate separate seeds for each thread's rng
      std::random_device rd;
      std::vector<SeedTy> seeds(omp_get_max_threads());
      for (auto &s : seeds) s = rd();

      std::vector<ArrayNf> unit_dirs(n_interior_samples);
      #pragma omp parallel
      {
        // Initialize separate random number generator per thread
        std::mt19937 rng(seeds[omp_get_thread_num()]);
        std::uniform_real_distribution<float> distr(-1.f, 1.f);

        // Draw samples for this thread's range
        #pragma omp for
        for (int i = 0; i < unit_dirs.size(); ++i) {
          ArrayNf v;
          for (auto &f : v) f = distr(rng);
          unit_dirs[i] = detail::inv_unit_sphere_cdf(v);
        }
      }

      return unit_dirs;
    }
  } // namespace detail

  namespace io {
    ProjectData load_project(const fs::path &path) {
      met_trace();
      return load_json(path).get<ProjectData>();
    }

    void save_project(const fs::path &path, const ProjectData &data) {
      met_trace();
      save_json(path, data);
    }
  } // namespace io
  
  ProjectCreateInfo::ProjectCreateInfo()
  : n_exterior_samples(6),
    n_interior_samples(128),
    meshing_type(ProjectMeshingType::eDelaunay),
    illuminants({{ "D65",      models::emitter_cie_d65     },
                 { "E",        models::emitter_cie_e       },
                 { "FL2",      models::emitter_cie_fl2     },
                 { "FL11",     models::emitter_cie_fl11    },
                 { "LED-RGB1", models::emitter_cie_ledrgb1 }}),
    cmfs({{ "CIE XYZ", models::cmfs_cie_xyz  }}) { }

  ColrSystem ProjectData::csys(uint i) const {
    met_trace();
    return csys(color_systems[i]);
  }

  ColrSystem ProjectData::csys(CSys m) const {
    met_trace();
    return { .cmfs = cmfs[m.cmfs].second,
             .illuminant = illuminants[m.illuminant].second,
             .n_scatters = m.n_scatters };
  }
  
  void ApplicationData::create(ProjectCreateInfo &&info) {
    met_trace();

    fmt::print("Project generation\n");
    debug::check_expr_rel(!info.images.empty(), "ProjectCreateInfo::images must not be empty");

    clear_mods();

    // Set new project data
    project_save = ProjectSaveState::eNew;
    project_path = ""; // TBD on first save
    project_data = ProjectData();

    // Copy over newish create info and assign color systems
    project_data.meshing_type = info.meshing_type;
    project_data.cmfs         = info.cmfs;
    project_data.illuminants  = info.illuminants;
    project_data.color_systems.clear();
    for (auto &image : info.images)
      project_data.color_systems.push_back({ .cmfs = image.cmfs, 
                                             .illuminant = image.illuminant, 
                                             .n_scatters = 1 });

    // Move first texture into application - not project - data; later stored in separate file
    loaded_texture = std::move(info.images[0].image);
    info.images.erase(info.images.begin());

    // Generate initial geometric data structure; if extra input images are provided,
    // start fitting constraints
    detail::init_convex_hull(*this, info.n_exterior_samples);
    if (!info.images.empty())
      detail::init_constraints(*this, info.n_interior_samples, info.images);
  }
  
  void ApplicationData::save(const fs::path &path) {
    met_trace();

    project_save = ProjectSaveState::eSaved;
    project_path = io::path_with_ext(path, ".json");

    io::save_json(project_path, project_data);
    io::save_texture2d(io::path_with_ext(project_path, ".exr"), loaded_texture, true);
  }

  void ApplicationData::load(const fs::path &path) {
    met_trace();

    clear_mods();
    
    project_save   = ProjectSaveState::eSaved;
    project_path   = io::path_with_ext(path, ".json");
    project_data   = io::load_json(path).get<ProjectData>();
    loaded_texture = io::load_texture2d<Colr>(io::path_with_ext(project_path,".exr"), true);
  }


  void ApplicationData::unload() {
    met_trace();

    clear_mods();

    project_save = ProjectSaveState::eUnloaded;
    project_path  = "";
    project_data  = { };
    loaded_texture  = { };
  }  

  void ApplicationData::touch(ProjectMod &&mod) {
    met_trace();

    // Apply change
    mod.redo(project_data);

    // Ensure mod list doesn't exceed fixed length
    // and set the current mod to its end
    int n_mods = std::clamp(mod_i + 1, 0, 128);
    mod_i = n_mods;
    mods.resize(mod_i);
    mods.push_back(mod);   
    
    if (project_save == ProjectSaveState::eSaved) {
      project_save = ProjectSaveState::eUnsaved;
    }
  }

  void ApplicationData::redo_mod() {
    met_trace();
    
    guard(mod_i < (int(mods.size()) - 1));
    
    mod_i += 1;
    mods[mod_i].redo(project_data);

    if (project_save == ProjectSaveState::eSaved) {
      project_save = ProjectSaveState::eUnsaved;
    }
  }

  void ApplicationData::undo_mod() {
    met_trace();
    
    guard(mod_i >= 0);

    mods[mod_i].undo(project_data);
    mod_i -= 1;

    if (project_save == ProjectSaveState::eSaved) {
      project_save = ProjectSaveState::eUnsaved;
    }
  }

  void ApplicationData::clear_mods() {
    met_trace();

    mods  = { };
    mod_i = -1;
  }
} // namespace met