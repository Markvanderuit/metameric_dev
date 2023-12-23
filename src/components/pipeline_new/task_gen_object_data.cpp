#include <metameric/core/scene.hpp>
#include <metameric/components/pipeline_new/task_gen_object_data.hpp>
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
    const auto &e_settings  = e_scene.components.settings;
    
    // Force on first run, then make dependent on uplifting/object/texture settings. Yikes
    return is_first_eval()               ||
           e_object.state                ||  
           e_uplifting.state             ||
           e_scene.resources.meshes      || 
           e_scene.resources.images      ||
           e_settings.state.texture_size ;
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

    // Target texture here is gl-side
    const auto &e_barycentrics = e_scene.components.upliftings.gl.texture_weights;

    // Get external resources from object's corresponding, selected uplifting
    // An object has only one uplifting structure associated to it,
    // but several objects can reuse the same uplifting
    auto uplifting_task_name = std::format("gen_upliftings.gen_uplifting_{}", e_object.value.uplifting_i);
    const auto &e_tesselation_data = info(uplifting_task_name, "tesselation_data").getr<gl::Buffer>();
    const auto &e_tesselation_pack = info(uplifting_task_name, "tesselation_pack").getr<gl::Buffer>();

    // Determine dispatch size as the size of the object's patch in the barycentric texture atlas
    auto dispatch_n     = e_barycentrics.patch(m_object_i).size;
    auto dispatch_ndiv  = ceil_div(dispatch_n, 16u);
    m_dispatch.groups_x = dispatch_ndiv.x();
    m_dispatch.groups_y = dispatch_ndiv.y();

    // Push uniform data
    m_unif_map->dispatch_n = dispatch_n;
    m_unif_buffer.flush();

    // Bind required resources to corresponding targets
    m_program.bind("b_buff_unif",        m_unif_buffer);
    m_program.bind("b_buff_uplift_data", e_tesselation_data);
    m_program.bind("b_buff_uplift_pack", e_tesselation_pack);
    m_program.bind("b_buff_objects",     e_scene.components.objects.gl.object_info);
    m_program.bind("b_buff_textures",    e_scene.resources.images.gl.texture_info);
    m_program.bind("b_buff_weights",     e_barycentrics.buffer());
    m_program.bind("b_bary_4f",          e_barycentrics.texture());
    m_program.bind("b_txtr_3f",          e_scene.resources.images.gl.texture_atlas_3f.texture());

    gl::dispatch_compute(m_dispatch);
  }
} // namespace met