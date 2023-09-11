#include <metameric/components/misc/task_scene_handler.hpp>

namespace met {
  namespace detail {
    std::pair<
      std::vector<eig::Array4f>,
      std::vector<eig::Array4f>
    > pack(const AlMeshData &m) {
      std::vector<eig::Array4f> a(m.verts.size()),
                                b(m.verts.size());

      #pragma omp parallel for
      for (int i = 0; i < a.size(); ++i) {
        a[i] = (eig::Array4f() << m.verts[i], m.uvs[i][0]).finished();
        b[i] = (eig::Array4f() << m.norms[i], m.uvs[i][1]).finished();
      }
      
      return { std::move(a), std::move(b) };
    }
  }

  void SceneHandlerTask::init(SchedulerHandle &info) {
    met_trace();
    
    // Initialize empty holder objects for gpu-side resources
    info("meshes").set<std::vector<detail::MeshLayout>>({ });
    info("textures").set<std::vector<detail::TextureLayout>>({ });
    info("mesh_data").set<detail::RTMeshData>({ });
    info("objc_data").set<detail::RTObjectData>({ });

    // Force upload of all resources  on first run
    m_is_init = false;
  }

  void SceneHandlerTask::eval(SchedulerHandle &info) {
    met_trace_full();
    
    const auto &e_scene_handler = info.global("scene_handler").read_only<SceneHandler>();
    const auto &e_scene         = e_scene_handler.scene;
    const auto &e_objects       = e_scene.components.objects;

    // Process updates to gpu-side mesh resources 
    if (!m_is_init || e_scene.resources.meshes.is_mutated()) {
      auto &i_mesh_data = info("mesh_data").writeable<detail::RTMeshData>();
      {
        // Gather vertex/element lengths and offsets per mesh resources
        std::vector<uint> verts_size, elems_size, verts_offs, elems_offs;
        std::transform(range_iter(e_scene.resources.meshes), std::back_inserter(verts_size), 
          [](const auto &m) { return static_cast<uint>(m.value().verts.size()); });
        std::exclusive_scan(range_iter(verts_size), std::back_inserter(verts_offs), 0u);
        std::transform(range_iter(e_scene.resources.meshes), std::back_inserter(elems_size), 
          [](const auto &m) { return static_cast<uint>(m.value().elems.size()); });
        std::exclusive_scan(range_iter(elems_size), std::back_inserter(elems_offs), 0u);

        // Total vertex/element lengths across all meshes
        uint n_verts = verts_size.at(verts_size.size() - 1) + verts_offs.at(verts_offs.size() - 1);
        uint n_elems = elems_size.at(elems_size.size() - 1) + elems_offs.at(elems_offs.size() - 1);

        // Holder for packed layout of all meshes
        std::vector<detail::RTMeshInfo> packed_info(e_scene.resources.meshes.size());

        // Holders for packed data of all meshes
        std::vector<eig::Array4f>   packed_verts_a(n_verts),
                                    packed_verts_b(n_verts);
        std::vector<eig::Array3u>   packed_elems(n_elems);
        std::vector<eig::AlArray3u> packed_elems_al(n_elems);

        // Fill packed layout/data vectors
        #pragma omp parallel for
        for (int i = 0; i < e_scene.resources.meshes.size(); ++i) {
          const auto &mesh = e_scene.resources.meshes[i].value();
          auto [a, b] = detail::pack(mesh);
          
          // Copy over packed data to the correctly offset range;
          // adjust element indices to refer to the offset range as well
          rng::copy(a,               packed_verts_a.begin()  + verts_offs[i]);
          rng::copy(b,               packed_verts_b.begin()  + verts_offs[i]);
          rng::transform(mesh.elems, packed_elems.begin()    + elems_offs[i], 
            [offs = verts_offs[i]](const auto &v) { return (v + offs).eval(); });
          rng::transform(mesh.elems, packed_elems_al.begin() + elems_offs[i], 
            [offs = verts_offs[i]](const auto &v) { return (v + offs).eval(); });

          packed_info[i] = detail::RTMeshInfo {
            .verts_offs = verts_offs[i], .verts_size = verts_size[i],
            .elems_offs = elems_offs[i], .elems_size = elems_size[i],
          };
        }

        // Push layout/data to buffers
        i_mesh_data = detail::RTMeshData { 
          .info     = packed_info,
          .info_gl  = {{ .data = cnt_span<const std::byte>(packed_info)     }},
          .verts_a  = {{ .data = cnt_span<const std::byte>(packed_verts_a)  }},
          .verts_b  = {{ .data = cnt_span<const std::byte>(packed_verts_b)  }},
          .elems    = {{ .data = cnt_span<const std::byte>(packed_elems)    }},
          .elems_al = {{ .data = cnt_span<const std::byte>(packed_elems_al) }},
        };

        // Define corresponding vertex array object
        i_mesh_data.array = {{
          .buffers = {{ .buffer = &i_mesh_data.verts_a, .index = 0, .stride = sizeof(eig::Array4f) },
                      { .buffer = &i_mesh_data.verts_b, .index = 1, .stride = sizeof(eig::Array4f) }},
          .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e4 },
                      { .attrib_index = 1, .buffer_index = 1, .size = gl::VertexAttribSize::e4 }},
          .elements = &i_mesh_data.elems
        }};
      }

