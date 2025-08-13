// Copyright (C) 2024 Mark van de Ruit, Delft University of Technology.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <metameric/scene/scene.hpp>
#include <metameric/core/distribution.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <algorithm>
#include <bit>
#include <bitset>
#include <execution>
#include <numbers>
#include <numeric>

namespace met::detail {
  struct NodePack {
    uint data;                        // type 1b | child mask 8b | size 4b | offs 19b
    std::array<uint, 3> aabb;         // [lo.x, lo.y], [hi.x, hi.y], [lo.z, hi.z]
    std::array<uint, 8> child_aabb_0; // per child: lo.x | lo.y | hi.x | hi.y
    std::array<uint, 4> child_aabb_1; // per child: lo.z | hi.z
  };
  static_assert(sizeof(NodePack) == 64);

  // Helper method to pack BVH node data to NodePack type
  NodePack pack(const BVH<8>::Node &node) {
    met_trace();

    // Output node pack
    NodePack p;

    // Generate enclosing AABB over children
    auto child_aabbs = std::span<const AABB>(node.child_aabb.begin(), node.size);
    auto parent_aabb =*rng::fold_left_first(child_aabbs, std::plus {});

    // 3xu32 packs AABB lo, ex
    auto b_lo_in = parent_aabb.minb;
    auto b_ex_in = (parent_aabb.maxb - parent_aabb.minb).eval();
    p.aabb[0] = detail::pack_unorm_2x16_floor({ b_lo_in.x(), b_lo_in.y() });
    p.aabb[1] = detail::pack_unorm_2x16_ceil ({ b_ex_in.x(), b_ex_in.y() });
    p.aabb[2] = detail::pack_unorm_2x16_floor({ b_lo_in.z(), 0 }) 
              | detail::pack_unorm_2x16_ceil ({ 0, b_ex_in.z() });

    // Convert child leaf/node mask to bit field
    uint child_mask = 0;
    for (uint i = 0; i < node.child_mask.size(); ++i)
      child_mask |= node.child_mask[i] << i;

    // type 1b | child mask 8b | size 4b | offs 19b
    p.data =  (0x007FFFFu & node.offset)        // 19 bits, child range offset
           | ((0x000000Fu & node.size)   << 19) // 4 bits,  child range size
           | ((0x00000FFu & child_mask)  << 23) // 8 bits,  child leaf/node mask
           | ((0x0000001u & node.type)   << 31) // 1 bit,   current leaf/node type
           ;

    // Child AABBs are packed in 6 bytes per child
    p.child_aabb_0.fill(0);
    p.child_aabb_1.fill(0);
    for (uint i = 0; i < child_aabbs.size(); ++i) {
      auto b_lo_safe = ((child_aabbs[i].minb - b_lo_in) / b_ex_in).eval();
      auto b_hi_safe = ((child_aabbs[i].maxb - b_lo_in) / b_ex_in).eval();
      auto pack_0 = detail::pack_unorm_4x8_floor((eig::Array4f() << b_lo_safe.head<2>(), 0, 0).finished())
                  | detail::pack_unorm_4x8_ceil ((eig::Array4f() << 0, 0, b_hi_safe.head<2>()).finished());
      auto pack_1 = detail::pack_unorm_4x8_floor((eig::Array4f() << b_lo_safe.z(), 0, 0, 0).finished())
                  | detail::pack_unorm_4x8_ceil ((eig::Array4f() << 0, b_hi_safe.z(), 0, 0).finished());
      p.child_aabb_0[i    ] |= pack_0;
      p.child_aabb_1[i / 2] |= (pack_1 << ((i % 2) ? 16 : 0));
    }

    return p;
  }

