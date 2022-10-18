#include <metameric/components/pipeline/task_gen_ocs.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/linprog.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/pca.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/utility.hpp>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/convex_hull_3.h>
#include <CGAL/Polyhedron_3.h>
#include <CGAL/Surface_mesh.h>
#include <omp.h>
#include <algorithm>
#include <execution>
#include <limits>
#include <map>
#include <numbers>
#include <random>
#include <ranges>
#include <unordered_map>
#include <unordered_set>

namespace met {
  constexpr uint n_samples = 128;

  namespace detail {
    using Kernel      = CGAL::Exact_predicates_inexact_constructions_kernel;
    using Point       = Kernel::Point_3;
    using SurfaceMesh = CGAL::Surface_mesh<Point>;

    /**
     * Given a set of vertices in 3d space, compute the convex hull around these vertices as a
     * indexed mesh.
     * 
     * Src: https://github.com/libigl/libigl/blob/main/include/igl/copyleft/cgal/convex_hull.cpp#L39
     * and  https://github.com/libigl/libigl/blob/main/include/igl/copyleft/cgal/polyhedron_to_mesh.cpp
    */
    SurfaceMesh cgal_convex_hull(const std::vector<eig::AlArray3f> &inp) {
      met_trace();

      // Transform to eigen point data into CGAL's format
      std::vector<Point> p(inp.size());
      std::transform(std::execution::par_unseq, range_iter(inp), p.begin(),
        [](const auto &p) { return Point(p.x(), p.y(), p.z()); });

      // Perform convex hull operation to generate a sf mesh
      SurfaceMesh sf;
      CGAL::convex_hull_3(range_iter(p), sf);
      return sf;
    }

    BasicAlMesh cgal_surface_to_mesh(const SurfaceMesh &sf) {
      met_trace();

      // Allocate output buffers
      std::vector<eig::AlArray3f> verts(sf.num_vertices());
      std::vector<eig::Array3u>   elems(sf.num_faces());
      
      // Transform vertex point data and face index data into output format
      std::transform(std::execution::par_unseq, range_iter(sf.points()), verts.begin(),
        [](const auto &p) { return eig::AlArray3f(p.x(), p.y(), p.z()); });
      std::transform(std::execution::par_unseq, range_iter(sf.faces()),  elems.begin(), [&](auto &f) {
        eig::Array3u v;
        std::ranges::copy(sf.vertices_around_face(sf.halfedge(f)), v.begin());
        return v;
      });

      return { std::move(verts), std::move(elems) };
    }

    constexpr float gaussian_epsilon   = .0001f;
    constexpr float gaussian_alpha     = 1.f;
    constexpr float gaussian_alpha_inv = 1.f / gaussian_alpha;
    constexpr float gaussian_k         = 2.f / (std::numbers::pi_v<float> * gaussian_alpha);

    // Given a random vector in R3 bounded to [-1, 1], return a vector
    // distributed over a gaussian distribution
    template <uint N>
    eig::Array<float, N, 1> inv_gaussian_cdf(const eig::Array<float, N, 1> &x) {
      using ArrayNf = eig::Array<float, N, 1>;
      met_trace();

      auto y = (ArrayNf(1.f) - x * x).max(gaussian_epsilon).log().eval();
      auto z = (ArrayNf(gaussian_k) + 0.5f * y).eval();
      return (((z * z - y * gaussian_alpha_inv).sqrt() - z).sqrt() * x.sign()).eval();
    }
    
    // Given a random vector in R3 bounded to [-1, 1], return a uniformly
    // distributed point on the unit sphere
    template <uint N>
    eig::Array<float, N, 1> inv_unit_sphere_cdf(const eig::Array<float, N, 1> &x) {
      met_trace();
      return inv_gaussian_cdf<N>(x).matrix().normalized().eval();
    }

    template <uint N>
    std::vector<eig::Array<float, N, 1>> generate_unit_dirs(uint n_samples) {
      using ArrayNf = eig::Array<float, N, 1>;
      met_trace();

      // Generate separate seeds for each thread's rng
      std::random_device rd;
      using SeedTy = std::random_device::result_type;
      std::vector<SeedTy> seeds(omp_get_max_threads());
      for (auto &s : seeds) s = rd();

      std::vector<ArrayNf> unit_dirs(n_samples);
      #pragma omp parallel
      {
        // Initialize separate random number generator per thread
        std::mt19937 eng(seeds[omp_get_thread_num()]);
        std::uniform_real_distribution<float> distr(-1.f, 1.f);

        // Draw samples for this thread's range
        #pragma omp for
        for (int i = 0; i < unit_dirs.size(); ++i) {
          ArrayNf v;
          for (auto &f : v) f = distr(eng);

          unit_dirs[i] = detail::inv_unit_sphere_cdf<N>(v);
        }
      }

      return unit_dirs;
    }

