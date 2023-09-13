#include <metameric/components/misc/detail/scene.hpp>
#include <metameric/core/utility.hpp>
#include <algorithm>
#include <deque>
#include <execution>
#include <ranges>
#include <vector>

namespace met::detail {
  constexpr uint atlas_padding    = 16u;
  constexpr uint atlas_padding_2x = 2u * atlas_padding;
  constexpr auto atlas_widths     = { 16384u + atlas_padding_2x, 
                                      12228u + atlas_padding_2x, 
                                      8192u  + atlas_padding_2x };
  
  struct AtlasSpace {
    eig::Array2u offs, size; 
    uint layer;
  };

  struct Atlas {
    eig::Array3u            size; // 3rd component accounts for texture arrays
    std::vector<AtlasSpace> data;
  };

  struct AtlasCreateInfo {
    struct Image {
      uint image_i, work_i;
      eig::Array2u size;
    };
    
    eig::Array2u       size; // Initial size to use for atlas generation
    std::vector<Image> data; // Input image references to build over
  };

  /* Texture atlas helper functions */

  constexpr auto atlas_area = [](auto space) -> uint { return space.size.prod(); };
  constexpr auto atlas_maxm = [](auto space) -> eig::Array2u { return (space.offs + space.size).eval(); };
  constexpr auto atlas_test = [](AtlasCreateInfo::Image img, AtlasSpace space) -> bool {
    return ((img.size + atlas_padding_2x) <= space.size).all();
  };
  constexpr auto atlas_split = [](AtlasCreateInfo::Image img, AtlasSpace space) 
    -> std::pair<AtlasSpace, std::vector<AtlasSpace>> {
    AtlasSpace result = { .offs  = space.offs + atlas_padding,
                                 .size  = img.size,
                                 .layer = space.layer };

    std::vector<AtlasSpace> remainder;
    if (uint remainder_x = space.size.x() - atlas_padding_2x - img.size.x(); remainder_x > 0)
      remainder.push_back({ .offs  = space.offs + eig::Array2u(atlas_padding_2x + img.size.x(), 0),
                            .size  = { space.size.x() - atlas_padding_2x - img.size.x(), 
                                       img.size.x() + atlas_padding_2x },
                            .layer = space.layer });
    if (uint remainder_y = space.size.y() - atlas_padding_2x - img.size.y(); remainder_y > 0)
      remainder.push_back({ .offs  = space.offs + eig::Array2u(0, atlas_padding_2x + img.size.y()),
                            .size  = { space.size.x(), 
                                       space.size.y() - img.size.y() - atlas_padding_2x },
                            .layer = space.layer });

    return { result, remainder };
  };

 Atlas generate_atlas(AtlasCreateInfo info) {
    met_trace();
  
    // Current nr. of layers in use for the different image formats
    uint layer_count = 1;

    // Space vectors; we start with (uninitialized) reserved space for all images, and empty space for all 
    // formats and at a maximum size, which will be shrunk later
    std::vector<AtlasSpace> reserved_spaces(info.data.size());
    std::vector<AtlasSpace> empty_spaces = {{ .offs = 0, .size = info.size, .layer = 0 }};

    // Process a work queue over the generated work, sorted by decreasing image area
    rng::sort(info.data, rng::greater {}, atlas_area);
    std::deque<AtlasCreateInfo::Image> work_queue(range_iter(info.data));
    while (!work_queue.empty()) {
      auto &work = work_queue.front();

      // Generate a view over empty spaces where the image would, potentially, fit;
      // this incorporates padding
      auto available_space = vws::filter(empty_spaces, [work](auto s) { return atlas_test(work, s); });

      // If no space is available, we add a layer and restart
      if (available_space.empty()) {
        // Add a new layer for the required image type
        empty_spaces.push_back({ .offs  = 0, 
                                 .size  = info.size, 
                                 .layer = layer_count++ });
        continue;
      }

      // Find the smallest available space for current work
      auto smallest_space_it = rng::min_element(available_space, {}, atlas_area);
      
      // Part of smallest space is reserved for the current image work
      // Split the smallest space; part is reserved for the current image, while
      // part is made available as empty space. The original is removed
      auto [reserved, remainder] = atlas_split(work, *smallest_space_it);
      empty_spaces.erase(smallest_space_it.base());
      reserved_spaces[work.work_i] = reserved;
      empty_spaces.insert(empty_spaces.end(), range_iter(remainder));

      // Work for this image is removed from the queue, as space has been found
      work_queue.pop_front();
    } // while (work_queue)
    
    auto maxm = rng::fold_left(reserved_spaces | vws::transform(atlas_maxm), eig::Array2u(0), 
      [](auto a, auto b) { return a.cwiseMax(b).eval(); });

    return Atlas { .size = { maxm.x(), maxm.y(), layer_count },
                   .data = reserved_spaces };
  }

  float texture_atlas_metric_ratio(const Atlas &atlas) {
    met_trace();
    return static_cast<float>(atlas.size.minCoeff()) 
         / static_cast<float>(atlas.size.maxCoeff());
  }

  float texture_atlas_metric_area(const Atlas &atlas) {
    met_trace();
    uint full_area = atlas.size.prod();
    uint used_area = rng::fold_left(atlas.data | vws::transform(atlas_area), 0u, std::plus {});
    return static_cast<float>(used_area) / static_cast<float>(full_area);
  }

