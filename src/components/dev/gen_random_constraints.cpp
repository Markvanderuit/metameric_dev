#include <metameric/core/data.hpp>
#include <metameric/core/distribution.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/components/dev/gen_random_constraints.hpp>
#include <omp.h>
#include <execution>
#include <numbers>

namespace met {
  constexpr uint n_img_samples = 16; // Nr. of images to generate
  constexpr uint n_ocs_samples = 64; // Nr. of samples for color system OCS generation

  namespace detail {
    // Given a random vector in RN bounded to [-1, 1], return a vector
    // distributed over a gaussian distribution
    inline
    auto inv_gaussian_cdf(const auto &x) {
      met_trace();
      auto y = (-(x * x) + 1.f).max(.0001f).log().eval();
      auto z = (0.5f * y + (2.f / std::numbers::pi_v<float>)).eval();
      return (((z * z - y).sqrt() - z).sqrt() * x.sign()).eval();
    }
    
    // Given a random vector in RN bounded to [-1, 1], return a uniformly
    // distributed point on the unit sphere
    inline
    auto inv_unit_sphere_cdf(const auto &x) {
      met_trace();
      return inv_gaussian_cdf(x).matrix().normalized().eval();
    }

    // Generate a set of random, uniformly distributed unit vectors in RN
    inline
    std::vector<eig::ArrayXf> gen_unit_dirs_x(uint n_samples, uint n_dims) {
      met_trace();

      std::vector<eig::ArrayXf> unit_dirs(n_samples);
      
      #pragma omp parallel
      {
        // Draw samples for this thread's range with separate sampler per thread
        UniformSampler sampler(-1.f, 1.f, static_cast<uint>(omp_get_thread_num()));
        #pragma omp for
        for (int i = 0; i < unit_dirs.size(); ++i)
          unit_dirs[i] = detail::inv_unit_sphere_cdf(sampler.next_nd(n_dims));
      }

      return unit_dirs;
    }
  } // namespace detail

  void GenRandomConstraintsTask::init(SchedulerHandle &info) {
    met_trace();

    info("constraints").set<std::vector<std::vector<ProjectData::Vert>>>({ });

    m_has_run_once = false;
  }

  bool GenRandomConstraintsTask::is_active(SchedulerHandle &info) {
    met_trace();

    // Get external resources
    const auto &e_appl_data = info.global("appl_data").read_only<ApplicationData>();
    const auto &e_proj_data = e_appl_data.project_data;

    return e_proj_data.color_systems.size() > 1 
      && e_proj_data.color_systems[1].illuminant != 0 
      && !m_has_run_once;
  }

