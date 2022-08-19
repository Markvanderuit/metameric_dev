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
#include <omp.h>
#include <ranges>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/convex_hull_3.h>
#include <CGAL/Polyhedron_3.h>
#include <CGAL/Surface_mesh.h>

namespace met {
  constexpr uint n_samples = 256;

  using Kernel      = CGAL::Exact_predicates_inexact_constructions_kernel;
  using Point       = Kernel::Point_3;
  using SurfaceMesh = CGAL::Surface_mesh<Point>;

  struct BasicAlMesh {
    std::vector<eig::AlArray3f> vertices;
    std::vector<eig::Array3u> elements; 
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

    return { verts, elems };
  }
  
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
      .size = static_cast<size_t>(n_samples) * sizeof(eig::AlArray3f),
      .flags = gl::BufferCreateFlags::eMapFull
    });
    info.emplace_resource<gl::Buffer>("spectrum_buffer", {
      .size = static_cast<size_t>(n_samples) * sizeof(Spec),
      .flags = gl::BufferCreateFlags::eMapFull
    });

    // Generate a buffer of random noise
    std::vector<float> rand_buffer(n_samples);
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
    });

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
    auto &i_rand_buffer = info.get_resource<gl::Buffer>("rand_buffer");
    auto &i_colr_buffer = info.get_resource<gl::Buffer>("color_buffer");
    auto &i_spec_buffer = info.get_resource<gl::Buffer>("spectrum_buffer");
    auto &e_mapp_buffer = info.get_resource<gl::Buffer>("gen_spectral_mappings", "mappings_buffer");

    /* // Open maps
    auto colr_map = i_colr_buffer.map_as<eig::AlArray3f>(gl::BufferAccessFlags::eMapReadWrite);
    auto spec_map = i_spec_buffer.map_as<Spec>(gl::BufferAccessFlags::eMapReadWrite);

    // Obtain color system spectra
    CMFS cs = e_mappings[0].finalize();

    // Generate samples
    for (uint i = 0; i < n_samples; ++i) {
      // Generate random unit vector as point on sphere
      auto sv = eig::Array2f::Random() * 0.5f + 0.5f;
      float t = 2.f * std::numbers::pi_v<float> * sv.x();
      float o = std::acosf(1.f - 2.f * sv.y());
      auto uv = eig::Vector3f(std::sinf(o) * std::cosf(t),
                              std::sinf(o) * std::sinf(t),
                              std::cosf(o));

      // Generate the algorithm's matrix A_ij and the optimal spectrum R_ij and a mapped color
      Spec a_ij = uv.transpose() * cs.matrix().transpose();
      Spec r_ij = (a_ij >= 0.f).select(Spec(1.f), Spec(0.f));
      auto c_ij = cs.matrix().transpose() * r_ij.matrix();

      // Write results to buffers
      spec_map[i] = r_ij;
      colr_map[i] = uv; //c_ij;
    }

    // Close maps and flush data
    i_colr_buffer.unmap();
    i_spec_buffer.unmap(); */

    // Bind buffer resources to ssbo targets
    i_rand_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 0);
    e_mapp_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 1);
    i_spec_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 2);
    i_colr_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 3);
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);

    // Dispatch shader to samples optimal spectra on OCS border
    gl::dispatch_compute(m_dispatch);
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);
    gl::sync::memory_barrier(gl::BarrierFlags::eBufferUpdate);
    
    // Allocate buffers and transfer data for output
    std::vector<eig::AlArray3f> colr_buffer(n_samples);
    std::vector<Spec>           spec_buffer(n_samples);
    i_colr_buffer.get_as<eig::AlArray3f>(colr_buffer);
    i_spec_buffer.get_as<Spec>(spec_buffer);

    // Compute convex hull over output colors and submit data to buffer resources
    auto mesh = cgal_surface_to_mesh(cgal_convex_hull(colr_buffer));
    fmt::print("Generated convex hull ({} verts, {} elems)\n", mesh.vertices.size(), mesh.elements.size());
    info.emplace_resource<gl::Buffer>("ocs_verts", { .data = as_span<const std::byte>(mesh.vertices) });
    info.emplace_resource<gl::Buffer>("ocs_elems", { .data = as_span<const std::byte>(mesh.elements) });

    /* 
    std::vector<eig::AlArray3f> colr_buffer(n_samples);
    std::vector<Spec>           spec_buffer(n_samples);
    i_colr_buffer.get_as<eig::AlArray3f>(colr_buffer);
    i_spec_buffer.get_as<Spec>(spec_buffer);
    for (uint i = 0; i < 256; ++i) {
      fmt::print("{} -> {}\n", spec_buffer[i], colr_buffer[i]);
    }

    // Define equality and hash functions for eigen types to fit in std::unordered_set
    constexpr auto eig_eq = [](auto const& a, auto const &b) { 
      bool is_eq = true;
      for (uint i = 0; i < a.size(); ++i)
        is_eq &= (a[i] == b[i]);
      return is_eq;
    };
    constexpr auto eig_h = [](auto const& v) {
      size_t seed = v.size();
      for (auto &f : v)
        seed ^= std::hash<float>()(f) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      return seed;
    };

    std::unordered_set<eig::AlArray3f, decltype(eig_h), decltype(eig_eq)> colr_unique(
      range_iter(colr_buffer), 64, eig_h, eig_eq);
    fmt::print("{}\n", colr_unique.size()); */

    /* for (auto &v : colr_buffer) {
      if (colr_unique.contains(v)) {
        fmt::print("Not unique {}\n", v);
      } else {
        colr_unique.insert(v);
      }
    } */
  }
}