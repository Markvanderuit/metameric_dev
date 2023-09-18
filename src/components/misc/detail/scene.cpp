#include <metameric/components/misc/detail/scene.hpp>
#include <metameric/core/utility.hpp>
#include <algorithm>
#include <deque>
#include <execution>
#include <ranges>
#include <vector>

namespace met::detail {
  RTTextureData RTTextureData::realize(Settings::TextureSize texture_size, std::span<const detail::Resource<Image>> images) {
    met_trace_full();
    guard(!images.empty(), RTTextureData { });

    // Generate inputs for texture atlas
    std::vector<eig::Array2u> inputs_3f, inputs_1f;
    std::vector<uint>         inputs_i;
    for (uint i = 0; i < images.size(); ++i) {
      const auto &img = images[i].value();
      if (img.pixel_frmt() == Image::PixelFormat::eRGB) {
        inputs_i.push_back(inputs_3f.size());
        inputs_3f.push_back(img.size());
      } else {
        inputs_i.push_back(inputs_1f.size());
        inputs_1f.push_back(img.size());
      }
    } // for (uint i)

    // Determine maximum texture sizes, and restrict texture sizes to these values to keep things simple
    eig::Array2u maxm_3f = rng::fold_left(inputs_3f, eig::Array2u(0), [](auto a, auto b) { return a.cwiseMax(b).eval(); });
    eig::Array2u maxm_1f = rng::fold_left(inputs_1f, eig::Array2u(0), [](auto a, auto b) { return a.cwiseMax(b).eval(); });
    switch (texture_size) {
      case Settings::TextureSize::eHigh:
        maxm_3f = maxm_3f.cwiseMin(2048u); maxm_1f = maxm_1f.cwiseMin(2048u); break;
      case Settings::TextureSize::eMed:
        maxm_3f = maxm_3f.cwiseMin(1024u); maxm_1f = maxm_1f.cwiseMin(1024u); break;
      case Settings::TextureSize::eLow:
        maxm_3f = maxm_3f.cwiseMin(512u); maxm_1f = maxm_1f.cwiseMin(512u); break;
    }
    for (auto &input : inputs_3f) input = maxm_3f;
    for (auto &input : inputs_1f) input = maxm_1f;

    // Generate data objects and initialize atlases
    RTTextureData data = { .info          = std::vector<RTTextureInfo>(images.size()),
                           .atlas_indices = inputs_i,
                           .atlas_3f      = {{ .sizes   = inputs_3f, .levels  = 1 + maxm_3f.log2().maxCoeff() }},
                           .atlas_1f      = {{ .sizes   = inputs_1f, .levels  = 1 + maxm_1f.log2().maxCoeff() }}};
    for (uint i = 0; i < inputs_i.size(); ++i) {
      const auto &img = images[i].value();

      bool is_3f = img.pixel_frmt() == Image::PixelFormat::eRGB;
      auto size  = is_3f ? data.atlas_3f.size() : data.atlas_1f.size();
      auto resrv = is_3f ? data.atlas_3f.reservation(inputs_i[i])
                         : data.atlas_1f.reservation(inputs_i[i]);

      // Determine UV coordinates of the texture inside the full atlas
      eig::Array2f uv0 = resrv.offs.cast<float>() / size.head<2>().cast<float>();
      eig::Array2f uv1 = resrv.size.cast<float>() / size.head<2>().cast<float>();

      // Fill in info object
      data.info[i] = { .is_3f = is_3f,
                       .layer = resrv.layer_i,
                       .offs  = resrv.offs,
                       .size  = resrv.size,
                       .uv0   = uv0,
                       .uv1   = uv1 };
                      
      // Get a float representation of image data, and push it to GL-side
      auto imgf = img.convert({ .resize_to  = is_3f ? maxm_3f : maxm_1f,
                                .pixel_type = Image::PixelType::eFloat,
                                .color_frmt = Image::ColorFormat::eLRGB });
      if (is_3f) {
        data.atlas_3f.texture().set(imgf.data<float>(), 0,
          { resrv.size.x(), resrv.size.y(), 1             },
          { resrv.offs.x(), resrv.offs.y(), resrv.layer_i });
      } else {
        data.atlas_1f.texture().set(imgf.data<float>(), 0,
          { resrv.size.x(), resrv.size.y(), 1             },
          { resrv.offs.x(), resrv.offs.y(), resrv.layer_i });
      }
    } // for (uint i)

    // Fill in mip data using OpenGL's base functions
    data.atlas_3f.texture().generate_mipmaps();
    data.atlas_1f.texture().generate_mipmaps();

    // Finally, push info objects
    data.info_gl = {{ .data = cnt_span<const std::byte>(data.info) }};

    return data;
  }

  void RTTextureData::update(std::span<const detail::Resource<Image>>) {
    met_trace();
    bool handle_resize = false;

    


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

  RTObjectData RTObjectData::realize(std::span<const detail::Component<Scene::Object>> objects) {
    met_trace_full();

    RTObjectData data;
    guard(!objects.empty(), data);
    
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
      met_trace();
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