    // Hash and key_equal for eigen types for std::unordered_map insertion
    constexpr auto matrix_hash = [](const auto &mat) {
      size_t seed = 0;
      for (size_t i = 0; i < mat.size(); ++i) {
        auto elem = *(mat.data() + i);
        seed ^= std::hash<float>()(elem) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      }
      return seed;
    };
    constexpr auto matrix_equal = [](const auto &a, const auto &b) { return a.isApprox(b); };

    BasicAlMesh generate_uv_sphere(const uint subdiv_depth = 3) {
      met_trace();

      using Vt = BasicAlMesh::VertType;
      using El = BasicAlMesh::ElemType;
      using VMap  = std::unordered_map<Vt, uint, decltype(matrix_hash), decltype(matrix_equal)>;

      // Initial mesh describes an octahedron
      std::vector<Vt> vts = { Vt(-1.f, 0.f, 0.f ), Vt( 0.f,-1.f, 0.f ), Vt( 0.f, 0.f,-1.f ),
                              Vt( 1.f, 0.f, 0.f ), Vt( 0.f, 1.f, 0.f ), Vt( 0.f, 0.f, 1.f ) };
      std::vector<El> els = { El(0, 1, 2), El(3, 2, 1), El(0, 5, 1), El(3, 1, 5),
                              El(0, 4, 5), El(3, 5, 4), El(0, 2, 4), El(3, 4, 2) };

      // Perform loop subdivision several times
      for (uint d = 0; d < subdiv_depth; ++d) {        
        std::vector<El> els_(4 * els.size()); // New elements are inserted in this larger vector
        VMap vmap; // Identical vertices are first tested in this map

        #pragma omp parallel for
        for (int e = 0; e < els.size(); ++e) {
          // Old and new vertex indices
          eig::Array3u ijk = els[e], abc;
          
          // Compute edge midpoints, lifted to the unit sphere
          std::array<Vt, 3> new_vts = { (vts[ijk[0]] + vts[ijk[1]]).matrix().normalized(),
                                        (vts[ijk[1]] + vts[ijk[2]]).matrix().normalized(),
                                        (vts[ijk[2]] + vts[ijk[0]]).matrix().normalized() };

          // Inside critical section, insert lifted edge midpoints and set new vertex indices
          // if they don't exist already on a neighbouring triangle
          #pragma omp critical
          for (uint i = 0; i < abc.size(); ++i) {
            if (auto it = vmap.find(new_vts[i]); it != vmap.end()) {
              abc[i] = it->second;
            } else {
              abc[i] = vts.size();
              vts.push_back(new_vts[i]);
              vmap.emplace(new_vts[i], abc[i]);
            }
          }
        
          // Create and insert newly subdivided elements
          const auto new_els = { El(ijk[0], abc[0], abc[2]), El(ijk[1], abc[1], abc[0]), 
                                 El(ijk[2], abc[2], abc[1]), El(abc[0], abc[1], abc[2]) };
          std::ranges::copy(new_els, els_.begin() + 4 * e);
        }

        els = els_; // Overwrite list of elements with new subdivided list
      }      

      return { std::move(vts), std::move(els) };
    }

    BasicAlMesh generate_approximate_hull(BasicAlMesh mesh, const std::vector<Colr> &points) {
      met_trace();

      // For each vertex in mesh, each defining a line through the origin:
      std::for_each(std::execution::par_unseq, range_iter(mesh.vertices), [&](auto &v) {
        // Obtain a range of point projections along this line
        auto proj_funct  = [&v](const auto &p) { return v.matrix().dot(p.matrix()); };
        auto proj_range = points | std::views::transform(proj_funct);

        // Find iterator to endpoint, given projections
        auto it = std::ranges::max_element(proj_range);

        // Replace mesh vertex with this endpoint
        v = *(points.begin() + std::distance(proj_range.begin(), it));
      });

      /* // Find and erase collapsed triangles
      fmt::print("pre_erase {}\n", mesh.elements.size());
      std::erase_if(mesh.elements, [&](const auto &e) {
        const uint i = e.x(), j = e.y(), k = e.z();
        return (mesh.vertices[i].isApprox(mesh.vertices[j])) ||
               (mesh.vertices[j].isApprox(mesh.vertices[k])) ||
               (mesh.vertices[k].isApprox(mesh.vertices[i]));
      });
      fmt::print("post_erase {}\n", mesh.elements.size()); */

      // Find and erase inward-pointing triangles
      /* std::erase_if(mesh.elements, [&](const auto &e) {
      }); */

      return mesh;
    }
  } // namespace detail
  
