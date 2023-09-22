#include <metameric/core/data.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/dev/task_draw_bvh.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/utility.hpp>
#include <algorithm>
#include <bit>
#include <execution>
#include <ranges>
#include <vector>

namespace met {
  // Buffer flags for flushable, persistent, write-only mapping
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;
  constexpr uint max_primitive_support = 512u;

  void ViewportDrawBVHTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_appl_data = info.global("appl_data").getr<ApplicationData>();
    const auto &e_colr_data = e_appl_data.loaded_texture;
    
    // Setup mapped buffer objects
    m_unif_buffer = {{ .size = sizeof(UniformBuffer), .flags = buffer_create_flags }};
    m_camr_buffer = {{ .size = sizeof(CameraBuffer), .flags = buffer_create_flags }};
    m_unif_map    = m_unif_buffer.map_as<UniformBuffer>(buffer_access_flags).data();
    m_camr_map    = m_camr_buffer.map_as<CameraBuffer>(buffer_access_flags).data();
    
    // Load shader object
    m_program = {{ .type = gl::ShaderType::eVertex,   
                   .spirv_path = "resources/shaders/views/dev/draw_bvh.vert.spv",
                   .cross_path = "resources/shaders/views/dev/draw_bvh.vert.json" },
                 { .type = gl::ShaderType::eFragment, 
                   .spirv_path = "resources/shaders/views/dev/draw_bvh.frag.spv",
                   .cross_path = "resources/shaders/views/dev/draw_bvh.frag.json" }};

    // Specify array and draw object
    m_array = {{ }};
    m_draw  = {
      .type             = gl::PrimitiveType::eLines,
      .vertex_count     = 0,
      // .draw_op          = gl::DrawOp::eLine,
      .bindable_array   = &m_array,
      .bindable_program = &m_program
    };

    // Let's start at the top for now
    m_tree_index = 0;
    m_tree_level = 0;

    // Build a BVH over texture color data
    std::vector<Colr> colr_data(range_iter(e_colr_data.data()));
    m_tree_points = BVHPoint(cnt_span<Colr>(colr_data));
    m_tree_buffer = {{ .data = cast_span<const std::byte>(m_tree_points.data()) }};
  }
  
  void ViewportDrawBVHTask::eval(SchedulerHandle &info) {
    met_trace_full();
    
    // Get external resources
    const auto &e_delaunay    = info("gen_convex_weights", "delaunay").getr<AlDelaunay>();
    const auto &e_view_state  = info("state", "view_state").getr<ViewportState>();

    // Select tree
    const auto &tree_data   = info("gen_convex_weights", "elem_tree").getr<BVH>();
    const auto &tree_buffer = info("gen_convex_weights", "tree_buffer").getr<gl::Buffer>();
    // const auto &tree_data   = m_tree_points;
    // const auto &tree_buffer = m_tree_buffer;

    // Determine draw count
    auto node_level     = tree_data.data(m_tree_level);
    uint draw_begin     = std::distance(tree_data.data().begin(), node_level.begin() /* + m_tree_index */); 
    uint draw_extent    = node_level.size(); // 1
    m_draw.vertex_count = 48 * draw_extent;

    // Push uniform data
    m_unif_map->node_begin  = draw_begin;
    m_unif_map->node_extent = draw_extent;
    m_unif_buffer.flush();

    // On relevant state change, update uniform buffer data
    if (e_view_state.camera_matrix || e_view_state.camera_aspect) {
      const auto &e_arcball = info("viewport.input", "arcball").getr<detail::Arcball>();
      m_camr_map->matrix = e_arcball.full().matrix();
      m_camr_map->aspect = { 1.f, e_arcball.m_aspect };
      m_camr_buffer.flush();
    }

    // Set OpenGL state shared for the coming draw operations
    auto shared_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eDepthTest, true),
                                 gl::state::ScopedSet(gl::DrawCapability::eBlendOp,   false),
                                 gl::state::ScopedSet(gl::DrawCapability::eCullOp,    false),
                                 gl::state::ScopedSet(gl::DrawCapability::eMSAA,      false) };

    // Bind resources and dispatch draw
    m_program.bind("b_tree", tree_buffer);
    m_program.bind("b_unif", m_unif_buffer);
    m_program.bind("b_camr", m_camr_buffer);

    gl::dispatch_draw(m_draw);

    // Spawn ImGui debug window
    if (ImGui::Begin("BVH debug window")) {
      uint tree_level_min = 0, tree_level_max = tree_data.n_levels() - 1;
      ImGui::SliderScalar("Level", ImGuiDataType_U32, &m_tree_level, &tree_level_min, &tree_level_max);

      // uint tree_index_min = 0, tree_index_max = tree_data.data(m_tree_level).size() - 1;
      // ImGui::SliderScalar("Index", ImGuiDataType_U32, &m_tree_index, &tree_index_min, &tree_index_max);

      const auto &node = tree_data.data()[draw_begin];
      // ImGui::Value("Node index", draw_begin);
      ImGui::Value("Node begin", node.i);
      ImGui::Value("Node end",   node.i + node.n - 1);
      ImGui::Value("Node size",  node.n);
    }
    ImGui::End();
  }
} // namespace met