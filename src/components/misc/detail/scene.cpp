#include <metameric/components/misc/detail/scene.hpp>
#include <metameric/core/utility.hpp>
#include <algorithm>
#include <deque>
#include <execution>
#include <ranges>
#include <vector>

namespace met::detail {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  eig::Array2u clamp_size_by_setting(Settings::TextureSize setting, eig::Array2u size) {
    switch (setting) {
      case Settings::TextureSize::eHigh: return size.cwiseMin(2048u);
      case Settings::TextureSize::eMed:  return size.cwiseMin(1024u);
      case Settings::TextureSize::eLow:  return size.cwiseMin(512u);
      default:                           return size;
    }
  }

  RTTextureData RTTextureData::realize(const Scene &scene) {
    met_trace_full();
    RTTextureData data;
    data.update(scene);
    return data;
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

    // Set atlas object indices to all inf
    atlas_indices.resize(e_images.size());
    rng::fill(atlas_indices, std::numeric_limits<uint>::max());

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
  > pack(const AlMesh &m) {
    std::vector<eig::Array4f> a(m.verts.size()),
                              b(m.verts.size());

    #pragma omp parallel for
    for (int i = 0; i < a.size(); ++i) {
      a[i] = (eig::Array4f() << m.verts[i], m.txuvs[i][0]).finished();
      b[i] = (eig::Array4f() << m.norms[i], m.txuvs[i][1]).finished();
    }
    
    return { std::move(a), std::move(b) };
  }

  RTMeshData RTMeshData::realize(std::span<const detail::Resource<AlMesh>> meshes) {
    met_trace_full();

    guard(!meshes.empty(), RTMeshData { });

    // Gather vertex/element lengths and offsets per mesh resources
    std::vector<uint> verts_size, elems_size, verts_offs, elems_offs;
    std::transform(range_iter(meshes), std::back_inserter(verts_size), 
      [](const auto &m) { return static_cast<uint>(m.value().verts.size()); });
    std::exclusive_scan(range_iter(verts_size), std::back_inserter(verts_offs), 0u);
    std::transform(range_iter(meshes), std::back_inserter(elems_size), 
      [](const auto &m) { return static_cast<uint>(m.value().elems.size()); });
    std::exclusive_scan(range_iter(elems_size), std::back_inserter(elems_offs), 0u);

    // Total vertex/element lengths across all meshes
    uint n_verts = verts_size.at(verts_size.size() - 1) + verts_offs.at(verts_offs.size() - 1);
    uint n_elems = elems_size.at(elems_size.size() - 1) + elems_offs.at(elems_offs.size() - 1);

    // Holders for packed data of all meshes
    std::vector<detail::RTMeshInfo> packed_info(meshes.size());
    std::vector<eig::Array4f>       packed_verts_a(n_verts),
                                    packed_verts_b(n_verts);
    std::vector<eig::Array3u>       packed_elems(n_elems);
    std::vector<eig::AlArray3u>     packed_elems_al(n_elems);

    // Fill packed layout/data vectors
    #pragma omp parallel for
    for (int i = 0; i < meshes.size(); ++i) {
      const auto &mesh = meshes[i].value();
      auto [a, b] = pack(mesh);
      
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
    RTMeshData data = {
      .info     = packed_info,
      .info_gl  = {{ .data = cnt_span<const std::byte>(packed_info)     }},
      .verts_a  = {{ .data = cnt_span<const std::byte>(packed_verts_a)  }},
      .verts_b  = {{ .data = cnt_span<const std::byte>(packed_verts_b)  }},
      .elems    = {{ .data = cnt_span<const std::byte>(packed_elems)    }},
      .elems_al = {{ .data = cnt_span<const std::byte>(packed_elems_al) }},
    };
    
    // Define corresponding vertex array object
    data.array = {{
      .buffers = {{ .buffer = &data.verts_a, .index = 0, .stride = sizeof(eig::Array4f)    },
                  { .buffer = &data.verts_b, .index = 1, .stride = sizeof(eig::Array4f)    }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e4 },
                  { .attrib_index = 1, .buffer_index = 1, .size = gl::VertexAttribSize::e4 }},
      .elements = &data.elems
    }};

    return data;
  }

  RTObjectData RTObjectData::realize(const Scene &scene) {
    met_trace_full();

    // Get external resources
    const auto e_objects = scene.components.objects;
    
    RTObjectData data;
    guard(!e_objects.empty(), data);
    
    // Build object info data
    {
      data.info.resize(e_objects.size());
      for (uint i = 0; i < e_objects.size(); ++i) {
        const auto &component = e_objects[i];
        const auto &object    = component.value;

        bool is_albedo_sampled    = object.diffuse.index() != 0;
        // bool is_roughness_sampled = object.roughness.index() != 0;
        // bool is_metallic_sampled  = object.metallic.index() != 0;
        // bool is_normals_sampled   = object.normals.index() != 0;

        data.info[i] = {
          .trf         = object.trf.matrix(),
          .trf_inv     = object.trf.inverse().matrix(),

          // Fill shape data
          .is_active   = object.is_active,
          .mesh_i      = object.mesh_i,
          .uplifting_i = object.uplifting_i,

          // Fill materials data
          .is_albedo_sampled = is_albedo_sampled,
          .albedo_i          = is_albedo_sampled ? std::get<1>(object.diffuse) : 0,
          .albedo_v          = is_albedo_sampled ? 0 : std::get<0>(object.diffuse)
        };
      }

      // Push to GL-side
      data.info_gl = {{ .data  = cnt_span<const std::byte>(data.info),
                        .flags = gl::BufferCreateFlags::eStorageDynamic }};
    }

    // Build barycentric data atlas
    {

    }

    return data;
  }

  bool RTObjectData::is_stale(const Scene &scene) const {
    met_trace();
    return scene.components.objects.is_mutated();
  }

  void RTObjectData::update(const Scene &scene) {
      met_trace_full();
      
      // Get external resources
      const auto e_objects = scene.components.objects;

      bool handle_resize = false;

      // Initialize or resize object buffer to accomodate
      if (!info_gl.is_init() || e_objects.size() != info.size()) {
        info.resize(e_objects.size());
        info_gl = {{ .size  = e_objects.size() * sizeof(detail::RTObjectInfo),
                     .flags = gl::BufferCreateFlags::eStorageDynamic         }};

        handle_resize = true;
      }
      
      // Process updates to gl-side object info
      for (uint i = 0; i < e_objects.size(); ++i) {
        const auto &component = e_objects[i];
        const auto &object    = component.value;
        
        guard_continue(handle_resize || component.state.is_mutated());

        bool is_albedo_sampled = object.diffuse.index() != 0;

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
          .albedo_v          = is_albedo_sampled ? 0 : std::get<0>(object.diffuse)
        };

        info_gl.set(obj_span<const std::byte>(info[i]),
                             sizeof(detail::RTObjectInfo),
                             sizeof(detail::RTObjectInfo) * static_cast<size_t>(i));
      } // for (uint i)
  }

