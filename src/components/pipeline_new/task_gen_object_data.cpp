#include <metameric/core/scene.hpp>
#include <metameric/components/pipeline_new/task_gen_object_data.hpp>
#include <metameric/components/misc/detail/scene.hpp>
#include <small_gl/texture.hpp>
#include <format>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;
  constexpr uint buffer_init_size    = 512u;

  // Generate a transforming view that performs unsigned integer index access over a range
  constexpr auto indexed_view(const auto &v) {
    return std::views::transform([&v](uint i) { return v[i]; });
  };

  GenObjectDataTask:: GenObjectDataTask(uint object_i)
  : m_object_i(object_i) { }

  bool GenObjectDataTask::is_active(SchedulerHandle &info) {
    met_trace();
    
    const auto &e_scene     = info.global("scene").getr<Scene>();
    const auto &e_object    = e_scene.components.objects[m_object_i];
    const auto &e_uplifting = e_scene.components.upliftings[e_object.value.uplifting_i];
    
    // TODO show fine-grained dependence on object materials and tesselation
    return e_object.state ||  e_uplifting.state            ||
           info("scene_handler", "mesh_data").is_mutated() ||
           info("scene_handler", "txtr_data").is_mutated() ||
           info("scene_handler", "uplf_data").is_mutated();
  }

  void GenObjectDataTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Initialize objects for compute dispatch
    m_program = {{ .type       = gl::ShaderType::eCompute,
                   .spirv_path = "resources/shaders/pipeline/gen_delaunay_weights.comp.spv",
                   .cross_path = "resources/shaders/pipeline/gen_delaunay_weights.comp.json" }};
    m_dispatch = { .bindable_program = &m_program }; 
    
    // Initialize uniform buffer and writeable, flushable mapping
    m_unif_buffer = {{ .size = sizeof(UnifLayout), .flags = buffer_create_flags }};
    m_unif_map    = m_unif_buffer.map_as<UnifLayout>(buffer_access_flags).data();
    m_unif_map->object_i = m_object_i;

    // Initialize buffer store for holding packed delaunay data, and map it
    m_pack_buffer = {{ .size = buffer_init_size * sizeof(ElemPack), .flags = buffer_create_flags }};
    m_pack_map    = m_pack_buffer.map_as<ElemPack>(buffer_access_flags);
  }

  void GenObjectDataTask::eval(SchedulerHandle &info) {
    met_trace_full();
    
    // Get external resources
    const auto &e_scene      = info.global("scene").getr<Scene>();
    const auto &e_object     = e_scene.components.objects[m_object_i];
    const auto &e_uplifting  = e_scene.components.upliftings[e_object.value.uplifting_i];
    const auto &e_txtr_data  = info("scene_handler", "txtr_data").getr<detail::RTTextureData>();
    const auto &e_uplf_data  = info("scene_handler", "uplf_data").getr<detail::RTUpliftingData>();
    const auto &e_objc_data  = info("scene_handler", "objc_data").getr<detail::RTObjectData>();

    auto uplifting_task_name = std::format("gen_upliftings.gen_uplifting_{}", e_object.value.uplifting_i);
    const auto &e_tesselation = info(uplifting_task_name, "tesselation").getr<AlDelaunay>();

    // Push stale packed tetrahedral data
    // TODO; move to gen_uplifting_data
    std::transform(std::execution::par_unseq, range_iter(e_tesselation.elems), m_pack_map.begin(), 
    [&](const auto &el) {
      const auto vts = el | indexed_view(e_tesselation.verts);
      ElemPack pack;
      pack.inv.block<3, 3>(0, 0) = (eig::Matrix3f() 
        << vts[0] - vts[3], vts[1] - vts[3], vts[2] - vts[3]
      ).finished().inverse();
      pack.sub.head<3>() = vts[3];
      return pack;
    });
    m_pack_buffer.flush(e_tesselation.elems.size() * sizeof(ElemPack));

    // Push uniform data
    m_unif_map->n_verts = e_tesselation.verts.size();
    m_unif_map->n_elems = e_tesselation.elems.size();
    m_unif_buffer.flush();

    // Bind required resources to corresponding targets
    m_program.bind("b_unif",          m_unif_buffer);
    m_program.bind("b_pack",          m_pack_buffer);
    m_program.bind("b_txtr_3f",       e_txtr_data.atlas_3f.texture());
    m_program.bind("b_uplf_4f",       e_uplf_data.atlas_4f.texture());
    // m_program.bind("b_buff_uplifts",  e_uplf_data.info_gl);
    m_program.bind("b_buff_objects",  e_objc_data.info_gl);
    m_program.bind("b_buff_textures", e_txtr_data.info_gl);
    

  }
} // namespace met