  void GenRandomConstraintsTask::eval(SchedulerHandle &info) {
    met_trace();
    
    // Get external resources
    const auto &e_appl_data = info.global("appl_data").read_only<ApplicationData>();
    const auto &e_proj_data = e_appl_data.project_data;
    const auto &e_verts     = e_proj_data.verts;

    // Get modified resources
    auto &i_constraints = info("constraints").writeable<std::vector<std::vector<ProjectData::Vert>>>();

    // Resize constraints data to correct format
    i_constraints.resize(n_img_samples);
    for (auto &constraints : i_constraints)
      constraints.resize(e_verts.size());

    // Provide items necessary for fast OCS generation
    auto samples_6d = detail::gen_unit_dirs_x(n_ocs_samples, 6);
    std::vector<CMFS> cmfs_i = { e_proj_data.csys(0).finalize_direct() }; // TODO ehhr
    std::vector<CMFS> cmfs_j = { e_proj_data.csys(1).finalize_direct() }; // TODO uhhr

    // Iterate through base vertex data step-by-step
    for (uint i = 0; i < e_verts.size(); ++i) {
      const auto &vert = e_verts[i];

      // Provide items necessary for OCS generation
      std::vector<Colr> sign_i = { vert.colr_i };

      // Generate boundary points over mismatch volume; these points lie on a convex hull
      auto ocs_gen_data = generate_mismatch_boundary({ .basis      = e_appl_data.loaded_basis, 
                                                       .basis_mean = e_appl_data.loaded_basis_mean, 
                                                       .systems_i  = cmfs_i, 
                                                       .signals_i  = sign_i, 
                                                       .system_j   = cmfs_j.front(), 
                                                       .samples    = samples_6d });
    
      // Compute center of convex hull
      constexpr auto f_add = [](const auto &a, const auto &b) { return (a + b).eval(); };
      Colr center = std::reduce(std::execution::par_unseq, range_iter(ocs_gen_data), Colr(0.f), f_add)
                  / static_cast<float>(ocs_gen_data.size());
      Colr closest = *std::min_element(range_iter(ocs_gen_data), [&](const eig::Vector3f &a, const eig::Vector3f &b) {
        return (a - center.matrix()).squaredNorm() < (b - center.matrix()).squaredNorm();
      });

      // Generate a delaunay tesselation of the convex hull, or collapse to a point
      std::vector<eig::Array3f> del_verts;
      std::vector<eig::Array4u> del_elems;
      if ((closest - center).matrix().norm() > 0.01f) {
        auto [verts, elems] = generate_delaunay<IndexedDelaunayData, eig::Array3f>(ocs_gen_data);
        std::tie(del_verts, del_elems) = { verts, elems };
        del_verts = verts;
        del_elems = elems;
      } else {
        del_verts = { center };
        del_elems = { };
      }

      // Compute volume of each tetrahedron in delaunay
      std::vector<float> del_volumes(del_elems.size());
      std::transform(std::execution::par_unseq, range_iter(del_elems), del_volumes.begin(),
      [&](const eig::Array4u &el) {
        // Get vertex positions for this tetrahedron
        std::array<eig::Vector3f, 4> p;
        std::ranges::transform(el, p.begin(), [&](uint i) { return del_verts[i]; });

        // Compute tetrahedral volume
        float nom = std::abs((p[0] - p[3]).dot((p[1] - p[3]).cross(p[2] - p[3])));
        return nom / 6.f;
      });

      // Components for sampling step
      UniformSampler sampler;
      Distribution volume_distr(del_volumes);

      // Start drawing samples
      for (uint j = 0; j < n_img_samples; ++j) {
        if (del_elems.empty()) {
          i_constraints[j][i] = ProjectData::Vert {
            .colr_i = vert.colr_i, 
            .csys_i = 0,           
            .colr_j = { del_verts[0] },
            .csys_j = { 1 }
          };

          continue;
        }

        // First, sample barycentric weights uniformly inside a tetrahedron
        // Src: https://vcg.isti.cnr.it/jgt/tetra.htm
        auto sample_3d = sampler.next_nd<3>();
        if (sample_3d.head<2>().sum() > 1.f) {
          sample_3d.head<2>() = 1.f - sample_3d.head<2>();
        }
        if (sample_3d.tail<2>().sum() > 1.f) {
          float t = sample_3d.z();
          sample_3d.z() = 1.f - sample_3d.head<2>().sum();
          sample_3d.y() = 1.f - t;
        } else if (sample_3d.sum() > 1.f) {
          float t = sample_3d.z();
          sample_3d.z() = sample_3d.sum() - 1.f;
          sample_3d.x() = 1.f - sample_3d.y() - t;
        }

        // Next, sample a tetrahedron uniformly based on volume, and grab its vertices
        std::array<eig::Vector3f, 4> p;
        std::ranges::transform(del_elems[volume_distr.sample(sampler.next_1d())], p.begin(), 
          [&](uint i) { return del_verts[i]; });

        // Recover sample position using the generated barycentric coordinates
        eig::Array3f v = p[0] * (1.f - sample_3d.sum())
                       + p[1] * sample_3d.x() + p[2] * sample_3d.y() + p[3] * sample_3d.z();
        
        // Store resulting sample vertex
        i_constraints[j][i] = ProjectData::Vert {
          .colr_i = vert.colr_i, 
          .csys_i = 0,           
          .colr_j = { v },
          .csys_j = { 1 }
        };
      }
    }

    m_has_run_once = true;
  }
} // namespace met