#include <metameric/core/data.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/components/pipeline/task_gen_delaunay_weights.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/utility.hpp>
#include <algorithm>
#include <bitset>
#include <execution>
#include <ranges>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;
  constexpr uint buffer_init_size    = 512u;
  constexpr uint colr_level_begin = 3; // 512 nodes wide
  constexpr uint elem_level_begin = 2; // 64 nodes wide

  // Generate a transforming view that performs unsigned integer index access over a range
  constexpr auto indexed_view(const auto &v) {
    return std::views::transform([&v](uint i) { return v[i]; });
  };

  void GenDelaunayWeightsTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources
    const auto &e_appl_data = info.global("appl_data").getr<ApplicationData>();
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
    info("delaunay").set<AlDelaunay>({ });
    const auto &elem_tree = info("elem_tree").set<BVH>(BVH(buffer_init_size)).getr<BVH>();
    
    // Initialize search tree over color data
    std::vector<Colr> colr_data(range_iter(e_colr_data.data()));
    const auto &i_colr_tree = info("colr_tree").set(BVHColr(cnt_span<Colr>(colr_data))).getr<BVHColr>();

    // Initialize buffer holding barycentric weights
    info("colr_buffer").set(std::move(colr_buffer)); // OpenGL buffer storing texture color positions
    info("vert_buffer").set(std::move(vert_buffer)); // OpenGL buffer storing delaunay vertex positions
    info("elem_buffer").set(std::move(elem_buffer)); // OpenGL buffer storing delaunay tetrahedral elements
    info("bary_buffer").init<gl::Buffer>({ .size = dispatch_n * sizeof(eig::Array4f) }); // Convex weights
    info("tree_buffer").init<gl::Buffer>({ .size = elem_tree.size_bytes_reserved(), .flags = gl::BufferCreateFlags::eStorageDynamic });

    // Initialize tree and work components
    m_bvh_colr_buffer = {{ .data = cast_span<const std::byte>(i_colr_tree.data()) }};
    m_bvh_colr_ordr_buffer = {{ .data = cast_span<const std::byte>(i_colr_tree.order()) }};
    m_bvh_elem_buffer = {{ .size = elem_tree.size_bytes_reserved(), .flags = gl::BufferCreateFlags::eStorageDynamic }};
    m_bvh_elem_ordr_buffer = {{ .size = buffer_init_size * sizeof(uint), .flags = gl::BufferCreateFlags::eStorageDynamic }};

    // Build a referral buffer; for a given color value, find its containing leaf node
    {
      std::vector<uint> refr_data(i_colr_tree.n_primitives());

      auto leaves = i_colr_tree.data(i_colr_tree.n_levels() - 1);
      auto order = i_colr_tree.order();

      #pragma omp parallel for
      for (int i = 0; i < leaves.size(); ++i) {
        const auto &leaf = leaves[i];
        guard_continue(leaf.n > 0);

        for (uint colr_i = leaf.i; colr_i < leaf.i + leaf.n; ++colr_i) {
          refr_data[order[colr_i]] = i;
        } // for colr_i
      } // for i

      /* for (uint i = 0; i < i_colr_tree.n_primitives(); ++i) {
        uint refr = buffer[i];
        const auto &leaf = leaves[refr];
        fmt::print("{} -> {}, in [{}, {}]\n", i, refr, leaf.i, leaf.i + leaf.n);
      } */

     /*  fmt::print("{}\n", buffer); */

      m_bvh_colr_refr_buffer = {{ .data = cnt_span<const std::byte>(refr_data) }};
      m_bvh_colr_flag_buffer = {{ .size = leaves.size() * sizeof(eig::Array2u), .flags = gl::BufferCreateFlags::eStorageDynamic }};
    }

    // Allocate leaf buffers
    constexpr size_t work_size = sizeof(eig::Array4u) + 64 * 1024 * 1024 * sizeof(eig::Array2u);
    m_bvh_curr_work   = {{ .size = work_size, .flags = gl::BufferCreateFlags::eStorageDynamic }};
    m_bvh_next_work   = {{ .size = work_size, .flags = gl::BufferCreateFlags::eStorageDynamic }};
    m_bvh_leaf_work   = {{ .size = work_size, .flags = gl::BufferCreateFlags::eStorageDynamic }};

    // Allocate uniform buffer and build persistent, flushable mapping
    m_bvh_unif_buffer = {{ .size = sizeof(BVHUniformBuffer), .flags = gl::BufferCreateFlags::eMapWritePersistent }};
    m_bvh_unif_map    = m_bvh_unif_buffer.map_as<BVHUniformBuffer>(gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush).data();

    m_bvh_div_32_program = {{ .type       = gl::ShaderType::eCompute,
                              .spirv_path = "resources/shaders/misc/dispatch_divide_32.comp.spv",
                              .cross_path = "resources/shaders/misc/dispatch_divide_32.comp.json" }};
    m_bvh_div_sg_program = {{ .type       = gl::ShaderType::eCompute,
                              .spirv_path = "resources/shaders/misc/dispatch_divide_sg.comp.spv",
                              .cross_path = "resources/shaders/misc/dispatch_divide_sg.comp.json" }};
    m_bvh_div_32_buffer = {{ .size = sizeof(eig::Array4u) }};
    m_bvh_div_sg_buffer = {{ .size = sizeof(eig::Array4u) }};
    m_bvh_div_32_dispatch = { .bindable_program = &m_bvh_div_32_program };
    m_bvh_div_sg_dispatch = { .bindable_program = &m_bvh_div_sg_program };

    m_bvh_desc_program = {{ .type       = gl::ShaderType::eCompute,
                            .spirv_path = "resources/shaders/pipeline/gen_delaunay_weights_traverse.comp.spv",
                            .cross_path = "resources/shaders/pipeline/gen_delaunay_weights_traverse.comp.json" }};
    m_bvh_bary_program = {{ .type       = gl::ShaderType::eCompute,
                            .spirv_path = "resources/shaders/pipeline/gen_delaunay_weights_accumulate.comp.spv",
                            .cross_path = "resources/shaders/pipeline/gen_delaunay_weights_accumulate.comp.json" }};
    m_bvh_desc_dispatch = { .buffer = &m_bvh_div_32_buffer, .bindable_program = &m_bvh_desc_program };
    m_bvh_bary_dispatch = { .buffer = &m_bvh_div_sg_buffer, .bindable_program = &m_bvh_bary_program };

    m_bvh_finl_program = {{ .type       = gl::ShaderType::eCompute,
                            .spirv_path = "resources/shaders/pipeline/gen_delaunay_weights_finalize.comp.spv",
                            .cross_path = "resources/shaders/pipeline/gen_delaunay_weights_finalize.comp.json" }};
    m_bvh_finl_dispatch = { .groups_x = dispatch_ndiv, .bindable_program = &m_bvh_finl_program }; 

    // Generate paired nodes at specified levels in elem/colr hierarchies
    auto init_pairs = detail::init_pair_data<8, 8>(elem_level_begin, colr_level_begin);

    // Subtract node offsets from element indices; we stay on this level and only ever build it
    uint elem_offs = init_pairs[0][0];
    std::for_each(std::execution::par_unseq, range_iter(init_pairs), 
      [elem_offs](eig::Array2u &u) { u[0] -= elem_offs; });

    // Strip node pairs referring to empty nodes in the color tree
    std::erase_if(init_pairs, 
      [&i_colr_tree](const eig::Array2u &u) { return i_colr_tree.data()[u[1]].n == 0; });
    
    // Pack together head and pair data into a single object for clearing input work queues
    std::vector<eig::Array2u> init_work_data = { eig::Array2u { init_pairs.size(), 0 }, 0 };
    std::ranges::copy(init_pairs, std::back_inserter(init_work_data));

    // Packed head data for clearing output work queues
    auto init_head_data = eig::Array4u(0);

    // Allocate buffers containing the correctly formatted initialization data
    m_bvh_init_work = {{ .data = cnt_span<const std::byte>(init_work_data) }};
    m_bvh_init_head = {{ .data = obj_span<const std::byte>(init_head_data) }};

    m_bvh_unif_map->n_colr_lvls = i_colr_tree.n_levels();
    m_bvh_unif_buffer.flush();
  }
  
  bool GenDelaunayWeightsTask::is_active(SchedulerHandle &info) {
    met_trace();
    return info("state", "proj_state").getr<ProjectState>().verts;
  }

  void GenDelaunayWeightsTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_proj_state = info("state", "proj_state").getr<ProjectState>();
    const auto &e_appl_data  = info.global("appl_data").getr<ApplicationData>();
    const auto &e_proj_data  = e_appl_data.project_data;

    // Get modified resources
    auto &i_delaunay    = info("delaunay").getw<AlDelaunay>();
    auto &i_elem_tree   = info("elem_tree").getw<BVH>();
    auto &i_colr_tree   = info("colr_tree").getr<BVHColr>();
    auto &i_vert_buffer = info("vert_buffer").getw<gl::Buffer>();
    auto &i_elem_buffer = info("elem_buffer").getw<gl::Buffer>();
    /* auto &i_tree_buffer = info("tree_buffer").writeable<gl::Buffer>(); */

    // Generate new delaunay structure and search tree
    std::vector<Colr> delaunay_input(e_proj_data.verts.size());
    std::ranges::transform(e_proj_data.verts, delaunay_input.begin(), [](const auto &vt) { return vt.colr_i; });
    i_delaunay = generate_delaunay<AlDelaunay, Colr>(delaunay_input);
    /* i_elem_tree.build(i_delaunay.verts, i_delaunay.elems); */

    // Recover triangle element data and store in project
    auto [_verts, elems, _norms, _uvs] = convert_mesh<AlMesh>(i_delaunay);
    info.global("appl_data").getw<ApplicationData>().project_data.elems = elems;

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
    
    /* // Push stale mesh tree data // TODO optimize?
    auto tree_data = cast_span<const std::byte>(i_elem_tree.data());
    i_tree_buffer.set(tree_data, tree_data.size()); // Specify size as buffer over-reserves data size */

    /* // Push single level of stale mesh tree data
    auto elem_tree_data = cast_span<const std::byte>(i_elem_tree.data(elem_level_begin));
    auto elem_tree_order = cast_span<const std::byte>(i_elem_tree.order());
    m_bvh_elem_buffer.set(elem_tree_data, elem_tree_data.size());
    m_bvh_elem_ordr_buffer.set(elem_tree_order, elem_tree_order.size()); */

    // Push uniform data
    m_uniform_map->n_verts = i_delaunay.verts.size();
    m_uniform_map->n_elems = i_delaunay.elems.size();
    m_uniform_buffer.flush();

    // Bind required buffers to corresponding targets
    m_program.bind("b_unif", m_uniform_buffer);
    m_program.bind("b_pack", m_pack_buffer);
    m_program.bind("b_posi", info("colr_buffer").getr<gl::Buffer>());
    m_program.bind("b_bary", info("bary_buffer").getw<gl::Buffer>());
    
    // Dispatch shader to generate delaunay convex weights
    gl::dispatch_compute(m_dispatch);

    /* { met_trace_full_n("bvh_testing");
      // Clear flag buffer; no work is yet flagged for computation
      m_bvh_colr_flag_buffer.clear();

      // Reset work queues; initial node pairs start at lower levels
      m_bvh_init_work.copy_to(m_bvh_curr_work, m_bvh_init_work.size());
      m_bvh_init_head.copy_to(m_bvh_leaf_work, sizeof(uint));

      // Update relevant uniform data for new meshing
      m_bvh_unif_map->n_elem_lvls = i_elem_tree.n_levels();
      m_bvh_unif_map->n_elems = i_delaunay.elems.size();
      m_bvh_unif_buffer.flush();

      // Iterate through levels of hierarchy, finding an optimal dual-hierarchy cut for computation
      for (uint i = colr_level_begin; i < i_colr_tree.n_levels() - 1; ++i) {
        // Reset next work head to starting work head
        m_bvh_init_head.copy_to(m_bvh_next_work, sizeof(uint));

        // Copy divided data to indirect dispatch buffer (divide by (256/8) for 8-wide subgroup clusters)
        m_bvh_div_32_program.bind("b_data", m_bvh_curr_work);
        m_bvh_div_32_program.bind("b_disp", m_bvh_div_32_buffer);
        gl::sync::memory_barrier(gl::BarrierFlags::eStorageBuffer);
        gl::dispatch_compute(m_bvh_div_32_dispatch);

        // Bind relevant buffers
        m_bvh_desc_program.bind("b_unif", m_bvh_unif_buffer);
        m_bvh_desc_program.bind("b_elem", m_bvh_elem_buffer);
        m_bvh_desc_program.bind("b_colr", m_bvh_colr_buffer);
        m_bvh_desc_program.bind("b_flag", m_bvh_colr_flag_buffer);
        m_bvh_desc_program.bind("b_curr", m_bvh_curr_work);
        m_bvh_desc_program.bind("b_next", m_bvh_next_work);

        // Dispatch work using indirect buffer, based on previous work data
        gl::sync::memory_barrier(gl::BarrierFlags::eBufferUpdate | gl::BarrierFlags::eStorageBuffer);
        gl::dispatch_compute(m_bvh_desc_dispatch);
        
        {
          uint head_curr = 0, head_next = 0;
          m_bvh_curr_work.get_as<uint>(std::span { &head_curr, 1 }, 1, 0);
          m_bvh_next_work.get_as<uint>(std::span { &head_next, 1 }, 1, 0);
          fmt::print("{} : {} -> {}\n", i, head_curr, head_next);
        }

        // Swap current/next work buffers
        std::swap(m_bvh_curr_work, m_bvh_next_work);
      } // for i */

      /* { // Debugging, again
        // Closures
        uint bvh_degr = 8;
        uint bvh_degr_log = 3;
        float bvh_degr_ln_div = 1.f / std::log(bvh_degr);
        const auto lvl_from_index = [bvh_degr_ln_div](uint i) -> uint {
          return uint(log(float(i) * 7.f + 6.f) * bvh_degr_ln_div);
        };
        const auto begin_from_lvl = [bvh_degr_log](uint lvl) -> uint {
          return (0x92492492 >> (31 - bvh_degr_log * lvl)) >> 3; 
        };

        // Get last work buffer
        using Work = eig::Array2u;
        std::vector<Work> work_buffer(4096);
        m_bvh_curr_work.get_as<Work>( work_buffer, work_buffer.size(), 2);

        for (auto &i : work_buffer) {
          uint lvl = lvl_from_index(i[1]);
          uint floffs = i[1] - begin_from_lvl(lvl);
          uint flag_i = 2 * floffs + i[0] / 32;
          // uint lvl = be
          fmt::print("work: {}, lvl: {}, lvl_begin: {}, floffs: {}, flag_i: {}\n", i, lvl,  begin_from_lvl(lvl), floffs, flag_i);
        }
        std::exit(0);


        auto leaves = i_colr_tree.data(i_colr_tree.n_levels() - 1);
        std::vector<eig::Array2u> flags(leaves.size());
        m_bvh_colr_flag_buffer.get_as<eig::Array2u>(flags, flags.size());
        
        for (uint i = 0; i < leaves.size(); ++i) {
          const auto &node = leaves[i];
          guard_continue(node.n > 0);
          fmt::print("{} - [{}, {}]\n",
            i,
            std::bitset<32>(flags[i][0]).to_string(),
            std::bitset<32>(flags[i][1]).to_string()
          );
        }

        std::exit(0);
      } */

      /* // Process finalized results, recovering barycentric weights
      { met_trace_full_n("finalize");
        // Bind relevant buffers
        m_bvh_finl_program.bind("b_unif", m_uniform_buffer);
        m_bvh_finl_program.bind("b_flag", m_bvh_colr_flag_buffer);
        m_bvh_finl_program.bind("b_pack", m_pack_buffer);
        m_bvh_finl_program.bind("b_elem", m_bvh_elem_buffer);
        m_bvh_finl_program.bind("b_ordr", m_bvh_elem_ordr_buffer);
        m_bvh_finl_program.bind("b_refr", m_bvh_colr_refr_buffer);
        m_bvh_finl_program.bind("b_posi", info("colr_buffer").getr<gl::Buffer>());
        m_bvh_finl_program.bind("b_bary", info("bary_buffer").writeable<gl::Buffer>());

        // Dispatch shader to finalize delaunay convex weights
        gl::sync::memory_barrier(gl::BarrierFlags::eStorageBuffer);
        gl::dispatch_compute(m_bvh_finl_dispatch);
      } */

      // Process bottom part of cut
      /* { met_trace_full_n("bottom_cut");
        // Copy divided data to indirect dispatch buffer (divide by (256/sg) for subgroup-wide clusters)
        m_bvh_div_sg_program.bind("b_data", m_bvh_curr_work);
        m_bvh_div_sg_program.bind("b_disp", m_bvh_div_sg_buffer);
        gl::sync::memory_barrier(gl::BarrierFlags::eStorageBuffer);
        gl::dispatch_compute(m_bvh_div_sg_dispatch);

        // Bind relevant buffers
        m_bvh_bary_program.bind("b_unif", m_bvh_unif_buffer);
        m_bvh_bary_program.bind("b_elem", m_bvh_elem_buffer);
        m_bvh_bary_program.bind("b_colr", m_bvh_colr_buffer);
        m_bvh_bary_program.bind("b_ordr", m_bvh_colr_ordr_buffer);
        m_bvh_bary_program.bind("b_flag", m_bvh_flag_buffer);
        m_bvh_bary_program.bind("b_work", m_bvh_curr_work);

        // Dispatch work using indirect buffer, based on previous work data
        gl::sync::memory_barrier(gl::BarrierFlags::eStorageBuffer);
        gl::dispatch_compute(m_bvh_bary_dispatch);
      } */

      // Process leaf part of cut
      /* { met_trace_full_n("leaf_cut");
        // Copy divided data to indirect dispatch buffer (divide by (256/sg) for subgroup-wide clusters)
        m_bvh_div_sg_program.bind("b_data", m_bvh_leaf_work);
        m_bvh_div_sg_program.bind("b_disp", m_bvh_div_sg_buffer);
        gl::sync::memory_barrier(gl::BarrierFlags::eStorageBuffer);
        gl::dispatch_compute(m_bvh_div_sg_dispatch);

        // Bind relevant buffers
        m_bvh_bary_program.bind("b_unif", m_bvh_unif_buffer);
        m_bvh_bary_program.bind("b_elem", m_bvh_elem_buffer);
        m_bvh_bary_program.bind("b_colr", m_bvh_colr_buffer);
        m_bvh_bary_program.bind("b_ordr", m_bvh_colr_ordr_buffer);
        m_bvh_bary_program.bind("b_flag", m_bvh_flag_buffer);
        m_bvh_bary_program.bind("b_work", m_bvh_leaf_work);

        // Dispatch work using indirect buffer, based on previous work data
        gl::sync::memory_barrier(gl::BarrierFlags::eStorageBuffer);
        gl::dispatch_compute(m_bvh_bary_dispatch);
      } */
    // } // bvh_testing
  } 
} // namespace met