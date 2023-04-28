#include <metameric/core/data.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/components/pipeline/task_gen_delaunay_weights.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/utility.hpp>
#include <algorithm>
#include <execution>
#include <ranges>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;
  constexpr uint buffer_init_size    = 512u;
  constexpr uint bvh_level_begin = 3;

  // Generate a transforming view that performs unsigned integer index access over a range
  constexpr auto indexed_view(const auto &v) {
    return std::views::transform([&v](uint i) { return v[i]; });
  };

  void GenDelaunayWeightsTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources
    const auto &e_appl_data = info.global("appl_data").read_only<ApplicationData>();
    const auto &e_colr_data = e_appl_data.loaded_texture;
    const auto &e_proj_data = e_appl_data.project_data;
    
    // Determine compute dispatch size
    const uint dispatch_n    = e_colr_data.size().prod();
    const uint dispatch_ndiv = ceil_div(dispatch_n, 256u);

    // Initialize objects for compute dispatch
    m_program = {{ .type       = gl::ShaderType::eCompute,
                   .spirv_path = "resources/shaders/pipeline/gen_delaunay_weights.comp.spv",
                   .cross_path = "resources/shaders/pipeline/gen_delaunay_weights.comp.json" }};
    m_dispatch = { .groups_x = dispatch_ndiv, 
                   .bindable_program = &m_program }; 

    // Initialize uniform buffer and writeable, flushable mapping
    m_uniform_buffer = {{ .size = sizeof(UniformBuffer), .flags = buffer_create_flags }};
    m_uniform_map    = &m_uniform_buffer.map_as<UniformBuffer>(buffer_access_flags)[0];
    m_uniform_map->n = e_appl_data.loaded_texture.size().prod();

    // Initialize buffer data
    gl::Buffer colr_buffer = {{ .data = cast_span<const std::byte>(io::as_aligned((e_colr_data)).data()) }};
    gl::Buffer vert_buffer = {{ .size = buffer_init_size * sizeof(eig::Array4f), .flags = buffer_create_flags}};
    gl::Buffer elem_buffer = {{ .size = buffer_init_size * sizeof(eig::Array4u), .flags = buffer_create_flags}};
    m_pack_buffer          = {{ .size = buffer_init_size * sizeof(ElemPack), .flags = buffer_create_flags }};

    // Initialize writeable, flushable mappings over relevant buffers
    m_vert_map = vert_buffer.map_as<eig::AlArray3f>(buffer_access_flags);
    m_elem_map = elem_buffer.map_as<eig::Array4u>(buffer_access_flags);
    m_pack_map = m_pack_buffer.map_as<ElemPack>(buffer_access_flags);

    // Initialize tesselation structure, and search tree over tesselation structure
    info("delaunay").set<AlignedDelaunayData>({ });
    const auto &elem_tree = info("elem_tree").set<BVH>(BVH(buffer_init_size)).read_only<BVH>();
    
    // Initialize search tree over color data
    std::vector<Colr> colr_data(range_iter(e_colr_data.data()));
    const auto &colr_tree = info("colr_tree").set(BVHColr(cnt_span<Colr>(colr_data))).read_only<BVHColr>();

    // Initialize buffer holding barycentric weights
    info("colr_buffer").set(std::move(colr_buffer)); // OpenGL buffer storing texture color positions
    info("vert_buffer").set(std::move(vert_buffer)); // OpenGL buffer storing delaunay vertex positions
    info("elem_buffer").set(std::move(elem_buffer)); // OpenGL buffer storing delaunay tetrahedral elements
    info("bary_buffer").init<gl::Buffer>({ .size = dispatch_n * sizeof(eig::Array4f) }); // Convex weights
    info("tree_buffer").init<gl::Buffer>({ .size = elem_tree.size_bytes_reserved(), .flags = gl::BufferCreateFlags::eStorageDynamic });

    /* // Initialize tree and work components
    constexpr size_t work_size = sizeof(eig::Array4u) + 256 * 1024 * 1024 * sizeof(eig::Array2u);
    m_bvh_comp_buffer = {{ .size = e_colr_data.data().size() * sizeof(uint), .flags = gl::BufferCreateFlags::eStorageDynamic }};
    m_bvh_colr_buffer = {{ .data = cast_span<const std::byte>(colr_tree.data()) }};
    m_bvh_elem_buffer = {{ .size = elem_tree.size_bytes_reserved(), .flags = gl::BufferCreateFlags::eStorageDynamic }};
    m_bvh_curr_work   = {{ .size = work_size, .flags = gl::BufferCreateFlags::eStorageDynamic }};
    m_bvh_next_work   = {{ .size = work_size, .flags = gl::BufferCreateFlags::eStorageDynamic }};
    m_bvh_leaf_work   = {{ .size = work_size, .flags = gl::BufferCreateFlags::eStorageDynamic }};
    m_bvh_unif_buffer = {{ .size = sizeof(BVHUniformBuffer), .flags = gl::BufferCreateFlags::eMapWritePersistent }};
    m_bvh_unif_map    = m_bvh_unif_buffer.map_as<BVHUniformBuffer>(gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush).data();

    m_bvh_div_32_program = {{ .type       = gl::ShaderType::eCompute,
                              .spirv_path = "resources/shaders/misc/dispatch_divide_32.comp.spv",
                              .cross_path = "resources/shaders/misc/dispatch_divide_32.comp.json" }};
    m_bvh_div_08_program = {{ .type       = gl::ShaderType::eCompute,
                             .spirv_path = "resources/shaders/misc/dispatch_divide_8.comp.spv",
                             .cross_path = "resources/shaders/misc/dispatch_divide_8.comp.json" }};
    m_bvh_div_32_buffer = {{ .size = sizeof(eig::Array4u) }};
    m_bvh_div_08_buffer = {{ .size = sizeof(eig::Array4u) }};
    m_bvh_div_32_dispatch = { .bindable_program = &m_bvh_div_32_program };
    m_bvh_div_08_dispatch = { .bindable_program = &m_bvh_div_08_program };

    m_bvh_desc_program = {{ .type       = gl::ShaderType::eCompute,
                            .spirv_path = "resources/shaders/pipeline/gen_delaunay_weights_traverse.comp.spv",
                            .cross_path = "resources/shaders/pipeline/gen_delaunay_weights_traverse.comp.json" }};
    m_bvh_bary_program = {{ .type       = gl::ShaderType::eCompute,
                            .spirv_path = "resources/shaders/pipeline/gen_delaunay_weights_accumulate.comp.spv",
                            .cross_path = "resources/shaders/pipeline/gen_delaunay_weights_accumulate.comp.json" }};
    m_bvh_desc_dispatch = { .buffer = &m_bvh_div_32_buffer, .bindable_program = &m_bvh_desc_program };
    m_bvh_bary_dispatch = { .buffer = &m_bvh_div_08_buffer, .bindable_program = &m_bvh_bary_program };

    // TODO; filter out empty texture nodes from the initial workload
    auto init_work_data = detail::init_pair_data<8, 8>(3, bvh_level_begin);
    auto init_head = eig::Array4u(0);
    std::vector<eig::Array2u> init_work = { eig::Array2u { init_work_data.size(), 0 }, 0 };
    std::ranges::copy(init_work_data, std::back_inserter(init_work));
    m_bvh_init_work = {{ .data = cnt_span<const std::byte>(init_work) }};
    m_bvh_init_head = {{ .data = obj_span<const std::byte>(init_head) }}; */
  }
  
  bool GenDelaunayWeightsTask::is_active(SchedulerHandle &info) {
    met_trace();
    // return info("state", "proj_state").read_only<ProjectState>().verts;
    return true;
  }

  void GenDelaunayWeightsTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_proj_state = info("state", "proj_state").read_only<ProjectState>();
    const auto &e_appl_data  = info.global("appl_data").read_only<ApplicationData>();
    const auto &e_proj_data  = e_appl_data.project_data;

    // Get modified resources
    auto &i_delaunay    = info("delaunay").writeable<AlignedDelaunayData>();
    auto &i_elem_tree   = info("elem_tree").writeable<BVH>();
    auto &i_colr_tree   = info("colr_tree").read_only<BVHColr>();
    auto &i_vert_buffer = info("vert_buffer").writeable<gl::Buffer>();
    auto &i_elem_buffer = info("elem_buffer").writeable<gl::Buffer>();
    auto &i_tree_buffer = info("tree_buffer").writeable<gl::Buffer>();

    // Generate new delaunay structure and search tree
    std::vector<Colr> delaunay_input(e_proj_data.verts.size());
    std::ranges::transform(e_proj_data.verts, delaunay_input.begin(), [](const auto &vt) { return vt.colr_i; });
    i_delaunay = generate_delaunay<AlignedDelaunayData, Colr>(delaunay_input);
    i_elem_tree.build(i_delaunay.verts, i_delaunay.elems);

    // Recover triangle element data and store in project
    auto [_, elems] = convert_mesh<AlignedMeshData>(i_delaunay);
    info.global("appl_data").writeable<ApplicationData>().project_data.elems = elems;

    // Push stale vertices
    auto vert_range = std::views::iota(0u, static_cast<uint>(e_proj_state.verts.size()))
                    | std::views::filter([&](uint i) -> bool { return e_proj_state.verts[i]; });
    for (uint i : vert_range) {
      m_vert_map[i] = e_proj_data.verts[i].colr_i;
      i_vert_buffer.flush(sizeof(eig::AlArray3f), i * sizeof(eig::AlArray3f));
    }

    // Push stale packed tetrahedral data
    std::ranges::transform(i_delaunay.elems, m_pack_map.begin(), [&](const auto &el) {
      const auto verts = el | indexed_view(i_delaunay.verts);
      ElemPack pack;
      pack.inv.block<3, 3>(0, 0) = (eig::Matrix3f() 
        << verts[0] - verts[3],
           verts[1] - verts[3],
           verts[2] - verts[3]).finished().inverse();
      pack.sub.head<3>() = verts[3];
      return pack;
    });
    m_pack_buffer.flush(i_delaunay.elems.size() * sizeof(ElemPack));

    // Push stale tetrahedral element data // TODO optimize?
    std::ranges::copy(i_delaunay.elems, m_elem_map.begin());
    i_elem_buffer.flush(i_delaunay.elems.size() * sizeof(eig::Array4u));
    
    // Push stale mesh tree data // TODO optimize?
    auto tree_data = cast_span<const std::byte>(i_elem_tree.data());
    i_tree_buffer.set(tree_data, tree_data.size()); // Specify size as buffer over-reserves data size
    /* m_bvh_elem_buffer.set(tree_data, tree_data.size()); */

    // Push uniform data
    m_uniform_map->n_verts = i_delaunay.verts.size();
    m_uniform_map->n_elems = i_delaunay.elems.size();
    m_uniform_buffer.flush();

    // Bind required buffers to corresponding targets
    m_program.bind("b_unif", m_uniform_buffer);
    m_program.bind("b_pack", m_pack_buffer);
    m_program.bind("b_posi", info("colr_buffer").read_only<gl::Buffer>());
    m_program.bind("b_bary", info("bary_buffer").writeable<gl::Buffer>());
    
    // Dispatch shader to generate delaunay convex weights
    gl::dispatch_compute(m_dispatch);

    { met_trace_full_n("bvh_testing");
     /*  // Reset some buffers
      m_bvh_init_work.copy_to(m_bvh_curr_work, m_bvh_init_work.size());
      // m_bvh_init_head.copy_to(m_bvh_leaf_work, sizeof(uint));

      // Push uniform data
      m_bvh_unif_map->n_elems = i_delaunay.elems.size();
      m_bvh_unif_buffer.flush(); */

      // Iterate through levels of hierarchy, finding an optimal dual-hierarchy cut for computation
      /* for (uint i = bvh_level_begin; i < i_colr_tree.n_levels() - 2; ++i) {
        // Reset next work head to starting work head
        m_bvh_init_head.copy_to(m_bvh_next_work, sizeof(uint));

        // Copy divided data to indirect dispatch buffer (divide by (256/8))
        m_bvh_div_32_program.bind("b_data", m_bvh_curr_work);
        m_bvh_div_32_program.bind("b_disp", m_bvh_div_32_buffer);
        gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);
        gl::dispatch_compute(m_bvh_div_32_dispatch);

        // Bind relevant buffers
        m_bvh_desc_program.bind("b_unif", m_bvh_unif_buffer);
        m_bvh_desc_program.bind("b_elem", m_bvh_elem_buffer);
        m_bvh_desc_program.bind("b_colr", m_bvh_colr_buffer);
        m_bvh_desc_program.bind("b_curr", m_bvh_curr_work);
        m_bvh_desc_program.bind("b_next", m_bvh_next_work);
        m_bvh_desc_program.bind("b_leaf", m_bvh_leaf_work);

        // Dispatch work using indirect buffer, based on previous work data
        gl::sync::memory_barrier(gl::BarrierFlags::eBufferUpdate | gl::BarrierFlags::eShaderStorageBuffer);
        gl::dispatch_compute(m_bvh_desc_dispatch);

        // Swap current/next work buffers
        std::swap(m_bvh_curr_work, m_bvh_next_work);
      } // for i */

      /* // Clear comparative helper buffer to some reasonably large integer value
      int comp_max = 1024;
      m_bvh_comp_buffer.clear(cast_span<const std::byte>(std::span<int> { &comp_max, 1 })); */

      // Process bottom part of cut
      /* { met_trace_full_n("bottom_cut");
        // Copy divided data to indirect dispatch buffer (divide by (256/32))
        m_bvh_div_08_program.bind("b_data", m_bvh_curr_work);
        m_bvh_div_08_program.bind("b_disp", m_bvh_div_08_buffer);
        gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);
        gl::dispatch_compute(m_bvh_div_08_dispatch);

        // Bind relevant buffers
        m_bvh_bary_program.bind("b_unif", m_bvh_unif_buffer);
        m_bvh_bary_program.bind("b_elem", m_bvh_elem_buffer);
        m_bvh_bary_program.bind("b_colr", m_bvh_colr_buffer);
        m_bvh_bary_program.bind("b_comp", m_bvh_comp_buffer);
        m_bvh_bary_program.bind("b_work", m_bvh_curr_work);
        m_bvh_bary_program.bind("b_pack", m_pack_buffer);
        m_bvh_bary_program.bind("b_posi", info("colr_buffer").read_only<gl::Buffer>());
        m_bvh_bary_program.bind("b_bary", info("bary_buffer").writeable<gl::Buffer>());

        // Dispatch work using indirect buffer, based on previous work data
        gl::sync::memory_barrier(gl::BarrierFlags::eBufferUpdate | gl::BarrierFlags::eShaderStorageBuffer);
        gl::dispatch_compute(m_bvh_bary_dispatch);
      } */

      /* // Process leaf part of cut
      { met_trace_full_n("leaf_cut");
        // Copy divided data to indirect dispatch buffer (divide by (256/32))
        m_bvh_div_08_program.bind("b_data", m_bvh_leaf_work);
        m_bvh_div_08_program.bind("b_disp", m_bvh_div_08_buffer);
        gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);
        gl::dispatch_compute(m_bvh_div_08_dispatch);

        // Bind relevant buffers
        m_bvh_bary_program.bind("b_unif", m_bvh_unif_buffer);
        m_bvh_bary_program.bind("b_elem", m_bvh_elem_buffer);
        m_bvh_bary_program.bind("b_colr", m_bvh_colr_buffer);
        m_bvh_bary_program.bind("b_comp", m_bvh_comp_buffer);
        m_bvh_bary_program.bind("b_work", m_bvh_leaf_work);
        m_bvh_bary_program.bind("b_pack", m_pack_buffer);
        m_bvh_bary_program.bind("b_posi", info("colr_buffer").read_only<gl::Buffer>());
        m_bvh_bary_program.bind("b_bary", info("bary_buffer").writeable<gl::Buffer>());

        // Dispatch work using indirect buffer, based on previous work data
        gl::sync::memory_barrier(gl::BarrierFlags::eBufferUpdate | gl::BarrierFlags::eShaderStorageBuffer);
        gl::dispatch_compute(m_bvh_bary_dispatch);
      } */

      // { // Do some more debugging
      //   using BVHNode = detail::BVHNode;
      //   struct WorkUnit { uint elem_i, colr_i; };

      //   // Get tree nodes 
      //   auto colr_nodes = i_colr_tree.data();
      //   auto elem_nodes = i_elem_tree.data();

      //   // Get work queue and leaf queue
      //   uint leaf_head, curr_head;
      //   m_bvh_leaf_work.get_as<uint>(std::span<uint> { &leaf_head, 1}, 1, 0);
      //   // m_bvh_curr_work.get_as<uint>(std::span<uint> { &curr_head, 1}, 1, 0);
        
      //   std::vector<WorkUnit> leaf_work(leaf_head);
      //   // std::vector<WorkUnit> curr_work(curr_head);
      //   m_bvh_leaf_work.get_as<WorkUnit>(leaf_work, leaf_work.size(), 2);
      //   // m_bvh_curr_work.get_as<WorkUnit>(curr_work, curr_work.size(), 2);

      //   fmt::print("leaf\n\t{}\n", cast_span<eig::Array2u>(std::span { leaf_work.begin(), 64 }));
      //   // fmt::print("curr\n\t{}\n", cast_span<eig::Array2u>(std::span { curr_work.begin(), 64 }));

      //   std::vector<uint> point_count(e_appl_data.loaded_texture.data().size(), 0);
      //   /* for (const auto &work : curr_work) {
      //     const auto &colr = colr_nodes[work.colr_i];
      //     const auto &elem = elem_nodes[work.colr_i];
      //     for (uint i = colr.i; i < colr.i + colr.n; ++i)
      //       point_count[i]++;
      //     // fmt::print("{} - {}\n", colr.i, colr.n);
      //   } */

      //   for (const auto &work : leaf_work) {
      //     const auto &colr = colr_nodes[work.colr_i];
      //     const auto &elem = elem_nodes[work.elem_i];
      //     for (uint i = colr.i; i < colr.i + colr.n; ++i)
      //       point_count[i] += elem.n;
      //     // fmt::print("{} - {}\n", colr.i, colr.n);
      //   }

      //   // fmt::print("count\n\t{}\n", std::span { point_count.begin(), 64 });
      //   fmt::print("elems = {}\n", i_delaunay.elems.size());
      //   for (uint i = 0; i < 128; ++i)
      //     fmt::print("{} - {}\n", i, point_count[i]);


      //   /* int colr_count = 0, elem_count = 0;
      //   for (const auto &node : colr_nodes) {
      //     colr_count += node.n;
      //   }
      //   for (const auto &node : elem_nodes) {
      //     elem_count += node.n;
      //   }

      //   fmt::print("colr_count {} of {}\n", colr_count, e_appl_data.loaded_texture.data().size());
      //   fmt::print("elem_count {} of {}\n", elem_count, i_delaunay.elems.size()); */

      //   std::exit(0);
      // } // End debugging

      /* { // Do some debugging
        struct BVHNode  { uint indx, i, n; };
        struct WorkNode { uint elem_i, colr_i; };
        
        uint curr_work_head;
        m_bvh_curr_work.get_as<uint>(std::span<uint> { & curr_work_head, 1 }, 1, 0);
        fmt::print("queried work head: {}\n", curr_work_head);
        std::vector<WorkNode> curr_work(curr_work_head);
        m_bvh_curr_work.get_as<WorkNode>(curr_work, curr_work_head, 2);

        auto colr_span = i_colr_tree.data();
        auto elem_span = i_elem_tree.data();

        std::vector<BVHNode> colr_nodes(curr_work.size()), elem_nodes(curr_work.size());
        std::transform(std::execution::par_unseq, range_iter(curr_work), colr_nodes.begin(), [&](auto &work) {
          if (work.colr_i >= colr_span.size()) {
            fmt::print("Caught work {} of {}\n", eig::Array2u { work.elem_i, work.colr_i }, colr_span.size());
            std::exit(0);
            // debug::check_expr(work.colr_i < colr_span.size());
          }
          const auto &node = colr_span[work.colr_i];
          return BVHNode { work.colr_i, node.i, node.n };
        });
        std::transform(std::execution::par_unseq, range_iter(curr_work), elem_nodes.begin(), [&](auto &work) {
          if (work.elem_i >= elem_span.size()) {
            fmt::print("Caught elem {} of {}\n", eig::Array2u { work.elem_i, work.colr_i }, elem_span.size());
            std::exit(0);
            // debug::check_expr(work.elem_i < elem_span.size());
          }
          const auto &node = elem_span[work.elem_i];
          return BVHNode { work.elem_i, node.i, node.n };
        });

        fmt::print("Colr:\n\t{}\n",
          cast_span<const eig::Array3u>(std::span { colr_nodes.begin() + 0, 16 }));

        fmt::print("Elem:\n\t{}\n",
          cast_span<const eig::Array3u>(std::span { elem_nodes.begin() + 0, 16 }));
          
        std::exit(0);
      } // Do some debugging */

      // uint curr_head = 0, disp_head = 0;
      // m_bvh_curr_work.get_as<uint>(std::span<uint> { &curr_head, 1 }, 1);
      // m_bvh_div_08_buffer.get_as<uint>(std::span<uint> { &disp_head, 1 }, 1);
      // fmt::print("Dispatching {} / (256 / 32) = {}\n", curr_head, disp_head);

      // std::vector<int> comp_data(32);
      // // /* m_bvh_comp_buffer.get_as<int>(comp_data, comp_data.size(), 2048 * 1024 * sizeof(uint));
      // // fmt::print("comp pre : {}\n", comp_data); */
      // m_bvh_comp_buffer.get_as<int>(comp_data, comp_data.size(), 1024 * 2048);
      // fmt::print("comp post: {}\n", comp_data);
      // std::exit(0);
    } // bvh_testing

    /* { met_trace_n("work_testing");
      // Intersect function
      constexpr auto intersect = [](auto &node_a, auto &node_b) {
        return (node_a.maxb >= node_b.minb).all() && (node_a.minb < node_b.maxb).all();
      };

      // Get shared resources
      const auto &i_colr_tree = info("colr_tree").read_only<BVHColr>();

      // Get good set of bbox levels from either hierarchy
      auto elem_nodes = i_elem_tree.data(i_elem_tree.n_levels() - 1);
      auto colr_nodes = i_colr_tree.data(std::min(6u, i_colr_tree.n_levels() - 1));

      uint n_comp_size = 0;
      uint n_cuts_size = 0;
      for (uint a = 0; a < elem_nodes.size(); ++a) {
        for (uint b = 0; b < colr_nodes.size(); ++b) {
          if (intersect(elem_nodes[a], colr_nodes[b])) {
            n_comp_size += elem_nodes[a].n * colr_nodes[b].n;
            n_cuts_size++;
          }
        }
      }
      fmt::print("Interactions : {}, nodes in cut : {}\n", n_comp_size, n_cuts_size);

      const auto &colr_max_node = *std::ranges::max_element(colr_nodes, {}, &detail::BVHNode::n);
      const auto &elem_max_node = *std::ranges::max_element(elem_nodes, {}, &detail::BVHNode::n);

      fmt::print("elem x colr : {} x {} = {}\n", 
        elem_nodes.size(), colr_nodes.size(), elem_nodes.size() * colr_nodes.size());
      fmt::print("Maximum; elem = {} ({} x {}), colr = {} ({} x {})\n",
        elem_max_node.n, elem_max_node.minb, elem_max_node.maxb,
        colr_max_node.n, colr_max_node.minb, colr_max_node.maxb);
    } // work_testing */

    /* auto top    = m_tree_points.data(0).front();
    auto bottom = m_tree_points.data(3)
                | std::views::transform([](const auto &node) { return node.n; });
    uint bcount = std::reduce(range_iter(bottom), 0);
    fmt::print("{} vs {}\n", top.n, bcount); */
  } 
} // namespace met