      // TODO; delete below!

      auto &i_meshes = info("meshes").writeable<std::vector<detail::MeshLayout>>();
      i_meshes.resize(e_scene.resources.meshes.size());

      for (uint i = 0; i < i_meshes.size(); ++i) {
        const auto &rsrc = e_scene.resources.meshes[i];
        guard_continue(!m_is_init || rsrc.is_mutated());
        i_meshes[i] = detail::MeshLayout::realize(rsrc.value());
      } // for (uint i)
    }

    if (!m_is_init || e_objects.is_mutated()) {
      auto &i_objc_data = info("objc_data").writeable<detail::RTObjectData>();

      // Initialize or resize object buffer to accomodate
      if (!i_objc_data.info_gl.is_init() || e_objects.size() != i_objc_data.info.size()) {
        i_objc_data.info.resize(e_objects.size());
        i_objc_data.info_gl = {{ .size  = e_objects.size() * sizeof(detail::RTObjectInfo),
                                 .flags = gl::BufferCreateFlags::eStorageDynamic }};
      }
      
      // Process updates to gl-side object info
      for (uint i = 0; i < e_objects.size(); ++i) {
        const auto &component = e_objects[i];
        const auto &object    = component.value;
        
        guard_continue(!m_is_init || component.state.is_mutated());

        i_objc_data.info[i] = {
          .trf         = object.trf.matrix(),
          .trf_inv     = object.trf.inverse().matrix(),
          .is_active   = object.is_active,
          .mesh_i      = object.mesh_i,
          .uplifting_i = object.uplifting_i,

          // Fill materials later
          // .albedo_use_sampler = object.diffuse.index() != 0,
        };

        i_objc_data.info_gl.set(obj_span<const std::byte>(i_objc_data.info[i]),
                                  sizeof(detail::RTObjectInfo),
                                  sizeof(detail::RTObjectInfo) * static_cast<size_t>(i));
      } // for (uint i)
    }
    
    // Process updates to gpu-side image resources
    if (!m_is_init || e_scene.resources.images.is_mutated()) {
      auto &i_textures = info("textures").writeable<std::vector<detail::TextureLayout>>();
      i_textures.resize(e_scene.resources.images.size());
      
      for (uint i = 0; i < i_textures.size(); ++i) {
        const auto &rsrc = e_scene.resources.images[i];
        guard_continue(!m_is_init || rsrc.is_mutated());
        i_textures[i] = detail::TextureLayout::realize(rsrc.value());
      } // for (uint i)
    }

    // Process updates to gpu-side illuminant resources
    if (!m_is_init || e_scene.resources.illuminants.is_mutated()) {
      // ...
    }
    
    // Process updates to gpu-side observer resources
    if (!m_is_init || e_scene.resources.observers.is_mutated()) {
      // ...
    }
    
    // Process updates to gpu-side basis function resources
    if (!m_is_init || e_scene.resources.bases.is_mutated()) {
      // ...
    }

    // Scene resources are now up-to-date;
    // do some bookkeeping on state tracking across resources/components
    {
      auto &e_scene_handler = info.global("scene_handler").writeable<SceneHandler>();
      auto &e_scene         = e_scene_handler.scene;

      e_scene.resources.meshes.set_mutated(false);
      e_scene.resources.images.set_mutated(false);
      e_scene.resources.illuminants.set_mutated(false);
      e_scene.resources.observers.set_mutated(false);
      e_scene.resources.bases.set_mutated(false);

      e_scene.components.colr_systems.test_mutated();
      e_scene.components.emitters.test_mutated();
      e_scene.components.objects.test_mutated();
      e_scene.components.upliftings.test_mutated();
    }

    m_is_init = true;
  }
} // namespace met