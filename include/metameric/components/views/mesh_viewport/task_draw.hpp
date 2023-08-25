#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/data.hpp>
#include <metameric/core/state.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>

namespace met {
  class MeshViewportDrawTask : public detail::TaskNode {
    struct UnifLayout {
      alignas(64) eig::Matrix4f camera_matrix;
      /* alignas(64) eig::Matrix4f model_matrix; */
    };

    UnifLayout     *m_unif_buffer_map;
    gl::Buffer      m_unif_buffer;
    gl::Buffer      m_vpos_buffer;
    gl::Buffer      m_vnor_buffer;
    gl::Buffer      m_vuvs_buffer;
    gl::Buffer      m_elem_buffer;
    gl::Array       m_array;
    gl::Program     m_program;
    gl::DrawInfo    m_draw;
    gl::Sampler     m_sampler;
    gl::Texture2d3f m_texture;
    
    
  public:
    bool is_active(SchedulerHandle &info) override {
      met_trace();

      // Get external resources
      const auto &e_appl_data = info.global("appl_data").read_only<ApplicationData>();

      return !e_appl_data.loaded_mesh.verts.empty() && m_array.is_init(); // TODO only if viewport active and mesh data present
    }

    void init(SchedulerHandle &info) override {
      met_trace_full();

      // Get external resources
      const auto &e_appl_data = info.global("appl_data").read_only<ApplicationData>();
      const auto &e_mesh      = e_appl_data.loaded_mesh;
      const auto &e_texture   = e_appl_data.loaded_texture;

      // TODO: more robust data push system in eval()
      guard(!e_mesh.verts.empty());

      // Initialize program object
      m_program = {{ .type       = gl::ShaderType::eVertex,
                     .spirv_path = "resources/shaders/views/draw_mesh.vert.spv",
                     .cross_path = "resources/shaders/views/draw_mesh.vert.json" },
                   { .type       = gl::ShaderType::eFragment,
                     .spirv_path = "resources/shaders/views/draw_mesh.frag.spv",
                     .cross_path = "resources/shaders/views/draw_mesh.frag.json" }};

      // Initialize uniform buffer and corresponding mappings
      constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
      constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;
      m_unif_buffer     = {{ .size = sizeof(UnifLayout), .flags = buffer_create_flags }};
      m_unif_buffer_map = m_unif_buffer.map_as<UnifLayout>(buffer_access_flags).data();

      // NOTE: guaranteed by assimp loader?
      debug::check_expr(e_mesh.has_norms() && e_mesh.has_uvs(), "Mesh data incomplete");

      // Initialize meshing data
      m_vpos_buffer = {{ .data = cnt_span<const std::byte>(e_mesh.verts) }};
      m_vnor_buffer = {{ .data = cnt_span<const std::byte>(e_mesh.norms) }};
      m_vuvs_buffer = {{ .data = cnt_span<const std::byte>(e_mesh.uvs)   }};
      m_elem_buffer = {{ .data = cnt_span<const std::byte>(e_mesh.elems) }};
      m_array = {{
        .buffers  = {{ .buffer = &m_vpos_buffer, .index = 0, .stride = sizeof(AlMeshData::VertTy) },
                     { .buffer = &m_vnor_buffer, .index = 1, .stride = sizeof(AlMeshData::VertTy) },
                     { .buffer = &m_vuvs_buffer, .index = 2, .stride = sizeof(AlMeshData::UVTy)   }},
        .attribs  = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 },
                     { .attrib_index = 1, .buffer_index = 1, .size = gl::VertexAttribSize::e3 },
                     { .attrib_index = 2, .buffer_index = 2, .size = gl::VertexAttribSize::e2 }},
        .elements = &m_elem_buffer
      }};

      // Initialize texture data
      m_texture = {{ .size = e_texture.size(), .data = cast_span<const float>(e_texture.data()) }};
      m_sampler = {{
        .min_filter = gl::SamplerMinFilter::eLinear,
        .mag_filter = gl::SamplerMagFilter::eLinear,
      }};

      // Initialize draw object
      m_draw = { 
        .type             = gl::PrimitiveType::eTriangles,
        .vertex_count     = static_cast<uint>(e_mesh.elems.size()) * 3,
        .capabilities     = {{ gl::DrawCapability::eMSAA,      true },
                             { gl::DrawCapability::eDepthTest, true },
                             { gl::DrawCapability::eCullOp,    true }},
        .draw_op          = gl::DrawOp::eFill,
        .bindable_array   = &m_array,
        .bindable_program = &m_program 
      };
    }
    
    void eval(SchedulerHandle &info) override {
      met_trace_full();

      // Get shared resources 
      const auto &e_arcball = info.relative("view_input")("arcball").read_only<detail::Arcball>();

      // Push uniform data
      m_unif_buffer_map->camera_matrix = e_arcball.full().matrix();
      m_unif_buffer.flush();

      // Bind required resources to corresponding targets
      m_program.bind("b_unif", m_unif_buffer);
      m_program.bind("b_txtr", m_sampler);
      m_program.bind("b_txtr", m_texture);

      // Dispatch shader to draw mesh
      gl::dispatch_draw(m_draw);
    }
  };
} // namespace met