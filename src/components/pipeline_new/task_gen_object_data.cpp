#include <metameric/core/scene.hpp>
#include <metameric/components/pipeline_new/task_gen_object_data.hpp>
#include <metameric/components/misc/detail/scene.hpp>
#include <small_gl/texture.hpp>
#include <format>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  GenObjectDataTask:: GenObjectDataTask(uint object_i)
  : m_object_i(object_i) { }

  bool GenObjectDataTask::is_active(SchedulerHandle &info) {
    met_trace();
    
    // Get external resources
    const auto &e_scene     = info.global("scene").getr<Scene>();
    const auto &e_object    = e_scene.components.objects[m_object_i];
    const auto &e_uplifting = e_scene.components.upliftings[e_object.value.uplifting_i];

     // Force on first run, then make dependent on uplifting/object
    return is_first_eval()                                 ||
           e_object.state                                  ||  
           e_uplifting.state                               ||
           info("gen_objects",   "bary_data").is_mutated() ||
           info("scene_handler", "mesh_data").is_mutated() || 
           info("scene_handler", "txtr_data").is_mutated();
  }

  void GenObjectDataTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Initialize objects for compute dispatch
    m_program = {{ .type       = gl::ShaderType::eCompute,
                   .spirv_path = "resources/shaders/pipeline_new/gen_tesselation_weights.comp.spv",
                   .cross_path = "resources/shaders/pipeline_new/gen_tesselation_weights.comp.json" }};
    m_dispatch = { .bindable_program = &m_program }; 
    
    // Initialize uniform buffer and writeable, flushable mapping
    m_unif_buffer = {{ .size = sizeof(UnifLayout), .flags = buffer_create_flags }};
    m_unif_map    = m_unif_buffer.map_as<UnifLayout>(buffer_access_flags).data();
    m_unif_map->object_i = m_object_i;
  }

  void GenObjectDataTask::eval(SchedulerHandle &info) {
    met_trace_full();
    
    // Get external resources
    const auto &e_scene     = info.global("scene").getr<Scene>();
    const auto &e_object    = e_scene.components.objects[m_object_i];
    const auto &e_uplifting = e_scene.components.upliftings[e_object.value.uplifting_i];
    const auto &e_txtr_data = info("scene_handler", "txtr_data").getr<detail::RTTextureData>();
    const auto &e_uplf_data = info("scene_handler", "uplf_data").getr<detail::RTUpliftingData>();
    const auto &e_objc_data = info("scene_handler", "objc_data").getr<detail::RTObjectData>();
    const auto &e_bary_data = info("gen_objects", "bary_data").getr<detail::RTObjectWeightData>();
    const auto &e_objc_info = e_objc_data.info[m_object_i];

    // Get external resources from object's selected uplifting
    auto uplifting_task_name = std::format("gen_upliftings.gen_uplifting_{}", e_object.value.uplifting_i);
    const auto &e_tesselation = info(uplifting_task_name, "tesselation").getr<AlDelaunay>();
    const auto &e_tesselation_pack = info(uplifting_task_name, "tesselation_pack").getr<gl::Buffer>();

    // Determine dispatch size
    auto dispatch_n    = e_objc_info.size;
    auto dispatch_ndiv = ceil_div(dispatch_n, 16u);
    m_dispatch.groups_x = dispatch_ndiv.x();
    m_dispatch.groups_y = dispatch_ndiv.y();

    // Push uniform data
    m_unif_map->dispatch_n = dispatch_n;
    m_unif_buffer.flush();

    // Bind required resources to corresponding targets
    m_program.bind("b_buff_unif",     m_unif_buffer);
    m_program.bind("b_txtr_3f",       e_txtr_data.atlas_3f.texture());
    m_program.bind("b_bary_4f",       e_bary_data.atls_4f.texture());
    m_program.bind("b_buff_pack",     e_tesselation_pack);
    m_program.bind("b_buff_objects",  e_objc_data.info_gl);
    m_program.bind("b_buff_textures", e_txtr_data.info_gl);
    m_program.bind("b_buff_uplifts",  e_uplf_data.info_gl);
    m_program.bind("b_buff_weights",  e_bary_data.info_gl);

    gl::dispatch_compute(m_dispatch);
    
    fmt::print("Gen object, dispat {} * {}\n", dispatch_n.x(), dispatch_n.y());
    if (e_objc_info.is_albedo_sampled) {
      fmt::print("\tGen {}, sample from {}\n", m_object_i, e_objc_info.albedo_i);
    } else {
      fmt::print("\tGen {}, value is {}\n", m_object_i, e_objc_info.albedo_v);
    }
  }
} // namespace met