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
  constexpr uint n_samples = 16384;

  namespace detail {
    using Kernel      = CGAL::Exact_predicates_inexact_constructions_kernel;
    using Point       = Kernel::Point_3;
    using SurfaceMesh = CGAL::Surface_mesh<Point>;

    struct BasicAlMesh {
      using VertType = eig::AlArray3f;
      using ElemType = eig::Array3u;

      std::vector<VertType> vertices;
      std::vector<ElemType> elements; 
    };

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

  } // namespace detail
  
  GenOCSTask::GenOCSTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void GenOCSTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    /* // Initialize objects for shader call
    constexpr uint wvl_div = ceil_div(wavelength_samples, 4u);
    m_program = {{ .type = gl::ShaderType::eCompute,
                   .path = "resources/shaders/gen_ocs/gen_ocs_cl.comp.spv_opt",
                   .is_spirv_binary = true }};
    m_dispatch = { .groups_x = ceil_div(n_samples, 256u / wvl_div), 
                   .bindable_program = &m_program };

    // Set up uniform buffer
    std::array<uint, 2> uniform_data = { n_samples, 0u };
    m_uniform_buffer = {{ .data = obj_span<std::byte>(uniform_data) }};

    // Initialize main buffers
    info.emplace_resource<gl::Buffer>("color_buffer", {
      .size  = static_cast<size_t>(n_samples) * sizeof(eig::AlArray3f)
    });
    info.emplace_resource<gl::Buffer>("spectrum_buffer", {
      .size = static_cast<size_t>(n_samples) * sizeof(Spec)
    }); */

    // Get shared resources
    auto &e_basis = info.get_resource<BMatrixType>(global_key, "pca_basis");

    // Generate a unit sphere as a test mesh
    // auto points_unal = detail::generate_unit_dirs<3>(256);
    // auto points = std::vector<AlColr>(range_iter(points_unal));
    // auto hull = detail::cgal_convex_hull(points);
    // auto mesh = detail::cgal_surface_to_mesh(hull);

    // Replace sampled vertices with an example metamer boundary
    auto unit_samples = detail::generate_unit_dirs<6>(256);
    auto basis  = e_basis.rightCols(wavelength_bases);
    Mapp mapp_i = { .cmfs = models::cmfs_cie_xyz, .illuminant = models::emitter_cie_d65  };
    Mapp mapp_j = { .cmfs = models::cmfs_cie_xyz, .illuminant = models::emitter_cie_fl11 };
    CMFS cmfs_i = mapp_i.finalize();
    CMFS cmfs_j = mapp_j.finalize();
    Colr signal = 0.5f;
    // auto boundary = generate_boundary_examp(cmfs_i, cmfs_j, unit_samples);
    auto boundary = generate_boundary(basis, cmfs_i, cmfs_j, signal, unit_samples);

    // Generate test mesh
    auto points = std::vector<AlColr>(range_iter(boundary));
    auto hull = detail::cgal_convex_hull(points);
    auto mesh = detail::cgal_surface_to_mesh(hull);

    using map_3f_1ui = std::unordered_map<eig::Array<float, 3, 1>, uint, 
                                          decltype(detail::matrix_hash), decltype(detail::matrix_equal)>;

    // Assign one index to every possible unique vertex
    map_3f_1ui vertex_source_ui(32, detail::matrix_hash, detail::matrix_equal);
    for (uint i = 0; i < boundary.size(); ++i) {
      auto vertex = boundary[i];
      guard_continue(!vertex_source_ui.contains(vertex));
      vertex_source_ui.insert({ vertex, i });
    }

    // Obtain index of vertices in their mesh order
    std::vector<uint> sorted_source_ui(mesh.vertices.size());
    std::transform(std::execution::par_unseq, range_iter(mesh.vertices), sorted_source_ui.begin(),
      [&](const auto &v) { return vertex_source_ui[v]; });

    // Obtain unique subset of original 6d samples using sorted indices
    std::vector<eig::Array<float, 6, 1>> unique_samples(sorted_source_ui.size());
    std::transform(std::execution::par_unseq, range_iter(sorted_source_ui), unique_samples.begin(),
      [&](const uint &i) { return unit_samples[i]; });

    // Re-generate metamer boundary with this subset and different parameters
    // Mapp mapp_k = { .cmfs = models::cmfs_cie_xyz, .illuminant = models::emitter_cie_fl11 };
    // CMFS cmfs_k = mapp_k.finalize();
    // boundary = generate_boundaryz(basis, cmfs_i, cmfs_j, Colr(0.5, 0.5, 0.5), unit_samples);
    // // boundary = generate_boundary_examp(cmfs_i, cmfs_k, unique_samples);
    // points = std::vector<AlColr>(range_iter(boundary));
    // mesh.vertices = points;

    // hull = detail::cgal_convex_hull(points);
    // mesh = detail::cgal_surface_to_mesh(hull);
    // if (mesh.elements.size() == 0)
    //   mesh.elements = { eig::Array3u(0) };
    // mesh = detail::cgal_surface_to_mesh(hull);

    fmt::print("{} -> {}, {}\n", unique_samples.size(), mesh.vertices.size(), mesh.elements.size());

    // Determine center of mesh (should be approaching 0, 0, 0)
    auto mesh_center = std::reduce(std::execution::par_unseq, range_iter(mesh.vertices), 
      eig::AlArray3f(0.f), [](const auto &a, const auto &b) { return (a + b).eval(); })
                  / static_cast<float>(mesh.vertices.size());

    // Insert example buffers
    info.emplace_resource<gl::Buffer>("ocs_buffer", { .data = cnt_span<const std::byte>(points) });
    info.emplace_resource<gl::Buffer>("ocs_verts",  { .data = cnt_span<const std::byte>(mesh.vertices) });
    info.emplace_resource<gl::Buffer>("ocs_elems",  { .data = cnt_span<const std::byte>(mesh.elements) });
    info.insert_resource<Colr>("ocs_centr", Colr(mesh_center));

    /* auto ocs_samples = detail::generate_unit_dirs<6>(128);
    info.insert_resource<std::vector<eig::Array<float, 6, 1>>>("ocs_samples", std::move(ocs_samples));
    info.insert_resource<gl::Buffer>("ocs_buffer", gl::Buffer());
    info.insert_resource<gl::Buffer>("ocs_verts", gl::Buffer());
    info.insert_resource<gl::Buffer>("ocs_elems", gl::Buffer());
    info.insert_resource<Colr>("ocs_centr", 0.f); */

    // m_stale = true;
  }

  void GenOCSTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Generate OCS texture only on relevant state change
    auto &e_gamut_idx = info.get_resource<int>("viewport", "gamut_selection");
    guard(e_gamut_idx >= 0);

    /* 

    // Get shared resources
    auto &i_ocs_samples  = info.get_resource<std::vector<eig::Array<float, 6, 1>>>("ocs_samples");
    auto &e_bases        = info.get_resource<BMatrixType>(global_key, "pca_basis");
    auto &e_app_data     = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_mappings     = e_app_data.loaded_mappings;
    auto &e_gamut_mapp_i = e_app_data.project_data.gamut_mapp_i;
    auto &e_gamut_mapp_j = e_app_data.project_data.gamut_mapp_j;
    auto &e_gamut_colr   = e_app_data.project_data.gamut_colr_i;
    auto &e_gamut_spec   = e_app_data.project_data.gamut_spec;

    // Obtain color system data
    CMFS system_i = e_mappings[e_gamut_mapp_i[e_gamut_idx]].finalize(e_gamut_spec[e_gamut_idx]);
    CMFS system_j = e_mappings[e_gamut_mapp_j[e_gamut_idx]].finalize(e_gamut_spec[e_gamut_idx]);
    Colr signal_i = e_gamut_colr[e_gamut_idx];

    // Generate metamer set boundary
    auto boundary = generate_boundary_colr(e_bases.rightCols(wavelength_bases), 
      system_i, system_j, signal_i, i_ocs_samples);
    std::vector<AlColr> boundary_al(range_iter(boundary));

    // Generate convex hull mesh representatioon of boundary
    auto ocs_mesh = detail::cgal_surface_to_mesh(detail::cgal_convex_hull(std::vector<AlColr>(range_iter(boundary))));
    Colr ocs_cntr = std::reduce(range_iter(ocs_mesh.vertices), AlColr(0.f), 
                  [](const auto &a, const auto &b) { return (a + b).eval(); })
                  / static_cast<float>(ocs_mesh.vertices.size());

    // Upload to buffer 
    // TODO no!
    info.get_resource<Colr>("ocs_centr")        = ocs_cntr;
    info.get_resource<gl::Buffer>("ocs_verts")  = {{ .data = cnt_span<const std::byte>(ocs_mesh.vertices) }};
    info.get_resource<gl::Buffer>("ocs_elems")  = {{ .data = cnt_span<const std::byte>(ocs_mesh.elements) }};
    info.get_resource<gl::Buffer>("ocs_buffer") = {{ .data = cnt_span<const std::byte>(boundary_al) }}; */
  }
}