  // Helper to calculate (poorly) fitting AABB
  AABB generate_rotated_aabb(const eig::Matrix4f &trf) {
    // Corners of AABB
    std::vector<eig::Array3f> corners = {
      (trf * eig::Vector4f(0.f, 0.f, 0.f, 1.f)).head<3>().eval(),
      (trf * eig::Vector4f(0.f, 0.f, 1.f, 1.f)).head<3>().eval(),
      (trf * eig::Vector4f(0.f, 1.f, 0.f, 1.f)).head<3>().eval(),
      (trf * eig::Vector4f(0.f, 1.f, 1.f, 1.f)).head<3>().eval(),
      (trf * eig::Vector4f(1.f, 0.f, 0.f, 1.f)).head<3>().eval(),
      (trf * eig::Vector4f(1.f, 0.f, 1.f, 1.f)).head<3>().eval(),
      (trf * eig::Vector4f(1.f, 1.f, 0.f, 1.f)).head<3>().eval(),
      (trf * eig::Vector4f(1.f, 1.f, 1.f, 1.f)).head<3>().eval(),
    };
    
    // Establish poor bounnding box around rotated vertices
    // (should probably just recalculate over entire mesh instead)
    AABB aabb = {
      .minb = *rng::fold_left_first(corners, [](auto a, auto b) { return a.cwiseMin(b).eval(); }),
      .maxb = *rng::fold_left_first(corners, [](auto a, auto b) { return a.cwiseMax(b).eval(); })
    };

    return aabb;
  }

  AABB generate_fitting_aabb(const Mesh &mesh, const eig::Matrix4f &trf) {
    // Transform mesh data in parallel
    std::vector<Mesh::vert_type> verts(mesh.verts.size());
    std::transform(std::execution::par_unseq, range_iter(mesh.verts), verts.begin(),
      [&trf](const auto &vt) { return (trf * (eig::Vector4f() << vt, 1).finished()).template head<3>().eval(); });
    
    // Establish bounding box around mesh's vertices
    eig::Array3f minb = std::reduce(std::execution::par_unseq, range_iter(verts), verts[0],
      [](auto a, auto b) { return a.cwiseMin(b).eval(); });
    eig::Array3f maxb = std::reduce(std::execution::par_unseq, range_iter(verts), verts[0],
      [](auto a, auto b) { return a.cwiseMax(b).eval(); });
    
    return AABB { minb, maxb };
  }

  SceneGLHandler<met::Mesh>::SceneGLHandler() {
    met_trace_full();

    // Preallocate up to a number of meshes and obtain writeable/flushable mapping
    std::tie(blas_info, m_blas_info_map) = gl::Buffer::make_flusheable_object<BLASInfoBufferLayout>();
  }

