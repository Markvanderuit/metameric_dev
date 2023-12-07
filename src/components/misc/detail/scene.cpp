#include <metameric/components/misc/detail/scene.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/embree.hpp>
#include <algorithm>
#include <deque>
#include <execution>
#include <ranges>
#include <vector>

namespace met::detail {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  RTTextureData::RTTextureData(const Scene &scene) {
    met_trace();
    // Initialize on first run
    // update(scene);
  }

  bool RTTextureData::is_stale(const Scene &scene) const {
    met_trace();
    return scene.resources.images.is_mutated() || scene.components.settings.state.texture_size;
  }

  void RTTextureData::update(const Scene &scene) {
    met_trace_full();

    // Get external resources
    const auto &e_images   = scene.resources.images;
    const auto &e_settings = scene.components.settings.value.texture_size;

    guard(!e_images.empty());

    // Keep track of which atlas' position a texture needs to be stuffed in
    std::vector<uint> atlas_indices(e_images.size(), std::numeric_limits<uint>::max());\

    // Generate inputs for texture atlas
    std::vector<eig::Array2u> inputs_3f, inputs_1f;
    for (uint i = 0; i < e_images.size(); ++i) {
      const auto &img = e_images[i].value();
      bool is_3f = img.pixel_frmt() == Image::PixelFormat::eRGB;

      atlas_indices[i] = is_3f ? inputs_3f.size() : inputs_1f.size();
      
      if (is_3f) inputs_3f.push_back(img.size());
      else       inputs_1f.push_back(img.size());
    } // for (uint i)

    // Determine maximum texture sizes, and texture scaling necessary to uphold size settings
    eig::Array2u maximal_3f = rng::fold_left(inputs_3f, eig::Array2u(0), 
      [](auto a, auto b) { return a.cwiseMax(b).eval(); });
    eig::Array2u maximal_1f = rng::fold_left(inputs_1f, eig::Array2u(0), 
      [](auto a, auto b) { return a.cwiseMax(b).eval(); });
    eig::Array2u clamped_3f = clamp_size_by_setting(e_settings, maximal_3f);
    eig::Array2u clamped_1f = clamp_size_by_setting(e_settings, maximal_1f);
    eig::Array2f scaled_3f  = clamped_3f.cast<float>() / maximal_3f.cast<float>();
    eig::Array2f scaled_1f  = clamped_1f.cast<float>() / maximal_1f.cast<float>();

    // Scale input sizes by appropriate texture scaling
    for (auto &input : inputs_3f)
      input = (input.cast<float>() * scaled_3f).max(1.f).cast<uint>().eval();
    for (auto &input : inputs_1f)
      input = (input.cast<float>() * scaled_1f).max(1.f).cast<uint>().eval();

    // Rebuild texture atlases with mips
    atlas_3f = {{ .sizes   = inputs_3f, .levels = 1 + clamped_3f.log2().maxCoeff() }};
    atlas_1f = {{ .sizes   = inputs_1f, .levels = 1 + clamped_1f.log2().maxCoeff() }};
    
    // Process image data
    info.resize(e_images.size());
    for (uint i = 0; i < e_images.size(); ++i) {
      const auto &img = e_images[i].value();

      // Load reservation from appropriate atlas (3f or 1f)
      bool is_3f = img.pixel_frmt() == Image::PixelFormat::eRGB;
      auto size  = is_3f ? atlas_3f.size() 
                         : atlas_1f.size();
      auto resrv = is_3f ? atlas_3f.reservation(atlas_indices[i])
                         : atlas_1f.reservation(atlas_indices[i]);

      // Determine UV coordinates of the texture inside the full atlas
      eig::Array2f uv0 = resrv.offs.cast<float>() / size.head<2>().cast<float>(),
                   uv1 = resrv.size.cast<float>() / size.head<2>().cast<float>();

      // Fill in info object
      info[i] = { .is_3f = is_3f,      .layer = resrv.layer_i,
                  .offs  = resrv.offs, .size  = resrv.size,
                  .uv0   = uv0,        .uv1   = uv1 };

      // Get a resampled float representation of image data
      auto imgf = img.convert({ .resize_to  = resrv.size,
                                .pixel_type = Image::PixelType::eFloat,
                                .color_frmt = Image::ColorFormat::eLRGB });
      
      // Put image in appropriate place
      if (is_3f) {
        atlas_3f.texture().set(imgf.data<float>(), 0,
          { resrv.size.x(), resrv.size.y(), 1             },
          { resrv.offs.x(), resrv.offs.y(), resrv.layer_i });
      } else {
        atlas_1f.texture().set(imgf.data<float>(), 0,
          { resrv.size.x(), resrv.size.y(), 1             },
          { resrv.offs.x(), resrv.offs.y(), resrv.layer_i });
      }
    } // for (uint i)

    // Fill in mip data using OpenGL's base functions
    if (atlas_3f.texture().is_init()) atlas_3f.texture().generate_mipmaps();
    if (atlas_1f.texture().is_init()) atlas_1f.texture().generate_mipmaps();

    // Finally, push info objects
    info_gl = {{ .data = cnt_span<const std::byte>(info) }};
  }