  Atlas generate_texture_atlas(const std::vector<AtlasCreateInfo::Image> &data, auto metric) {
    met_trace();

    // Use a clamped maximum width; up to GL_MAX_TEXTURE_SIZE
    uint max_width = static_cast<uint>(gl::state::get_variable_int(gl::VariableName::eMaxTextureSize));
    
    // Generate a set of candidate atlases across several threads
    std::vector<Atlas> candidates(atlas_widths.size());
    std::transform(std::execution::par_unseq, range_iter(atlas_widths), candidates.begin(), [&](uint w) { 
      return generate_atlas({ .size = { std::min(w, max_width), std::numeric_limits<uint>::max() }, .data = data });  
    });
    
    // Determine best the available atlas based on the provided metric and return
    return *rng::max_element(candidates, rng::less {}, metric);
  }

  RTTextureData RTTextureData::realize(std::span<const detail::Resource<DynamicImage>> images) {
    met_trace_full();

    // Generate work objects for each image and image type, before atlas generation
    std::vector<AtlasCreateInfo::Image> work_3f, work_1f;
    for (uint i = 0; i < images.size(); ++i) {
      const auto &img = images[i].value();
      if (img.frmt() == DynamicImage::PixelFormat::eRGB) {
        work_3f.push_back({ .image_i = i, .work_i = (uint) work_3f.size(), .size = img.size() });
      } else {
        work_1f.push_back({ .image_i = i, .work_i = (uint) work_1f.size(), .size = img.size() });
      }
    }

    // Generate texture atlases for 3- and 1-component textures, go for best area usage
    auto atlas_3f = generate_texture_atlas(work_3f, texture_atlas_metric_ratio);
    auto atlas_1f = generate_texture_atlas(work_1f, texture_atlas_metric_ratio);

    // Next, now that we know the atlas layout, we can allocate texture arrays
    RTTextureData data = { .info = std::vector<RTTextureInfo>(images.size()),
                           .atlas_3f = {{ .size = atlas_3f.size }},
                           .atlas_1f = {{ .size = atlas_1f.size }} };

    // Next we push image data to their respective atlases
    for (uint i = 0; i < work_3f.size(); ++i) {
      auto work  = work_3f[i];      
      auto space = atlas_3f.data[i];

      eig::Array2f wh  = atlas_3f.size.head<2>().cast<float>();
      eig::Array2f uv0 = space.offs.cast<float>() / wh;
      eig::Array2f uv1 = space.size.cast<float>() / wh;

      // Fill in info object
      data.info[work.image_i] = { .is_3f = true,
                                  .layer = space.layer,
                                  .offs  = space.offs,
                                  .size  = space.size,
                                  .uv0   = uv0,
                                  .uv1   = uv1 };

      // Get a float representation of image data, and push to GL-side
      auto img = images[work.image_i].value().convert({ .pixel_type = DynamicImage::PixelType::eFloat });
      data.atlas_3f.set(img.data<float>(), 
                        0,
                        { space.size.x(), space.size.y(), 1           },
                        { space.offs.x(), space.offs.y(), space.layer });
    } // for (uint i)

    // .. continued 
    for (uint i = 0; i < work_1f.size(); ++i) {
      auto work  = work_1f[i];      
      auto space = atlas_1f.data[i];

      eig::Array2f wh  = atlas_3f.size.head<2>().cast<float>();
      eig::Array2f uv0 = space.offs.cast<float>() / wh;
      eig::Array2f uv1 = space.size.cast<float>() / wh;
      
      // Fill in info object
      data.info[work.image_i] = { .is_3f = false,
                                  .layer = space.layer,
                                  .offs  = space.offs,
                                  .size  = space.size,
                                  .uv0   = uv0,
                                  .uv1   = uv1 };

      // Get a float representation of image data, and push to GL-side
      auto img = images[work.image_i].value().convert({ .pixel_type = DynamicImage::PixelType::eFloat });
      data.atlas_1f.set(img.data<float>(), 
                        0,
                        { space.size.x(), space.size.y(), 1           },
                        { space.offs.x(), space.offs.y(), space.layer });
    } // for (uint i)

    // Generate texture views for each atlas array layer
    data.views_3f.clear();
    for (uint i = 0; i < atlas_3f.size.z(); ++i)
      data.views_3f.push_back({{ .texture = &data.atlas_3f, .min_layer = i }});
    data.views_1f.clear();
    for (uint i = 0; i < atlas_1f.size.z(); ++i)
      data.views_1f.push_back({{ .texture = &data.atlas_1f, .min_layer = i }});

    // Finally, push info objects
    data.info_gl = {{ .data = cnt_span<const std::byte>(data.info) }};

    return data;
  }

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

  RTMeshData RTMeshData::realize(std::span<const detail::Resource<AlMeshData>> meshes) {
    met_trace_full();

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

  RTObjectData RTObjectData::realize(std::span<const detail::Component<Scene::Object>> objects) {
    met_trace_full();

    RTObjectData data;
    
    data.info.resize(objects.size());
    for (uint i = 0; i < objects.size(); ++i) {
      const auto &component = objects[i];
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

    data.info_gl = {{ .data  = cnt_span<const std::byte>(data.info),
                      .flags = gl::BufferCreateFlags::eStorageDynamic }};

    return data;
  }

  void RTObjectData::update(std::span<const detail::Component<Scene::Object>> objects) {
      bool handle_resize = false;

      // Initialize or resize object buffer to accomodate
      if (!info_gl.is_init() || objects.size() != info.size()) {
        info.resize(objects.size());
        info_gl = {{ .size  = objects.size() * sizeof(detail::RTObjectInfo),
                     .flags = gl::BufferCreateFlags::eStorageDynamic         }};

        handle_resize = true;
      }
      
      // Process updates to gl-side object info
      for (uint i = 0; i < objects.size(); ++i) {
        const auto &component = objects[i];
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
  
} // namespace met::detail