  void SceneGLHandler<met::Mesh>::update(const Scene &scene) {
    met_trace_full();

    const auto &meshes = scene.resources.meshes;
    guard(!meshes.empty() && meshes);

    // Resize cache vector, which keeps cleaned, simplified mesh data around 
    mesh_cache.resize(meshes.size());

    // Set appropriate mesh count in buffer
    m_blas_info_map->size = static_cast<uint>(meshes.size());

    // For each mesh, we simplify the mesh to a hardcoded maximum, 
    // fit it to a [0, 1] cube, and finally compute a BVH over the result.
    // The result is cached cpu-side
    #pragma omp parallel for
    for (int i = 0; i < meshes.size(); ++i) {
      const auto &[value, state] = meshes[i];
      guard_continue(state);
      MeshData &data = mesh_cache[i];
      data.mesh     = fixed_degenerate_uvs<met::Mesh>(simplified_mesh<met::Mesh>(value, 131072, 5e-3));
      data.unit_trf = unitize_mesh<met::Mesh>(data.mesh);
      data.bvh      = {{ .mesh = data.mesh, .n_leaf_children = 1 }};
    }

    // Pack mesh/BVH data tightly and fill in corresponding mesh layout info
    // Temporary layout data
    std::vector<uint> verts_size(meshes.size()), // Nr. of verts in mesh i
                      verts_offs(meshes.size()), // Nr. of elems in mesh i
                      nodes_size(meshes.size()), // Nr. of elems in bvh i
                      nodes_offs(meshes.size()), // Nr. of nodes prior to bvh i
                      elems_size(meshes.size()), // Nr. of elems in mesh i
                      elems_offs(meshes.size()); // Nr. of elems prior to mesh i
    
    // Fill in vert/node/elem sizes and scans
    rng::transform(mesh_cache, verts_size.begin(), [](const auto &m) -> uint { return m.mesh.verts.size(); });
    rng::transform(mesh_cache, nodes_size.begin(), [](const auto &m) -> uint { return m.bvh.nodes.size();  });
    rng::transform(mesh_cache, elems_size.begin(), [](const auto &m) -> uint { return m.mesh.elems.size(); });
    std::exclusive_scan(range_iter(verts_size), verts_offs.begin(), 0u);
    std::exclusive_scan(range_iter(nodes_size), nodes_offs.begin(), 0u);
    std::exclusive_scan(range_iter(elems_size), elems_offs.begin(), 0u);
    
    // Fill in mesh layout info
    #pragma omp parallel for
    for (int i = 0; i < meshes.size(); ++i) {
      auto &layout = m_blas_info_map->data[i];
      auto &data   = mesh_cache[i];
      layout.nodes_offs = nodes_offs[i];
      layout.prims_offs = elems_offs[i];
      data.nodes_offs   = nodes_offs[i];
      data.prims_offs   = elems_offs[i];
    }

    // Mostly temporary data packing vectors; will be copied to gpu-side buffers after
    std::vector<VertexPack>    verts_packed(verts_size.back() + verts_offs.back()); // Compressed and packed vertex data
    std::vector<eig::Array3u>  elems_packed(elems_size.back() + elems_offs.back()); // Uncompressed, just packed element indices
    std::vector<NodePack>      nodes_packed(nodes_size.back() + nodes_offs.back()); // Compressed 8-way BLAS node data
    std::vector<PrimitivePack> prims_packed(elems_size.back() + elems_offs.back()); // Compressed 8-way BLAS leaf data, ergo duplicated mesh primitives

    // Pack data tightly into pack data vectors
    #pragma omp parallel for
    for (int i = 0; i < meshes.size(); ++i) {
      const auto &mesh_data = mesh_cache[i];
      const auto &blas = mesh_data.bvh;
      const auto &mesh = mesh_data.mesh;

      // Pack vertex data tightly and copy to the correctly offset range
      #pragma omp parallel for
      for (int j = 0; j < mesh.verts.size(); ++j) {
        auto norm = mesh.has_norms() ? mesh.norms[j] : eig::Array3f(0, 0, 1);
        auto txuv = mesh.has_txuvs() ? mesh.txuvs[j] : eig::Array2f(0.5);

        // Force UV to [0, 1]
        txuv = txuv.unaryExpr([](float f) {
          int   i = static_cast<int>(f);
          float a = f - static_cast<float>(i);
          return (i % 2) ? 1.f - a : a;
        });

        // Vertices are compressed as well as packed
        Vertex v = { .p = mesh.verts[j], .n = norm, .tx = txuv };
        verts_packed[verts_offs[i] + j] = v.pack();
      }

      // Pack primitive data tightly and copy to the correctly offset range
      // while scattering data into leaf node order for the BVH
      #pragma omp parallel for
      for (int j = 0; j < blas.prims.size(); ++j) {
        // BVH primitives are packed in bvh order
        auto el = mesh.elems[blas.prims[j]];
        prims_packed[elems_offs[i] + j] = PrimitivePack {
          .v0 = verts_packed[verts_offs[i] + el[0]],
          .v1 = verts_packed[verts_offs[i] + el[1]],
          .v2 = verts_packed[verts_offs[i] + el[2]]
        };
      }

      // Pack node data tightly and copy to the correctly offset range
      #pragma omp parallel for
      for (int j = 0; j < blas.nodes.size(); ++j) {
        nodes_packed[nodes_offs[i] + j] = pack(blas.nodes[j]);
      }
      
      // Copy element indices and adjust to refer to the correctly offset range
      rng::transform(mesh.elems, elems_packed.begin() + elems_offs[i], 
        [o = verts_offs[i]](const auto &v) -> eig::AlArray3u { return v + o; });
    }

    // Copy data to buffers
    mesh_verts = {{ .data = cnt_span<const std::byte>(verts_packed) }};
    mesh_elems = {{ .data = cnt_span<const std::byte>(elems_packed) }};
    blas_nodes = {{ .data = cnt_span<const std::byte>(nodes_packed) }};
    blas_prims = {{ .data = cnt_span<const std::byte>(prims_packed) }};

    // Define corresponding vertex array object and generate multidraw command info
    mesh_array = {{
      .buffers = {{ .buffer = &mesh_verts, .index = 0, .stride = sizeof(eig::Array4u)      }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e4 }},
      .elements = &mesh_elems
    }};
    mesh_draw.resize(meshes.size());
    for (uint i = 0; i < meshes.size(); ++i) {
      mesh_draw[i] = { .vertex_count = elems_size[i] * 3, .vertex_first = elems_offs[i] * 3 };
    }