  std::pair<
    std::vector<eig::Array4f>,
    std::vector<eig::Array4f>
  > pack(const Mesh &m) {
    std::vector<eig::Array4f> a(m.verts.size()), b(m.verts.size());
    
    #pragma omp parallel for
    for (int i = 0; i < a.size(); ++i) {
      a[i] = (eig::Array4f() << m.verts[i], m.has_txuvs() ? m.txuvs[i][0] : 0).finished();
      b[i] = (eig::Array4f() << m.norms[i], m.has_txuvs() ? m.txuvs[i][1] : 0).finished();
    }
    
    return { std::move(a), std::move(b) };
  }

  RTMeshData::RTMeshData(const Scene &scene) {
    met_trace();
    // Initialize on first run
    // update(scene);
  }

  bool RTMeshData::is_stale(const Scene &scene) const {
    met_trace();
    return scene.resources.meshes.is_mutated();
  }

  void RTMeshData::update(const Scene &scene) {
    met_trace_full();

    // Get external resources
    const auto &e_meshes = scene.resources.meshes;

    guard(!e_meshes.empty());

    // Generate a simplified representation of each scene mesh
    std::vector<Mesh> simplified(e_meshes.size());
    std::transform(std::execution::par_unseq, range_iter(e_meshes), simplified.begin(), [](const auto &m) { 
        Mesh copy = m.value();
        // simplify_mesh(copy, 128, 1e-2);
        // renormalize_mesh(copy);

        // fmt::print("Simplified mesh vert count: {} -> {}\n", m.value().verts.size(), copy.verts.size());
        // fmt::print("Simplified mesh elem count: {} -> {}\n", m.value().elems.size(), copy.elems.size());

        return copy;
    });

    /* // TODO remove
    {
      const auto &mesh = simplified[0]; //.value();
      auto bvh = detail::create_bvh({
        .mesh            = mesh,
        .n_node_children = 8, // 2, 4, 8
        .n_leaf_children = 8,
      });

      for (uint i = 0; i < bvh.nodes.size(); ++i) {
        const auto &node = bvh.nodes[i];
        fmt::print("{} - minb = {}, maxb = {}, type = {}, children = {}\n ", 
          i, node.minb, node.maxb, node.is_leaf() ? "leaf" : "node", node.data1);
      }
    } */

    // Gather vertex/element lengths and offsets per mesh resources
    std::vector<uint> verts_size, elems_size, verts_offs, elems_offs;
    std::transform(range_iter(simplified), std::back_inserter(verts_size), 
      [](const auto &m) { return static_cast<uint>(m.verts.size()); });
    std::exclusive_scan(range_iter(verts_size), std::back_inserter(verts_offs), 0u);
    std::transform(range_iter(simplified), std::back_inserter(elems_size), 
      [](const auto &m) { return static_cast<uint>(m.elems.size()); });
    std::exclusive_scan(range_iter(elems_size), std::back_inserter(elems_offs), 0u);

    // Total vertex/element lengths across all meshes
    uint n_verts = verts_size.at(verts_size.size() - 1) + verts_offs.at(verts_offs.size() - 1);
    uint n_elems = elems_size.at(elems_size.size() - 1) + elems_offs.at(elems_offs.size() - 1);

    // Holders for packed data of all meshes
    std::vector<MeshInfo>       packed_info(simplified.size());
    std::vector<eig::Array4f>   packed_verts_a(n_verts),
                                packed_verts_b(n_verts);
    std::vector<eig::Array3u>   packed_elems(n_elems);
    std::vector<eig::AlArray3u> packed_elems_al(n_elems);

    // Fill packed layout/data info object
    info.resize(simplified.size());
    #pragma omp parallel for
    for (int i = 0; i < simplified.size(); ++i) {
      const auto &mesh = simplified[i];
      auto [a, b] = pack(mesh);
      
      // Copy over packed data to the correctly offset range;
      // adjust element indices to refer to the offset range as well
      rng::copy(a,               packed_verts_a.begin()  + verts_offs[i]);
      rng::copy(b,               packed_verts_b.begin()  + verts_offs[i]);
      rng::transform(mesh.elems, packed_elems.begin()    + elems_offs[i], 
        [offs = verts_offs[i]](const auto &v) { return (v + offs).eval(); });
      rng::transform(mesh.elems, packed_elems_al.begin() + elems_offs[i], 
        [offs = verts_offs[i]](const auto &v) { return (v + offs).eval(); });

      info[i] = {
        .verts_offs = verts_offs[i], .verts_size = verts_size[i],
        .elems_offs = elems_offs[i], .elems_size = elems_size[i],
      };
    }

    // Push layout/data to GL buffers
    info_gl  = {{ .data = cnt_span<const std::byte>(packed_info)     }};
    verts_a  = {{ .data = cnt_span<const std::byte>(packed_verts_a)  }};
    verts_b  = {{ .data = cnt_span<const std::byte>(packed_verts_b)  }};
    elems    = {{ .data = cnt_span<const std::byte>(packed_elems)    }};
    elems_al = {{ .data = cnt_span<const std::byte>(packed_elems_al) }};
    
    // Define corresponding vertex array object
    array = {{
      .buffers = {{ .buffer = &verts_a, .index = 0, .stride = sizeof(eig::Array4f)    },
                  { .buffer = &verts_b, .index = 1, .stride = sizeof(eig::Array4f)    }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e4 },
                  { .attrib_index = 1, .buffer_index = 1, .size = gl::VertexAttribSize::e4 }},
      .elements = &elems
    }};
  }

  RTObjectData::RTObjectData(const Scene &scene) {
    met_trace_full();
    // Initialize on first run
    // update(scene);
  }

  bool RTObjectData::is_stale(const Scene &scene) const {
    met_trace();

    // Set to false, ensuring atlas only becomes stale after update()
    m_is_atlas_stale = false;

    // Get shared resources
    const auto &e_objects  = scene.components.objects;
    const auto &e_images   = scene.resources.images;
    const auto &e_settings = scene.components.settings;
    
    // Views over diffuse textures for atlas
    auto stale_images = e_objects
                      | vws::filter([ ](const auto &comp) { return comp.value.diffuse.index() == 1; })
                      | vws::filter([&](const auto &comp) { return e_images[std::get<1>(comp.value.diffuse)].is_mutated(); });

    // Accumulate reasons for returning
    return !atlas_bary.texture().is_init() 
        || !info_gl.is_init()
        || !stale_images.empty()
        || e_objects.is_mutated()
        || e_settings.state.texture_size;
  }
  
  void RTObjectData::update(const Scene &scene) {
      met_trace_full();
            
      // Get external resources
      const auto &e_objects  = scene.components.objects;
      const auto &e_images   = scene.resources.images;
      const auto &e_settings = scene.components.settings;

      guard(!e_objects.empty());

      // Initialize or resize object buffer to accomodate correct nr of objects
      bool handle_resize = false;
      if (!info_gl.is_init() || e_objects.size() != info.size()) {
        info.resize(e_objects.size());
        info_gl = {{ .size  = e_objects.size() * sizeof(ObjectInfo),
                     .flags = gl::BufferCreateFlags::eStorageDynamic }};

        handle_resize = true;
      }
      
      // View over all objects with textures whose underlying texture was changed
      auto stale_textures = e_objects
                          | vws::filter([ ](const auto &comp) { return comp.value.diffuse.index() == 1; })
                          | vws::filter([&](const auto &comp) { return e_images[std::get<1>(comp.value.diffuse)].is_mutated(); });

      // View over all objects with textures whose texture index was changed
      auto stale_indices = e_objects
                         | vws::filter([](const auto &comp) { return comp.value.diffuse.index() == 1; })
                         | vws::filter([](const auto &comp) { return comp.state.diffuse;              });

      // Initialize or rebuild barycentric atlas if necessary
      m_is_atlas_stale =  
           !stale_textures.empty()          // Rebuild if a referred-to texture was changed
        || !stale_indices.empty()           // Rebuild if a referral was changed
        || !atlas_bary.texture().is_init()  // Rebuild if the atlas does not yet exist
        || e_settings.state.texture_size    // Rebuild if texture size setting was changed
        || handle_resize;                   // Rebuild if nr. of objects necessitated resize
      
      if (m_is_atlas_stale) {
        // Gather necessary texture sizes, and set relevant indices of objects in atlas
        std::vector<eig::Array2u> inputs;
        for (uint i = 0; i < e_objects.size(); ++i) {
          const auto &[e_obj, e_obj_state] = e_objects[i];
          
          if (e_obj.diffuse.index()) {
            const auto &e_img = e_images[std::get<1>(e_obj.diffuse)].value();
            inputs.push_back(e_img.size());
          } else {
            // A small 4x4 patch to ensure sampling at its center always returns the right value
            inputs.push_back({ 4, 4 });
          }
        } // for (uint i)
        fmt::print("Atlas inputs: {}\n", inputs);

        // Determine maximum texture sizes, and scale input sizes w.r.t. to this value
        eig::Array2u maximal_4f = rng::fold_left(inputs, eig::Array2u(0), 
          [](auto a, auto b) { return a.cwiseMax(b).eval(); });
        eig::Array2u clamped_4f = clamp_size_by_setting(e_settings.value.texture_size, maximal_4f);
        eig::Array2f scaled_4f  = clamped_4f.cast<float>() / maximal_4f.cast<float>();
        for (auto &input : inputs)
          input = (input.cast<float>() * scaled_4f).max(1.f).cast<uint>().eval();

        // Rebuild texture atlas without mips
        atlas_bary = {{ .sizes = inputs, .levels = 1 + clamped_4f.log2().maxCoeff() }};

        fmt::print("Rebuilt atlas_bary\n");
        for (const TextureAtlasSpace &s : atlas_bary.reservations()) {
          fmt::print("\tlayer = {}, offs = {}, size = {}\n", s.layer_i, s.offs, s.size);
        }
      }

      // Process updates to gl-side object info
      for (uint i = 0; i < e_objects.size(); ++i) {
        const auto &component = e_objects[i];
        const auto &object    = component.value;
        
        guard_continue(handle_resize || m_is_atlas_stale || component.state.is_mutated());

        // Get relevant texture info
        bool is_albedo_sampled = object.diffuse.index() != 0;
        auto resrv = atlas_bary.reservation(i);
        
        info[i] = {
          .trf         = object.trf.matrix(),
          .trf_inv     = object.trf.inverse().matrix(),

          // Fill shape data
          .is_active   = object.is_active,
          .mesh_i      = object.mesh_i,
          .uplifting_i = object.uplifting_i,

          // Fill materials data
          .is_albedo_sampled = is_albedo_sampled,
          .albedo_i          = is_albedo_sampled ? std::get<1>(object.diffuse) : 0,
          .albedo_v          = is_albedo_sampled ? 0 : std::get<0>(object.diffuse),

          // Fill barycentric atlas data
          .layer = resrv.layer_i,
          .offs  = resrv.offs,
          .size  = resrv.size
        };

        // Note; should upgrade to mapped range
        info_gl.set(obj_span<const std::byte>(info[i]), sizeof(ObjectInfo), sizeof(ObjectInfo) * static_cast<size_t>(i));
      } // for (uint i)
      
      fmt::print("Updating objects\n");
  }

  RTUpliftingData::RTUpliftingData(const Scene &scene) {
    met_trace_full();

    // Fixed settings 
    constexpr uint max_upliftings = 8u;

    // Get external resources
    const auto &e_objects  = scene.components.objects;
    const auto &e_settings = scene.components.settings;
    const auto &e_images   = scene.resources.images;

    uint max_texture_layers = 
      std::min(gl::state::get_variable_int(gl::VariableName::eMaxArrayTextureLayers), 2048);
    uint layer_size = max_texture_layers / max_upliftings;

    // Initialize info buffer up to maximum nr of supported upliftings;
    // so no resizing will take place
    {
      // Hardcode up to the nr. of upliftings for now
      info.resize(max_upliftings, { 0, 0 });
      for (uint i = 0; i < max_upliftings; ++i) {
        info[i] = { .elem_offs = layer_size * i, .elem_size = layer_size };
      }

      info_gl = {{ .data  = cnt_span<const std::byte>(info), .flags = gl::BufferCreateFlags::eStorageDynamic }};
    }

    // Pre-allocated up to the maximum size necessary; as this  is actually a reasonable 2mb
    // so no resizing will take place
    {
      spectra_gl_texture = {{ .size  = { wavelength_samples, max_texture_layers } }};
      spectra_gl         = {{ .size  = max_texture_layers * sizeof(SpecPack), 
                              .flags = buffer_create_flags }};
      spectra_gl_mapping = spectra_gl.map_as<SpecPack>(buffer_access_flags);
    }
  }

  bool RTUpliftingData::is_stale(const Scene &scene) const {
    met_trace();
    return false;
  }

  void RTUpliftingData::update(const Scene &scene) {
    met_trace();
    // Never runs; is instead filled in by subsequent uplifting pipeline
  }

  RTObserverData::RTObserverData(const Scene &scene) {
    met_trace_full();
    
    constexpr static auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
    constexpr static auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;
    
    // Initialize buffers/textures for preallocated nr. of layers;
    // very unlikely to exceed this count
    uint n_layers =  std::min(gl::state::get_variable_int(gl::VariableName::eMaxArrayTextureLayers), 256);
    cmfs_gl_texture = {{ .size = { wavelength_samples, n_layers } }};
    cmfs_gl         = {{ .size = cmfs_gl_texture.size().prod() * sizeof(eig::Array3f), .flags = buffer_create_flags }};
    cmfs_gl_mapping = cmfs_gl.map_as<CMFS>(buffer_access_flags);
  }

  bool RTObserverData::is_stale(const Scene &scene) const {
    met_trace();
    return scene.resources.observers.is_mutated();
  }

  void RTObserverData::update(const Scene &scene) {
    met_trace_full();

    uint i = 0;
    for (const auto &[cmfs, state] : scene.resources.observers) {
      guard_continue(state);
      cmfs_gl_mapping[i] = cmfs.transpose().reshaped(wavelength_samples, 3).eval();
      i++;
    }
    cmfs_gl.flush();
    cmfs_gl_texture.set(cmfs_gl);
  }

  RTIlluminantData::RTIlluminantData(const Scene &scene) {
    met_trace_full();
    
    constexpr static auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
    constexpr static auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

    // Initialize buffers/textures for preallocated nr. of layers;
    // very unlikely to exceed this count
    uint n_layers =  std::min(gl::state::get_variable_int(gl::VariableName::eMaxArrayTextureLayers), 256);
    illm_gl_texture = {{ .size = { wavelength_samples, n_layers } }};
    illm_gl         = {{ .size = illm_gl_texture.size().prod() * sizeof(float), .flags = buffer_create_flags }};
    illm_gl_mapping = illm_gl.map_as<Spec>(buffer_access_flags);
  }

  bool RTIlluminantData::is_stale(const Scene &scene) const {
    met_trace();
    return scene.resources.illuminants.is_mutated();
  }

  void RTIlluminantData::update(const Scene &scene) {
    met_trace_full();

    uint i = 0;
    for (const auto &[illm, state] : scene.resources.illuminants) {
      guard_continue(state);
      illm_gl_mapping[i] = illm;
      i++;
    }
    illm_gl.flush();
    illm_gl_texture.set(illm_gl);
  }

  RTColorSystemData::RTColorSystemData(const Scene &scene) {
    met_trace_full();
    
    constexpr static auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
    constexpr static auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

    // Initialize buffers/textures for preallocated nr. of layers;
    // very unlikely to exceed this count
    uint n_layers =  std::min(gl::state::get_variable_int(gl::VariableName::eMaxArrayTextureLayers), 256);
    csys_gl_texture = {{ .size = { wavelength_samples, n_layers } }};
    csys_gl         = {{ .size = csys_gl_texture.size().prod() * sizeof(eig::Array3f), .flags = buffer_create_flags }};
    csys_gl_mapping = csys_gl.map_as<CMFS>(buffer_access_flags);
  }

  bool RTColorSystemData::is_stale(const Scene &scene) const {
    met_trace();
    return scene.resources.illuminants.is_mutated()
        || scene.resources.observers.is_mutated()
        || scene.components.colr_systems.is_mutated();
  }

  void RTColorSystemData::update(const Scene &scene) {
    met_trace_full();

    uint i = 0;
    for (const auto &[csys, state] : scene.components.colr_systems) {
      auto func = scene.get_csys(csys).finalize_direct();
      // Data is transposed and reshaped into a [wvls, 3]-shaped object for gpu-side layout
      csys_gl_mapping[i] = func.transpose().reshaped(wavelength_samples, 3);
      i++;
    }
    csys_gl.flush();
    csys_gl_texture.set(csys_gl);
  }
} // namespace met::detail