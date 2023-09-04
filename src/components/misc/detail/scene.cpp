#include <metameric/components/misc/detail/scene.hpp>
#include <metameric/core/utility.hpp>

namespace met::detail {
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
    }};
    
    std::unique_ptr<gl::AbstractTexture> texture;
    switch (image.frmt()) {
      case DynamicImage::PixelFormat::eAlpha:
        texture = std::make_unique<gl::Texture2d1f>(gl::Texture2d1f::InfoType { 
          .size = image.size(), 
          .data = image.data<float>() 
        });
        break;
      case DynamicImage::PixelFormat::eRGB:
        texture = std::make_unique<gl::Texture2d3f>(gl::Texture2d3f::InfoType { 
          .size = image.size(), 
          .data = image.data<float>() 
        });
        break;
      case DynamicImage::PixelFormat::eRGBA:
        texture = std::make_unique<gl::Texture2d4f>(gl::Texture2d4f::InfoType { 
          .size = image.size(), 
          .data = image.data<float>() 
        });
        break;
    }
    
    return { .texture = std::move(texture), .sampler = std::move(sampler) };
  }
} // namespace met::detail