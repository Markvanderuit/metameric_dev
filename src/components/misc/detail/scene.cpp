#include <metameric/components/misc/detail/scene.hpp>
#include <metameric/core/utility.hpp>
#include <algorithm>
#include <deque>
#include <execution>
#include <ranges>
#include <vector>

namespace met::detail {
  constexpr uint texture_atlas_padding    = 16u;
  constexpr uint texture_atlas_padding_2x = 2u * texture_atlas_padding;
  constexpr auto texture_atlas_widths     = { 16384u + texture_atlas_padding_2x, 
                                              12228u + texture_atlas_padding_2x, 
                                              8192u  + texture_atlas_padding_2x };
  
  using TextureAtlasFormat = DynamicImage::PixelFormat;

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
    return ((img.size + texture_atlas_padding_2x) <= space.size).all();
  };
  constexpr auto atlas_split = [](AtlasCreateInfo::Image img, AtlasSpace space) 
    -> std::pair<AtlasSpace, std::vector<AtlasSpace>> {
    AtlasSpace result = { .offs  = space.offs + texture_atlas_padding,
                                 .size  = img.size,
                                 .layer = space.layer };

    std::vector<AtlasSpace> remainder;
    if (uint remainder_x = space.size.x() - texture_atlas_padding_2x - img.size.x(); remainder_x > 0)
      remainder.push_back({ .offs  = space.offs + eig::Array2u(texture_atlas_padding_2x + img.size.x(), 0),
                            .size  = { space.size.x() - texture_atlas_padding_2x - img.size.x(), 
                                       img.size.x() + texture_atlas_padding_2x },
                            .layer = space.layer });
    if (uint remainder_y = space.size.y() - texture_atlas_padding_2x - img.size.y(); remainder_y > 0)
      remainder.push_back({ .offs  = space.offs + eig::Array2u(0, texture_atlas_padding_2x + img.size.y()),
                            .size  = { space.size.x(), 
                                       space.size.y() - img.size.y() - texture_atlas_padding_2x },
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
    std::vector<Atlas> candidates(texture_atlas_widths.size());
    std::transform(std::execution::par_unseq, range_iter(texture_atlas_widths), candidates.begin(), [&](uint w) { 
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

      // Fill in info object
      data.info[work.image_i] = { .is_3f = true,
                                  .layer = space.layer,
                                  .offs  = space.offs,
                                  .size  = space.size  };

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
      
      // Fill in info object
      data.info[work.image_i] = { .is_3f = false,
                                  .layer = space.layer,
                                  .offs  = space.offs,
                                  .size  = space.size  };

      // Get a float representation of image data, and push to GL-side
      auto img = images[work.image_i].value().convert({ .pixel_type = DynamicImage::PixelType::eFloat });
      data.atlas_1f.set(img.data<float>(), 
                        0,
                        { space.size.x(), space.size.y(), 1           },
                        { space.offs.x(), space.offs.y(), space.layer });
    } // for (uint i)

    // Finally, push info objects
    data.info_gl = {{ .data = cnt_span<const std::byte>(data.info) }};

    return data;
  }

  RTMeshData RTMeshData::realize(std::span<const AlMeshData> meshes) {
    met_trace_full();

    RTMeshData data;

    return data;
  }
  
  MeshLayout MeshLayout::realize(const AlMeshData &mesh) {
    met_trace_full();
    
    // Initialize mesh buffers
    gl::Buffer verts = {{ .data = cnt_span<const std::byte>(mesh.verts) }};
    gl::Buffer norms = {{ .data = cnt_span<const std::byte>(mesh.norms) }};
    gl::Buffer texuv = {{ .data = cnt_span<const std::byte>(mesh.uvs)   }};
    gl::Buffer elems = {{ .data = cnt_span<const std::byte>(mesh.elems) }};

    // Initialize corresponding VAO
    gl::Array array = {{
      .buffers  = {{ .buffer = &verts, .index = 0, .stride = sizeof(AlMeshData::VertTy) },
                   { .buffer = &norms, .index = 1, .stride = sizeof(AlMeshData::VertTy) },
                   { .buffer = &texuv, .index = 2, .stride = sizeof(AlMeshData::UVTy)   }},
      .attribs  = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 },
                   { .attrib_index = 1, .buffer_index = 1, .size = gl::VertexAttribSize::e3 },
                   { .attrib_index = 2, .buffer_index = 2, .size = gl::VertexAttribSize::e2 }},
      .elements = &elems
    }};

    // Return structured object
    return detail::MeshLayout {
      .verts = std::move(verts),
      .norms = std::move(norms),
      .texuv = std::move(texuv),
      .elems = std::move(elems),
      .array = std::move(array),
    };
  }

  TextureLayout TextureLayout::realize(const DynamicImage &image_) {
    met_trace_full();
    
    // Convert to full float representation
    DynamicImage image = image_.convert({ .pixel_type = DynamicImage::PixelType::eFloat });

    // TODO; extract sampler initialization to "somewhere"
    gl::Sampler sampler = {{ 
      .min_filter = gl::SamplerMinFilter::eLinear,
      .mag_filter = gl::SamplerMagFilter::eLinear,
      .wrap       = gl::SamplerWrap::eRepeat
    }};
    
    std::unique_ptr<gl::AbstractTexture> texture;
    switch (image.frmt()) {
      case TextureAtlasFormat::eAlpha:
        texture = std::make_unique<gl::Texture2d1f>(gl::Texture2d1f::InfoType { 
          .size = image.size(), 
          .data = image.data<float>() 
        });
        break;
      case TextureAtlasFormat::eRGB:
        texture = std::make_unique<gl::Texture2d3f>(gl::Texture2d3f::InfoType { 
          .size = image.size(), 
          .data = image.data<float>() 
        });
        break;
      case TextureAtlasFormat::eRGBA:
        texture = std::make_unique<gl::Texture2d4f>(gl::Texture2d4f::InfoType { 
          .size = image.size(), 
          .data = image.data<float>() 
        });
        break;
    }
    
    return { .texture = std::move(texture), .sampler = std::move(sampler) };
  }
} // namespace met::detail