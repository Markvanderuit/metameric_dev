#include <metameric/components/tasks/task_gen_ocs.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/utility.hpp>
#include <algorithm>
#include <random>
#include <numbers>
#include <unordered_set>
#include <execution>
#include <map>
#include <ranges>
#include <set>
#include <omp.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/convex_hull_3.h>
#include <CGAL/Polyhedron_3.h>
#include <CGAL/Surface_mesh.h>
// #include <libqhullcpp/Qhull.h>

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
    m_program = {{ .type = gl::ShaderType::eCompute,
                   .path = "resources/shaders/gen_ocs/gen_ocs.comp" }};
    m_dispatch = { .groups_x = ceil_div(n_samples, 256u), 
                   .bindable_program = &m_program };

    // Set these uniforms once
    m_program.uniform("u_n", n_samples);
    m_program.uniform("u_mapping_i", 0u);

    // Initialize main buffers
    info.emplace_resource<gl::Buffer>("color_buffer", {
      .size  = static_cast<size_t>(n_samples) * sizeof(eig::AlArray3f)
    });
    info.emplace_resource<gl::Buffer>("spectrum_buffer", {
      .size = static_cast<size_t>(n_samples) * sizeof(Spec)
    });

    // Generate a buffer of random noise
    /* std::vector<float> rand_buffer(n_samples);
    #pragma omp parallel
    {
      // Set up random distribution across threads
      std::random_device device;
      std::mt19937 eng(device());
      eng.seed(2 * omp_get_thread_num() + 3);
      std::uniform_real_distribution<float> dist(0.f, 1.f);

      // fill buffer
      #pragma omp for
      for (int i = 0; i < static_cast<int>(rand_buffer.size()); ++i) {
        rand_buffer[i] = dist(eng);
      }
    }

    // Pass buffer to gpu
    info.emplace_resource<gl::Buffer>("rand_buffer", {
      .data = as_span<const std::byte>(rand_buffer)
    }); */

    m_stale = true;
  }

  void GenOCSTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Execute once, for now
    guard(m_stale); 
    m_stale = false;

    // Get shared resources
    auto &e_app_data    = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_mappings    = e_app_data.loaded_mappings;
    auto &i_spec_buffer = info.get_resource<gl::Buffer>("spectrum_buffer");
    auto &e_mapp_buffer = info.get_resource<gl::Buffer>("gen_spectral_mappings", "mappings_buffer");
    auto &i_colr_buffer = info.get_resource<gl::Buffer>("color_buffer");

    // Bind buffer resources to ssbo targets
    e_mapp_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 0);
    i_spec_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 1);
    i_colr_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 2);
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);

    // Dispatch shader to samples optimal spectra on OCS border
    gl::dispatch_compute(m_dispatch);
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);
    gl::sync::memory_barrier(gl::BarrierFlags::eBufferUpdate);
    
    // Allocate buffers and transfer data back to client for convex hull generation
    // auto colr_buffer = i_colr_buffer.map_as<const eig::AlArray3f>(gl::BufferAccessFlags::eMapRead);
    std::vector<eig::AlArray3f> colr_buffer(n_samples);
    i_colr_buffer.get_as<eig::AlArray3f>(colr_buffer);

    auto avg = std::reduce(std::execution::par_unseq, range_iter(colr_buffer), eig::AlArray3f(0))
           / static_cast<float>(colr_buffer.size());

    constexpr auto eig_hash = [](const auto &v) {
      size_t seed = 0;
      for (size_t i = 0; i < v.size(); ++i) {
        auto elem = *(v.data() + i);
        seed ^= std::hash<float>()(elem) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      }
      return seed;
    };
    constexpr auto eig_compare = [](const auto &a, const auto &b) {
      return (a < b).any();
    };
    std::set<eig::AlArray3f, decltype(eig_compare)> unique_set(range_iter(colr_buffer), eig_compare);

    // Output information about generated positions
   /*  for (auto &v : colr_buffer) {
      fmt::print("{}\n", v);
    } */
    fmt::print("Average: {}\n", avg);    
    fmt::print("Unique: {}\n", unique_set.size());    

    // Generate convex hull over output vectors
    auto mesh = detail::cgal_surface_to_mesh(detail::cgal_convex_hull(colr_buffer));
    fmt::print("Generated convex hull ({} verts, {} elems)\n", mesh.vertices.size(), mesh.elements.size());

    // Submit data to buffer resources
    info.emplace_resource<gl::Buffer>("ocs_verts", { .data = as_span<const std::byte>(mesh.vertices) });
    info.emplace_resource<gl::Buffer>("ocs_elems", { .data = as_span<const std::byte>(mesh.elements) });
  }
}