    // Flush changes to layout data
    blas_info.flush();
  }

  SceneGLHandler<met::Image>::SceneGLHandler() {
    met_trace_full();

    // Preallocate up to a number of blocks
    std::tie(texture_info, m_texture_info_map) = gl::Buffer::make_flusheable_object<BufferLayout>();
  }

  void SceneGLHandler<met::Image>::update(const Scene &scene) {
    met_trace_full();
    
    const auto &images = scene.resources.images;
    const auto &e_settings = scene.components.settings.value;
    guard(!images.empty() && (scene.resources.images || scene.components.settings.state.texture_size));

    // Keep track of which atlas' position a texture needs to be stuffed in
    std::vector<uint> indices(images.size(), std::numeric_limits<uint>::max());

    // Generate inputs for texture atlas generation
    std::vector<eig::Array2u> inputs_3f, inputs_1f;
    for (uint i = 0; i < images.size(); ++i) {
      const auto &[img, state] = images[i];
      bool is_3f 
         = img.pixel_frmt() == Image::PixelFormat::eRGB
        || img.pixel_frmt() == Image::PixelFormat::eRGBA;

      indices[i] = is_3f ? inputs_3f.size() : inputs_1f.size();
      
      if (is_3f) inputs_3f.push_back(img.size());
      else       inputs_1f.push_back(img.size());
    } // for (uint i)

    // Determine maximum texture sizes, and texture scaling necessary to uphold size settings
    eig::Array2u max_3f = rng::fold_left(inputs_3f, eig::Array2u(0), eig::cwiseMax<eig::Array2u>);
    eig::Array2u max_1f = rng::fold_left(inputs_1f, eig::Array2u(0), eig::cwiseMax<eig::Array2u>);
    eig::Array2f mul_3f = e_settings.apply_texture_size(max_3f).cast<float>() / max_3f.cast<float>();
    eig::Array2f mul_1f = e_settings.apply_texture_size(max_1f).cast<float>() / max_1f.cast<float>();

    // Scale input sizes by appropriate scaling
    rng::transform(inputs_3f, inputs_3f.begin(), 
      [mul_3f](const auto &v) { return (v.template cast<float>() * mul_3f).max(1.f).template cast<uint>().eval(); });
    rng::transform(inputs_1f, inputs_1f.begin(), 
      [mul_1f](const auto &v) { return (v.template cast<float>() * mul_1f).max(1.f).template cast<uint>().eval(); });

    // Rebuild texture atlases with mips
    texture_atlas_3f = {{ .sizes = inputs_3f, .levels = 1 }};
    texture_atlas_1f = {{ .sizes = inputs_1f, .levels = 1 }};

    // Set appropriate component count, then flush change to buffer
    m_texture_info_map->size = static_cast<uint>(images.size());
    texture_info.flush(sizeof(uint));

    for (uint i = 0; i < images.size(); ++i) {
      const auto &[img, state] = images[i];

      // Load patch from appropriate atlas (3f or 1f)
      bool is_3f 
         = img.pixel_frmt() == Image::PixelFormat::eRGB
        || img.pixel_frmt() == Image::PixelFormat::eRGBA;
      auto size = is_3f ? texture_atlas_3f.capacity() : texture_atlas_1f.capacity();
      auto resrv = is_3f ? texture_atlas_3f.patch(indices[i]) : texture_atlas_1f.patch(indices[i]);

      // Determine UV coordinates of the texture inside the full atlas
      eig::Array2f uv0 = resrv.offs.cast<float>() / size.head<2>().cast<float>(),
                   uv1 = resrv.size.cast<float>() / size.head<2>().cast<float>();

      // Fill in info object in mapped buffer
      m_texture_info_map->data[i] = { 
        .is_3f = is_3f, 
        .layer = resrv.layer_i,
        .offs  = resrv.offs, 
        .size  = resrv.size, 
        .uv0   = uv0,   
        .uv1   = uv1 
      };

      // Flush change to buffer; most changes to objects are local,
      // so we flush specific regions instead of the whole
      texture_info.flush(sizeof(BlockLayout), sizeof(BlockLayout) * i + sizeof(uint));

      // Put properly resampled image in appropriate place (rg)
      if (is_3f) {
        auto imgf = img.convert({ .resize_to  = resrv.size,
                                  .pixel_frmt = Image::PixelFormat::eRGB,
                                  .pixel_type = Image::PixelType::eFloat,
                                  .color_frmt = Image::ColorFormat::eLRGB })/* .normalize({}).first */;
        texture_atlas_3f.texture().set(imgf.data<float>(), 0,
          { resrv.size.x(), resrv.size.y(), 1             },
          { resrv.offs.x(), resrv.offs.y(), resrv.layer_i });
      } else {
        auto imgf = img.convert({ .resize_to  = resrv.size,
                                  .pixel_frmt = Image::PixelFormat::eAlpha,
                                  .pixel_type = Image::PixelType::eFloat,
                                  .color_frmt = Image::ColorFormat::eNone });
        texture_atlas_1f.texture().set(imgf.data<float>(), 0,
          { resrv.size.x(), resrv.size.y(), 1             },
          { resrv.offs.x(), resrv.offs.y(), resrv.layer_i });
      }
    } // for (uint i)

    // Fill in mip data
    if (texture_atlas_3f.texture().is_init()) 
      texture_atlas_3f.texture().generate_mipmaps();
    if (texture_atlas_1f.texture().is_init()) 
      texture_atlas_1f.texture().generate_mipmaps();
  }

  SceneGLHandler<met::Spec>::SceneGLHandler() {
    met_trace_full();

    auto n_layers   = std::min<uint>(gl::get_variable_int(gl::VariableName::eMaxArrayTextureLayers), met_max_constraints);
    spec_texture    = {{ .size = { wavelength_samples, n_layers } }};
    std::tie(spec_buffer, spec_buffer_map) = gl::Buffer::make_flusheable_span<Spec>(n_layers);
  }

  void SceneGLHandler<met::Spec>::update(const Scene &scene) {
    met_trace_full();

    const auto &illuminants = scene.resources.illuminants;
    guard(illuminants);
    
    for (uint i = 0; i < illuminants.size(); ++i) {
      const auto &[value, state] = illuminants[i];
      guard_continue(state);
      spec_buffer_map[i] = value;  
    }

    spec_buffer.flush();
    spec_texture.set(spec_buffer);
  }

  SceneGLHandler<met::CMFS>::SceneGLHandler() {
    met_trace_full();
    auto n_layers   = std::min<uint>(gl::get_variable_int(gl::VariableName::eMaxArrayTextureLayers), 16);
    cmfs_texture    = {{ .size = { wavelength_samples, n_layers } }};
    std::tie(cmfs_buffer, cmfs_buffer_map) = gl::Buffer::make_flusheable_span<CMFS>(n_layers);
  }

  void SceneGLHandler<met::CMFS>::update(const Scene &scene) {
    met_trace_full();

    const auto &observers = scene.resources.observers;
    guard(observers);
    
    for (uint i = 0; i < observers.size(); ++i) {
      const auto &[value, state] = observers[i];
      guard_continue(state);

      // Premultiply with RGB and normalize
      ColrSystem csys = { .cmfs = value, .illuminant = Spec(1)  };
      CMFS       cmfs = csys.finalize();
      cmfs_buffer_map[i] = cmfs.transpose().reshaped(wavelength_samples, 3);
    }

    cmfs_buffer.flush();
    cmfs_texture.set(cmfs_buffer);
  }

  SceneGLHandler<met::Scene>::SceneGLHandler() {
    met_trace_full();

    // // Obtain writeable/flushable mapping for scene layout
    // std::tie(scene_info, m_scene_info_map) = gl::Buffer::make_flusheable_object<BlockLayout>();
  }

  void SceneGLHandler<met::Scene>::update(const Scene &scene) {
    met_trace_full();

    // // Destroy old sync object
    // m_fence = { };

    // // Get relevant resources
    // const auto &objects    = scene.components.objects;
    // const auto &emitters   = scene.components.emitters;
    // const auto &views      = scene.components.views;
    // const auto &upliftings = scene.components.upliftings;

    // guard(objects || emitters || upliftings || views);

    // *m_scene_info_map = BlockLayout {
    //   .n_objects    = static_cast<uint>(objects.size()),
    //   .n_emitters   = static_cast<uint>(emitters.size()),
    //   .n_views      = static_cast<uint>(views.size()),
    //   .n_upliftings = static_cast<uint>(upliftings.size())
    // };

    // // Write out changes to buffer
    // scene_info.flush();
    // gl::sync::memory_barrier(gl::BarrierFlags::eBufferUpdate      |  
    //                           gl::BarrierFlags::eUniformBuffer     |
    //                           gl::BarrierFlags::eClientMappedBuffer);
                              
    // // Generate sync object for gpu wait
    // m_fence = gl::sync::Fence(gl::sync::time_ns(1));

    // // Collect bounding boxes for active scene emitters and objects as one shared type
    // struct PrimitiveData {
    //   bool is_object;
    //   uint i;
    //   AABB aabb;
    // };
    // std::vector<PrimitiveData> prims;

    // // Collect object data and AABB
    // for (int i = 0; i < objects.size(); ++i) {
    //   const auto &[object, state] = objects[i];
    //   guard_continue(object.is_active);

    //   const auto &mesh = scene.resources.meshes[object.mesh_i].value();

    //   // Get mesh transform, incorporate into object transform
    //   auto object_trf = object.transform.affine().matrix().eval();
    //   auto mesh_trf   = scene.resources.meshes.gl.mesh_cache[object.mesh_i].unit_trf;
    //   auto trf        = (object_trf * mesh_trf).eval();

    //   prims.push_back({ .is_object = true, 
    //                     .i         = static_cast<uint>(i), 
    //                     .aabb      = generate_rotated_aabb(trf) });
    // }

    // // Collect emitter data and AABB
    // for (int i = 0; i < emitters.size(); ++i) {
    //   const auto &[emitter, state] = emitters[i];
    //   guard_continue(emitter.is_active);
    //   guard_continue(emitter.type != Emitter::Type::eEnviron && emitter.type != Emitter::Type::ePoint);
            
    //   // Get emitter transform
    //   auto trf = emitter.transform.affine();
      
    //   // Get AABB dependent on emitter shape
    //   AABB aabb;
    //   if (emitter.type == Emitter::Type::eRect) {
    //     aabb.minb = (trf * eig::Vector4f(-.5f, -.5f, 0.f, 1.f)).head<3>().eval();
    //     aabb.maxb = (trf * eig::Vector4f(0.5f, 0.5f, 0.f, 1.f)).head<3>().eval();
    //   } else if (emitter.type == Emitter::Type::eSphere) {
    //     auto transl = eig::Affine3f(eig::Translation3f(eig::Vector3f { -.5f, -.5f, -.5f })).matrix().eval();
    //     aabb = generate_rotated_aabb((trf.matrix() * transl).matrix().eval());
    //   }

    //   prims.push_back({ .is_object = false,
    //                     .i         = static_cast<uint>(i), 
    //                     .aabb      = aabb });
    // }

    // // Get vector of AABBs only
    // auto prim_aabbs = vws::transform(prims, &PrimitiveData::aabb) | view_to<std::vector<AABB>>();

    // // Generate scene-enclosing bounding box
    // AABB scene_aabb = std::reduce(
    //   std::execution::par_unseq,
    //   range_iter(prim_aabbs),
    //   prim_aabbs[0],
    //   [](const auto& a, const auto &b) {
    //     return AABB { a.minb.cwiseMin(b.minb).eval(), a.maxb.cwiseMax(b.maxb).eval() };
    //   });
    
    // // Generate transformation to cap scene to a [0, 1] bbox
    // auto scale = (scene_aabb.maxb - scene_aabb.minb).eval();
    // scale = (scale.array().abs() > 0.00001f).select(1.f / scale, eig::Array3f(1));
    // auto trf = (eig::Scaling((scale).matrix().eval()) * eig::Translation3f(-scene_aabb.minb)).matrix().eval();

    // // Apply transformation to interior AABBs
    // std::for_each(
    //   std::execution::par_unseq,
    //   range_iter(prim_aabbs),
    //   [trf](AABB &aabb) {
    //     aabb.minb = (trf * (eig::Vector4f() << aabb.minb, 1).finished()).head<3>().eval();
    //     aabb.maxb = (trf * (eig::Vector4f() << aabb.maxb, 1).finished()).head<3>().eval();
    //   });
    
    // // Push transformations to buffer
    // m_tlas_info_map->trf = trf;
    // tlas_info.flush();
    
    /* // Create top-level BVH over AABBS
    BVH<8> bvh = {{ .aabb = prim_aabbs, .n_leaf_children = 1 }};

    // Pack BVH node/prim data tightly
    std::vector<uint>     prims_packed(bvh.prims.size());
    std::vector<NodePack> nodes_packed(bvh.nodes.size()); // Compressed 8-way tlas nodes
  
    // Pack primitive data tightly
    #pragma omp parallel for
    for (int j = 0; j < bvh.prims.size(); ++j) {
      // Small helper to pack primitive data in a single integer
      constexpr auto pack_prim = [](const PrimitiveData &prim) -> uint {
        return (prim.is_object ? 0x00000000 : 0x80000000) | (prim.i & 0x00FFFFFF);
      };
      prims_packed[j] = pack_prim(prims[bvh.prims[j]]);
    }

    // Pack node data tightly
    #pragma omp parallel for
    for (int j = 0; j < bvh.nodes.size(); ++j) {
      nodes_packed[j] = pack(bvh.nodes[j]);
    }

    // Push BVH data to buffer
    tlas_nodes = {{ .data = cnt_span<const std::byte>(nodes_packed) }};
    tlas_prims = {{ .data = cnt_span<const std::byte>(prims_packed) }}; */
  }
} // namespace met::detail