  RTUpliftingData RTUpliftingData::realize(const Scene &scene) {
    met_trace_full();

    // Fixed settings 
    constexpr uint max_upliftings = 8u;

    // Get external resources
    const auto &e_objects  = scene.components.objects;
    const auto &e_settings = scene.components.settings;
    const auto &e_images   = scene.resources.images;

    RTUpliftingData data;

    uint max_texture_layers = 
      std::min(gl::state::get_variable_int(gl::VariableName::eMaxArrayTextureLayers), 2048);

    // Initialize info buffer up to maximum nr of supported upliftings;
    // so no resizing will take place
    {
      data.spectra_info.resize(max_upliftings, { 0, 0 });
      data.spectra_info_gl = {{ .data  = cnt_span<const std::byte>(data.spectra_info), 
                                .flags = buffer_create_flags }};
      data.spectra_info_gl_mapping = data.spectra_info_gl.map_as<RTUpliftingInfo>(buffer_access_flags);
    }

    // Pre-allocated up to the maximum size necessary; as this  is actually a reasonable 2mb
    // so no resizing will take place
    {
      data.spectra_elem_gl_texture = {{ .size  = { wavelength_samples, max_texture_layers } }};
      data.spectra_elem_gl         = {{ .size  = max_texture_layers * sizeof(RTUpliftingData::ElemSpec), 
                                        .flags = buffer_create_flags }};
      data.spectra_elem_gl_mapping = data.spectra_elem_gl.map_as<Spec>(buffer_access_flags);
    }

    data.update(scene);
    return data;
  }

  bool RTUpliftingData::is_stale(const Scene &scene) const {
    met_trace();

    // Get shared resources
    const auto &e_objects  = scene.components.objects;
    const auto &e_settings = scene.components.settings;

    // Views over objects; objects with diffuse textures, objects which underwent a state change
    auto stale_objects = e_objects  
                       | vws::filter([](const auto &comp) { return comp.value.diffuse.index() == 1; }) 
                       | vws::filter([](const auto &comp) { return comp.state.diffuse;              });
                       
    // Accumulate reasons for returning
    return !atlas_4f.texture().is_init()
        || e_objects.is_resized()
        || e_settings.state.texture_size
        || !stale_objects.empty();
  }

  void RTUpliftingData::update(const Scene &scene) {
    met_trace_full();

    // Get shared resources
    const auto &e_settings = scene.components.settings.value.texture_size;
    const auto &e_objects  = scene.components.objects;
    const auto &e_images   = scene.resources.images;

    // Set atlas object indices to all inf
    atlas_indices.resize(e_objects.size());
    rng::fill(atlas_indices, std::numeric_limits<uint>::max());

    // Gather necessary texture sizes, and set relevant indices of objects in atlas
    std::vector<eig::Array2u> inputs;
    for (uint i = 0; i < e_objects.size(); ++i) {
      const auto &[e_obj, e_obj_state] = e_objects[i];

      // Object diffuse references image? Get it
      guard_continue(e_obj.diffuse.index());
      const auto &[e_img, e_img_state] = e_images[std::get<1>(e_obj.diffuse)];

      atlas_indices[i] = inputs.size();
      inputs.push_back(e_img.size());
    } // for (uint i)

    // Determine maximum texture sizes, and scale input sizes w.r.t. to this value
    eig::Array2u maximal_4f = rng::fold_left(inputs, eig::Array2u(0), 
      [](auto a, auto b) { return a.cwiseMax(b).eval(); });
    eig::Array2u clamped_4f = clamp_size_by_setting(e_settings, maximal_4f);
    eig::Array2f scaled_4f  = clamped_4f.cast<float>() / maximal_4f.cast<float>();
    for (auto &input : inputs)
      input = (input.cast<float>() * scaled_4f).max(1.f).cast<uint>().eval();

    // Rebuild texture atlas with mips
    atlas_4f = {{ .sizes = inputs, .levels = 1 + clamped_4f.log2().maxCoeff() }};
  }
} // namespace met::detail