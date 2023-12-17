#include <metameric/core/scene.hpp>
#include <metameric/components/views/mesh_viewport/task_draw_raytrace.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/render/scene_data.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/dispatch.hpp>

namespace met {
  constexpr uint ray_init_wg_size = 16; // x 16
  constexpr uint ray_isct_wg_size = 256;
  constexpr uint ray_draw_wg_size = 256;
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  bool MeshViewportDrawRaytraceTask::is_active(SchedulerHandle &info) {
    met_trace();
    auto is_objc_present = !info.global("scene").getr<Scene>().components.objects.empty();
    auto is_view_present = info.relative("viewport_begin")("is_active").getr<bool>();
    return is_objc_present && is_view_present; 
  }
  
  void MeshViewportDrawRaytraceTask::init(SchedulerHandle &info) {
    met_trace_full();
    
    // Initialize program objects
    m_program_ray_init = {{ .type       = gl::ShaderType::eCompute,
                            .spirv_path = "resources/shaders/render/ray_init.comp.spv",
                            .cross_path = "resources/shaders/render/ray_init.comp.json",
                            .spec_const = {{ 0, ray_init_wg_size }} }};
    m_program_ray_isct = {{ .type       = gl::ShaderType::eCompute,
                            .spirv_path = "resources/shaders/render/ray_isct.comp.spv",
                            .cross_path = "resources/shaders/render/ray_isct.comp.json",
                            .spec_const = {{ 0, ray_isct_wg_size }} }};
    
    // Initialize uniform buffers and corresponding mapping
    m_buffer_unif     = {{ .size = sizeof(UnifLayout), .flags = buffer_create_flags }};
    m_buffer_unif_map = m_buffer_unif.map_as<UnifLayout>(buffer_access_flags).data();

    // Internal target texture; can be differently sized
    info("target").set<gl::Texture2d4f>({ });
  }
  
  void MeshViewportDrawRaytraceTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get handles to relative task resource
    auto begin_handle   = info.relative("viewport_begin");
    auto target_handle  = begin_handle("lrgb_target");
    auto arcball_handle = info.relative("viewport_input")("arcball");
    auto object_handle  = info("scene_handler", "objc_data");
    auto weight_handle  = info("gen_objects", "bary_data");

    // Get shared resources 
    const auto &e_scene     = info.global("scene").getr<Scene>();
    const auto &e_objc_data = info("scene_handler", "objc_data").getr<ObjectData>();
    const auto &e_bvhs_data = info("scene_handler", "bvhs_data").getr<BVHData>();
    const auto &e_mesh_data = info("scene_handler", "mesh_data").getr<MeshData>();
    // const auto &e_txtr_data = info("scene_handler", "txtr_data").getr<detail::TextureData>();
    // const auto &e_uplf_data = info("scene_handler", "uplf_data").getr<detail::UpliftingData>();
    // const auto &e_cmfs_data = info("scene_handler", "cmfs_data").getr<detail::ObserverData>();
    // const auto &e_illm_data = info("scene_handler", "illm_data").getr<detail::IlluminantData>();
    // const auto &e_csys_data = info("scene_handler", "csys_data").getr<detail::ColorSystemData>();
    // const auto &e_bary_data = info("gen_objects", "bary_data").getr<detail::TextureAtlas<float, 4>>();
    // const auto &e_gbuffer   = info.relative("viewport_draw_gbuffer")("gbuffer").getr<gl::Texture2d4f>();

    // Get modified resources
    auto &i_target = info("target").getw<gl::Texture2d4f>();

    // Some state flags to test when to restart sampling
    bool rebuild_frame = !m_buffer_sampler_state.is_init() || target_handle.is_mutated();
    bool restart_frame = arcball_handle.is_mutated() || object_handle.is_mutated() || weight_handle.is_mutated();