  GenOCSTask::GenOCSTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void GenOCSTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Generate reused 6d samples and a uv sphere mesh for faster OCS hull generation
    m_sphere_mesh = detail::generate_uv_sphere();
    m_sphere_samples = detail::generate_unit_dirs<6>(n_samples);

    // Register ocs buffers
    constexpr auto create_flags = gl::BufferCreateFlags::eStorageDynamic;
    info.emplace_resource<gl::Buffer>("ocs_verts", { 
      .size = sizeof(eig::AlArray3f) * m_sphere_mesh.vertices.size(), .flags = create_flags });
    info.emplace_resource<gl::Buffer>("ocs_elems", { 
      .size = sizeof(eig::Array3u) * m_sphere_mesh.elements.size(), .flags = create_flags });
    info.emplace_resource<gl::Buffer>("ocs_buffer", { 
      .size = sizeof(eig::AlArray3f) * m_sphere_samples.size(), .flags = create_flags });
    info.insert_resource<Colr>("ocs_centr", Colr(0.f));
    
    // Set last accessed gamut to "none"
    m_gamut_idx = -1;
  }

  void GenOCSTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get shared resources to verify state change
    auto &e_state_gamut = info.get_resource<std::array<CacheState, 4>>("project_state", "gamut_summary");
    auto &e_gamut_idx   = info.get_resource<int>("viewport", "gamut_selection");

    // Generate OCS only on relevant state changes
    guard(e_gamut_idx >= 0);
    guard(m_gamut_idx != e_gamut_idx || e_state_gamut[e_gamut_idx] == CacheState::eStale);

    // Cache last accessed gamut idx
    m_gamut_idx = e_gamut_idx;

    // Get shared resources
    auto &e_basis       = info.get_resource<BMatrixType>(global_key, "pca_basis");
    auto &e_app_data    = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_gamut_colr_i  = e_app_data.project_data.gamut_colr_i[e_gamut_idx];
    auto &e_gamut_mapp_i  = e_app_data.project_data.gamut_mapp_i[e_gamut_idx];
    auto &e_gamut_mapp_j  = e_app_data.project_data.gamut_mapp_j[e_gamut_idx];
    auto &e_mapp_i        = e_app_data.loaded_mappings[e_gamut_mapp_i];
    auto &e_mapp_j        = e_app_data.loaded_mappings[e_gamut_mapp_j];

    // Generate color systems
    auto basis  = e_basis.rightCols(wavelength_bases);
    CMFS cmfs_i = e_mapp_i.finalize();
    CMFS cmfs_j = e_mapp_j.finalize();

    // Generate metamer set convex hull 
    auto points = generate_boundary(basis, cmfs_i, cmfs_j, e_gamut_colr_i, m_sphere_samples);
    auto ocs_points = std::vector<AlColr>(range_iter(points));
    auto ocs_hull   = detail::generate_approximate_hull(m_sphere_mesh, points);
    auto ocs_center = std::reduce(std::execution::par_unseq, range_iter(ocs_hull.vertices), 
      eig::AlArray3f(0.f), [](const auto &a, const auto &b) { return (a + b).eval(); })
                  / static_cast<float>(ocs_hull.vertices.size());

    // Update buffers
    auto &i_ocs_buffer = info.get_resource<gl::Buffer>("ocs_buffer");
    auto &i_ocs_verts  = info.get_resource<gl::Buffer>("ocs_verts");
    auto &i_ocs_elems  = info.get_resource<gl::Buffer>("ocs_elems");
    auto &i_ocs_center = info.get_resource<Colr>("ocs_centr");

    i_ocs_buffer.set(cnt_span<const std::byte>(ocs_points));
    i_ocs_verts.set(cnt_span<const std::byte>(ocs_hull.vertices));
    i_ocs_elems.set(cnt_span<const std::byte>(ocs_hull.elements));
    i_ocs_center = ocs_center;
  }
}