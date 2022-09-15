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
#include <random>
#include <numbers>
#include <unordered_set>
#include <execution>
#include <map>
#include <ranges>
#include <set>

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
  } // namespace detail
  
  GenOCSTask::GenOCSTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void GenOCSTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Initialize objects for shader call
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
    });

    auto ocs_samples = detail::generate_unit_dirs<6>(128);
    info.insert_resource<std::vector<eig::Array<float, 6, 1>>>("ocs_samples", std::move(ocs_samples));
    info.insert_resource<gl::Buffer>("ocs_buffer", gl::Buffer());
    info.insert_resource<gl::Buffer>("ocs_verts", gl::Buffer());
    info.insert_resource<gl::Buffer>("ocs_elems", gl::Buffer());
    info.insert_resource<Colr>("ocs_centr", 0.f);

    m_stale = true;
  }

  void GenOCSTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Generate OCS texture only on relevant state change
    auto &e_gamut_idx = info.get_resource<int>("viewport", "gamut_selection");
    guard(e_gamut_idx >= 0);

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
    info.get_resource<gl::Buffer>("ocs_buffer") = {{ .data = cnt_span<const std::byte>(boundary_al) }};
  }
}