    // Re-initialize state if target viewport is resized or needs initializing
    if (rebuild_frame) {
      const auto &e_target = target_handle.getr<gl::Texture2d4f>();

      // Resize internal state buffer and target accordingly
      m_buffer_sampler_state = {{ .size = e_target.size().prod() * sizeof(eig::Array2u)   	   }};
      i_target           = {{ .size = e_target.size()             }};
      m_buffer_work_head = {{ .size = sizeof(uint)                }};
      m_buffer_work      = {{ .size = e_target.size().prod() * 32 }};
      
      // Push target size to uniform data
      m_buffer_unif_map->view_size = e_target.size();
    }

    // Re-start state if something like camera/scene changed
    if (rebuild_frame || restart_frame) {
      const auto &e_arcball = arcball_handle.getr<detail::Arcball>();

      // Push camera data to uniform data
      m_buffer_unif_map->view_inv = e_arcball.view().inverse().matrix();
      m_buffer_unif_map->fovy_tan = std::tan(e_arcball.fov_y() * .5f);
      m_buffer_unif_map->aspect   = e_arcball.aspect();
      m_buffer_unif.flush();

      // Set cumulative frame to 0
      i_target.clear();
    }

    // First stage; initial ray generation
    {
      // Specify dispatch size
      auto dispatch_n    = i_target.size();
      auto dispatch_ndiv = ceil_div(dispatch_n, ray_init_wg_size);

      // Bind required resources to their corresponding targets
      m_program_ray_init.bind("b_buff_unif",      m_buffer_unif);
      m_program_ray_init.bind("b_buff_work_head", m_buffer_work_head);
      m_program_ray_init.bind("b_buff_work",      m_buffer_work);
      
      // Dispatch compute shader
      gl::dispatch_compute({ .groups_x         = dispatch_ndiv.x(),
                             .groups_y         = dispatch_ndiv.y(),
                             .bindable_program = &m_program_ray_init });
    }

    // Second stage; ray intersect
    {
      // Specify dispatch size
      auto dispatch_n    = i_target.size().prod();
      auto dispatch_ndiv = ceil_div(dispatch_n, ray_isct_wg_size);

      // Bind required resources to their corresponding targets
      m_program_ray_isct.bind("b_buff_unif",      m_buffer_unif);
      m_program_ray_isct.bind("b_buff_work_head", m_buffer_work_head);
      m_program_ray_isct.bind("b_buff_work",      m_buffer_work);
      m_program_ray_isct.bind("b_buff_objc_info", e_objc_data.info_gl);
      m_program_ray_isct.bind("b_buff_bvhs_info", e_bvhs_data.info_gl);
      m_program_ray_isct.bind("b_buff_bvhs_node", e_bvhs_data.nodes);
      m_program_ray_isct.bind("b_buff_bvhs_prim", e_bvhs_data.prims);
      m_program_ray_isct.bind("b_target_4f",      i_target);
      
      // Dispatch compute shader
      gl::dispatch_compute({ .groups_x         = dispatch_ndiv,
                             .bindable_program = &m_program_ray_isct });
    }

    // Debug stage; query data
    /* {
      struct RayQuery {
        eig::Array3f o;
        float        t;
        eig::Array3f d;
        uint         object_i;
      };
      
      std::vector<RayQuery> buffer(4);
      m_buffer_work.get(cnt_span<std::byte>(buffer), buffer.size() * sizeof(RayQuery));
      for (uint i = 0; i < buffer.size(); ++i) {
        auto query = buffer[i];
        fmt::print("{}: o = {}, d = {}, t = {}\n", i, query.o, query.d, query.t);
      }

      const auto &e_arcball = arcball_handle.getr<detail::Arcball>();
      fmt::print("pos = {}\n", e_arcball.eye_pos());

      auto ray = e_arcball.generate_ray({ 0.5f, 0.5f });
      fmt::print("should be near o = {}, d = {}\n", ray.o, ray.d);
    } */
  }
} // namespace met