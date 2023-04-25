#include <metameric/core/data.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/components/pipeline/task_gen_delaunay_weights.hpp>
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

  /* constexpr auto initPairs = []() {
    const uint fBegin = (0x24924924u >> (32u - logk * DH_HIER_INIT_LVL_3D));
    const uint fNodes = 1u << (logk * DH_HIER_INIT_LVL_3D);
    const uint eBegin = (0x24924924u >> (32u - logk * DH_HIER_INIT_LVL_3D));
    const uint eNodes = 1u << (logk * DH_HIER_INIT_LVL_3D);
    std::array<glm::uvec2, fNodes * eNodes> a {};
    for (uint i = 0; i < fNodes; ++i) {
      for (uint j = 0; j < eNodes; ++j) {
        a[i * eNodes + j] = glm::uvec2(fBegin + i, eBegin + j);
      }
    }
    return a;
  }(); */

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

    // Initialize tree and work components
    constexpr size_t work_size = sizeof(eig::Array4u) + 64 * 1024 * 1024 * sizeof(eig::Array2u);
    m_bvh_colr_buffer = {{ .data = cast_span<const std::byte>(colr_tree.data()) }};
    m_bvh_elem_buffer = {{ .size = elem_tree.size_bytes_reserved(), .flags = gl::BufferCreateFlags::eStorageDynamic }};
    m_bvh_curr_work = {{ .size = work_size/* , .flags = gl::BufferCreateFlags::eStorageDynamic */ }};
    m_bvh_next_work = {{ .size = work_size/* , .flags = gl::BufferCreateFlags::eStorageDynamic */ }};
    m_bvh_unif_buffer = {{ .size = sizeof(BVHUniformBuffer), .flags = gl::BufferCreateFlags::eMapWritePersistent }};
    m_bvh_unif_map    = m_bvh_unif_buffer.map_as<BVHUniformBuffer>(gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush).data();
    m_bvh_comp_buffer = {{ .size = e_colr_data.data().size() * sizeof(uint), .flags = gl::BufferCreateFlags::eStorageDynamic }};

    m_bvh_desc_program = {{ .type       = gl::ShaderType::eCompute,
                       .spirv_path = "resources/shaders/pipeline/gen_delaunay_weights_traverse.comp.spv",
                       .cross_path = "resources/shaders/pipeline/gen_delaunay_weights_traverse.comp.json" }};
    m_bvh_desc_dispatch = { .buffer = &m_bvh_div_32_buffer, .bindable_program = &m_bvh_desc_program };

    m_bvh_div_sg_program = {{ .type       = gl::ShaderType::eCompute,
                              .spirv_path = "resources/shaders/misc/dispatch_divide_8.comp.spv",
                              .cross_path = "resources/shaders/misc/dispatch_divide_8.comp.json" }};
    m_bvh_div_sg_buffer   = {{ .size = sizeof(eig::Array4u) }};
    m_bvh_div_sg_dispatch = { .bindable_program = &m_bvh_div_sg_program };

    m_bvh_div_32_program = {{ .type       = gl::ShaderType::eCompute,
                              .spirv_path = "resources/shaders/misc/dispatch_divide_32.comp.spv",
                              .cross_path = "resources/shaders/misc/dispatch_divide_32.comp.json" }};
    m_bvh_div_32_buffer   = {{ .size = sizeof(eig::Array4u) }};
    m_bvh_div_32_dispatch = { .bindable_program = &m_bvh_div_32_program };
    
    m_bvh_bary_program = {{ .type       = gl::ShaderType::eCompute,
                            .spirv_path = "resources/shaders/pipeline/gen_delaunay_weights_accumulate.comp.spv",
                            .cross_path = "resources/shaders/pipeline/gen_delaunay_weights_accumulate.comp.json" }};
    m_bvh_bary_dispatch = { .buffer = &m_bvh_div_sg_buffer, .bindable_program = &m_bvh_bary_program };

    auto init_work_data = detail::init_pair_data<8, 8>(3, bvh_level_begin);
    auto init_head = eig::Array4u(0);
    std::vector<eig::Array2u> init_work = { eig::Array2u { init_work_data.size(), 0 }, 0 };
    std::ranges::copy(init_work_data, std::back_inserter(init_work));
    m_bvh_init_work = {{ .data = cnt_span<const std::byte>(init_work) }};
    m_bvh_init_head = {{ .data = obj_span<const std::byte>(init_head) }};

    fmt::print("init {}\n", init_work.front().x());
    fmt::print("init size = {}, work size = {}\n", m_bvh_init_work.size(), m_bvh_curr_work.size());

    // TODO; filter out empty texture nodes from the initial workload
  }
  
  bool GenDelaunayWeightsTask::is_active(SchedulerHandle &info) {
    met_trace();
    return info("state", "proj_state").read_only<ProjectState>().verts;
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
    m_bvh_elem_buffer.set(tree_data, tree_data.size());

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
    /* gl::dispatch_compute(m_dispatch); */

    { met_trace_n("bvh_testing");
      // Copy starting work to buffer
      fmt::print("Copy coming up: {} bytes\n", m_bvh_init_work.size());
      m_bvh_init_work.copy_to(m_bvh_curr_work, m_bvh_init_work.size());

      // Iterate through levels of hierarchy, finding an optimal dual-hierarchy cut for computation
      for (uint i = bvh_level_begin; i < i_colr_tree.n_levels() - 3; ++i) {
        fmt::print("level {}, max {} nodes\n", i, i_colr_tree.data(i).size());
        
        // Reset next work head
        m_bvh_init_head.copy_to(m_bvh_next_work, sizeof(uint));

        // Copy divided data to indirect dispatch buffer (divide by (256/8))
        m_bvh_div_32_program.bind("b_data", m_bvh_curr_work);
        m_bvh_div_32_program.bind("b_disp", m_bvh_div_32_buffer);
        gl::dispatch_compute(m_bvh_div_32_dispatch);
        
        // Push uniform data
        m_bvh_unif_map->n_colr_nodes = i_colr_tree.data(i).size();
        m_bvh_unif_map->n_elem_nodes = i_elem_tree.data(i).size();
        m_bvh_unif_buffer.flush();

        // Bind relevant buffers
        m_bvh_desc_program.bind("b_unif", m_bvh_unif_buffer);
        m_bvh_desc_program.bind("b_elem", m_bvh_elem_buffer);
        m_bvh_desc_program.bind("b_colr", m_bvh_colr_buffer);
        m_bvh_desc_program.bind("b_curr", m_bvh_curr_work);
        m_bvh_desc_program.bind("b_next", m_bvh_next_work);

        // Dispatch work using indirect buffer, based on previous work data
        gl::sync::memory_barrier(gl::BarrierFlags::eBufferUpdate       | 
                                 gl::BarrierFlags::eShaderStorageBuffer);
        gl::dispatch_compute(m_bvh_desc_dispatch);

        /* uint prev_head = 0, next_head = 0;
        m_bvh_curr_work.get_as<uint>(std::span<uint> { &prev_head, 1 }, 1);
        m_bvh_next_work.get_as<uint>(std::span<uint> { &next_head, 1 }, 1);
        fmt::print("{}: previous={} -> exp={}, next={}\n", i, prev_head, 8 * prev_head, next_head); */

        // Swap current/next work buffers
        std::swap(m_bvh_curr_work, m_bvh_next_work);
      } // for i

      /* { // Forcibly subdivide large nodes into smaller work

      } */

      /* { // Do some debugging
        struct WorkNode { uint elem_i, node_i; };
        std::vector<WorkNode> curr_work(m_bvh_curr_work.size() / sizeof(WorkNode) - 2 * sizeof(WorkNode));
        m_bvh_curr_work.get_as<WorkNode>(curr_work, curr_work.size(), 2);

        std::for_each(std::execution::par_unseq, range_iter(curr_work), [&](auto &work) {
          work.elem_i = i_elem_tree.data()[work.elem_i].n;
          work.node_i = i_colr_tree.data()[work.node_i].n;
        });

        auto large_work_view = curr_work | std::views::filter([](const WorkNode &w) { return w.node_i > 1024; });
        std::vector<WorkNode> large_work(range_iter(large_work_view));

        size_t large_work_n = large_work.size();
        size_t small_work_n = curr_work.size() - large_work_n;

        fmt::print("Large: {}, small: {}, sum: {}\n",
          large_work_n, small_work_n, large_work_n + small_work_n);


        // fmt::print("Nodes (elem, colr) : {}, {}\n", i_elem_tree.data().size(), i_colr_tree.data().size());
        fmt::print("Large work: {}\n", cast_span<const eig::Array2u>(std::span { large_work.begin(), 256 }));

        std::exit(0);
      } // Do some debugging */

      // Clear comparative buffer to some large integer value
      int comp_max = 1024;
      m_bvh_comp_buffer.clear(cast_span<const std::byte>(std::span<int> { &comp_max, 1 }));

      // Copy divided data to indirect dispatch buffer (divide by (256/32))
      m_bvh_div_sg_program.bind("b_data", m_bvh_curr_work);
      m_bvh_div_sg_program.bind("b_disp", m_bvh_div_sg_buffer);
      gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);
      gl::dispatch_compute(m_bvh_div_sg_dispatch);

      // Push uniform data
      m_bvh_unif_map->n_elems = i_delaunay.elems.size();
      m_bvh_unif_buffer.flush();

      // Bind relevant buffers
      m_bvh_bary_program.bind("b_unif", m_bvh_unif_buffer);
      m_bvh_bary_program.bind("b_elem", m_bvh_elem_buffer);
      m_bvh_bary_program.bind("b_colr", m_bvh_colr_buffer);
      m_bvh_bary_program.bind("b_comp", m_bvh_comp_buffer);
      m_bvh_bary_program.bind("b_curr", m_bvh_curr_work);
      m_bvh_bary_program.bind("b_pack", m_pack_buffer);
      m_bvh_bary_program.bind("b_posi", info("colr_buffer").read_only<gl::Buffer>());
      m_bvh_bary_program.bind("b_bary", info("bary_buffer").writeable<gl::Buffer>());

      // Dispatch work using indirect buffer, based on previous work data
      gl::sync::memory_barrier(gl::BarrierFlags::eBufferUpdate       | 
                               gl::BarrierFlags::eShaderStorageBuffer);
      gl::dispatch_compute(m_bvh_bary_dispatch);

      // uint curr_head = 0, disp_head = 0;
      // m_bvh_curr_work.get_as<uint>(std::span<uint> { &curr_head, 1 }, 1);
      // m_bvh_div_sg_buffer.get_as<uint>(std::span<uint> { &disp_head, 1 }, 1);
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