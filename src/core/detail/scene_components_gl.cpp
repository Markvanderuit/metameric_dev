#include <metameric/core/detail/scene_components_gl.hpp>
#include <metameric/core/distribution.hpp>
#include <metameric/core/moments.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/scene.hpp>
#include <ranges>
#include <numbers>

namespace met::detail {
  constexpr static auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr static auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  // Packed BVH node data
  struct NodePack {
    uint aabb_pack_0;                 // lo.x, lo.y
    uint aabb_pack_1;                 // hi.x, hi.y
    uint aabb_pack_2;                 // lo.z, hi.z
    uint data_pack;                   // leaf | size | offs
    std::array<uint, 8> child_pack_0; // per child: lo.x | lo.y | hi.x | hi.y
    std::array<uint, 4> child_pack_1; // per child: lo.z | hi.z
  };
  static_assert(sizeof(NodePack) == 64);

  // Helper method to pack BVH node data tightly
  NodePack pack(const BVH::Node &node) {
    // Obtain a merger of the child bounding boxes
    constexpr auto merge = [](const BVH::AABB &a, const BVH::AABB &b) -> BVH::AABB {
      return { .minb = a.minb.cwiseMin(b.minb), .maxb = a.maxb.cwiseMax(b.maxb) };
    };

    auto aabb = *rng::fold_left_first(std::span(node.child_aabb.begin(), node.size()), merge); //.value();
    // auto aabb = rng::fold_left_first(node.child_aabb.begin(), 
    //                                  node.child_aabb.begin() + node.size(), merge).value();

    NodePack p;

    // 3xu32 packs AABB lo, ex
    auto b_lo_in = aabb.minb;
    auto b_ex_in = (aabb.maxb - aabb.minb).eval();
    p.aabb_pack_0 = detail::pack_unorm_2x16_floor({ b_lo_in.x(), b_lo_in.y() });
    p.aabb_pack_1 = detail::pack_unorm_2x16_ceil ({ b_ex_in.x(), b_ex_in.y() });
    p.aabb_pack_2 = detail::pack_unorm_2x16_floor({ b_lo_in.z(), 0 }) 
                  | detail::pack_unorm_2x16_ceil ({ 0, b_ex_in.z() });

    // 1xu32 packs node type, child offset, child count
    p.data_pack = node.offs_data | (node.size_data << 27);

    // Child AABBs are packed in 6 bytes per child
    p.child_pack_0.fill(0);
    p.child_pack_1.fill(0);
    for (uint i = 0; i < node.size(); ++i) {
      auto b_lo_safe = ((node.child_aabb[i].minb - b_lo_in) / b_ex_in).eval();
      auto b_hi_safe = ((node.child_aabb[i].maxb - b_lo_in) / b_ex_in).eval();
      auto pack_0 = detail::pack_unorm_4x8_floor((eig::Array4f() << b_lo_safe.head<2>(), 0, 0).finished())
                  | detail::pack_unorm_4x8_ceil ((eig::Array4f() << 0, 0, b_hi_safe.head<2>()).finished());
      auto pack_1 = detail::pack_unorm_4x8_floor((eig::Array4f() << b_lo_safe.z(), 0, 0, 0).finished())
                  | detail::pack_unorm_4x8_ceil ((eig::Array4f() << 0, b_hi_safe.z(), 0, 0).finished());
      p.child_pack_0[i    ] |= pack_0;
      p.child_pack_1[i / 2] |= (pack_1 << ((i % 2) ? 16 : 0));
    }

    return p;
  }

  eig::Array2u clamp_texture_size_by_setting(Settings::TextureSize setting, eig::Array2u size) {
    switch (setting) {
      case Settings::TextureSize::eHigh: return size.cwiseMin(2048u);
      case Settings::TextureSize::eMed:  return size.cwiseMin(1024u);
      case Settings::TextureSize::eLow:  return size.cwiseMin(512u);
      default:                           return size;
    }
  }

  SceneGLType<met::Object>::SceneGLType() {
    met_trace_full();

    // Uniform layout which includes nr. of active components
    struct ObjectUniformBufferLayout {
      alignas(4) uint n;
      ObjectInfoLayout data[max_supported_objects];
    };

    // Preallocate up to a number of meshes
    object_info = {{ .size  = sizeof(ObjectUniformBufferLayout),
                     .flags = buffer_create_flags }};

    // Obtain writeable, flushable mapping of nr. of meshes and individual mesh data
    auto *map = object_info.map_as<ObjectUniformBufferLayout>(buffer_access_flags).data();
    m_info_map_size = &map->n;
    m_info_map_data = map->data;
  }

  void SceneGLType<met::Object>::update(const Scene &scene) {
    met_trace_full();

    const auto &objects = scene.components.objects;
    guard(!objects.empty() && objects);

    // Set appropriate object count, then flush change to buffer
    *m_info_map_size = static_cast<uint>(objects.size());
    object_info.flush(sizeof(uint));

    // Write updated objects to mapping
    for (uint i = 0; i < objects.size(); ++i) {
      const auto &[object, state] = objects[i];
      guard_continue(state);
      
      // Get relevant texture info
      bool is_albedo_sampled = object.diffuse.index() != 0;

      // Get mesh transform, incorporate into gl-side object transform
      auto object_trf = object.transform.affine();
      auto mesh_trf   = scene.resources.meshes.gl.transforms[object.mesh_i];

      m_info_map_data[i] = {
        // Fill transform data
        .trf          = object_trf.matrix(),
        .trf_inv      = object_trf.matrix().inverse().eval(),
        .trf_mesh     = (object_trf.matrix() * mesh_trf).eval(),
        .trf_mesh_inv = (object_trf.matrix() * mesh_trf).inverse().eval(),

        // Fill shape data
        .is_active   = object.is_active,
        .mesh_i      = object.mesh_i,
        .uplifting_i = object.uplifting_i,

        // Fill materials data
        .is_albedo_sampled = is_albedo_sampled,
        .albedo_i          = is_albedo_sampled ? std::get<1>(object.diffuse) : 0,
        .albedo_v          = is_albedo_sampled ? 0 : std::get<0>(object.diffuse)
      };

      // Flush change to buffer; most changes to objects are local,
      // so we flush specific regions instead of the whole
      object_info.flush(sizeof(ObjectInfoLayout), 
                        sizeof(ObjectInfoLayout) * i + sizeof(uint) * 4);
    } // for (uint i)

  }
  SceneGLType<met::Uplifting>::SceneGLType() {
    met_trace_full();

    // Initialize texture atlas to hold per-object coefficients
    texture_coefficients = {{ .levels  = 1, .padding = 0 }};

    // Initialize warped phase data for spectral MESE method
    auto warp_data = generate_warped_phase();
    texture_warp = {{ .size = { static_cast<uint>(warp_data.size()) },
                      .data = cnt_span<const float>(warp_data)      }};

    // Initialize basis function data
    texture_basis = {{ .size = { wavelength_samples, wavelength_bases } }};
  }

  void SceneGLType<met::Uplifting>::update(const Scene &scene) {
    // Get relevant resources
    const auto &e_objects    = scene.components.objects;
    const auto &e_upliftings = scene.components.upliftings;
    const auto &e_images     = scene.resources.images;
    const auto &e_settings   = scene.components.settings;
    
    // Barycentric texture has not been touched yet
    e_upliftings.gl.texture_coefficients.set_invalitated(false);
    
    // Only rebuild if there are upliftings and objects
    guard(!e_upliftings.empty() && !e_upliftings.empty());
    guard(e_upliftings                                              || 
          e_objects                                                 ||
          e_settings.state.texture_size                             ||
          !e_upliftings.gl.texture_coefficients.texture().is_init() ||
          !e_upliftings.gl.texture_coefficients.buffer().is_init()  );
    
    // Gather necessary texture sizes for each object
    std::vector<eig::Array2u> inputs(e_objects.size());
    rng::transform(e_objects, inputs.begin(), [&](const auto &comp) -> eig::Array2u {
      const auto &object = comp.value;
      if (auto value_ptr = std::get_if<uint>(&object.diffuse)) {
        // Texture index specified; insert texture size in the atlas inputs
        const auto &image = e_images[*value_ptr].value();
        return image.size();
      } else {
        // Color specified directly; a small patch suffices
        // TODO make smaller and test
        return { 256, 256 };
      }
    });

    // Determine maximum texture sizes, and scale atlas inputs w.r.t. to this value and
    // specified texture settings
    eig::Array2u maximal_4f = rng::fold_left(inputs, eig::Array2u(0), 
      [](auto a, auto b) { return a.cwiseMax(b).eval(); });
    eig::Array2u clamped_4f = e_settings->apply_texture_size(maximal_4f);
    eig::Array2f scaled_4f  = clamped_4f.cast<float>() / maximal_4f.cast<float>();
    for (auto &input : inputs)
      input = (input.cast<float>() * scaled_4f).max(2.f).cast<uint>().eval();

    // Test if the necessitated inputs match exactly to the atlas' reserved patches
    bool is_exact_fit = rng::equal(inputs, texture_coefficients.patches(),
      eig::safe_approx_compare<eig::Array2u>, {}, &detail::TextureAtlasBase::PatchLayout::size);

    // Regenerate atlas if inputs don't match the atlas' current layout
    // Note; barycentric weights will need a full rebuild, which is detected
    //       by the nr. of objects changing or the texture setting changing. A bit spaghetti.
    texture_coefficients.resize(inputs);
    if (texture_coefficients.is_invalitated()) {
      // The barycentric texture was re-allocated, which means underlying memory was all invalidated.
      // So in a case of really bad spaghetti-code, we force object-dependent parts of the pipeline 
      // to rerun here. But uhh, code smell much?
      auto &e_scene = const_cast<Scene &>(scene);
      e_scene.components.objects.set_mutated(true);

      fmt::print("Rebuilt texture atlas\n");
      for (const auto &patch : texture_coefficients.patches()) {
        fmt::print("\toffs = {}, size = {}, uv0 = {}, uv1 = {}\n", patch.offs, patch.size, patch.uv0, patch.uv1);
      }
    }

    // Push basis function data, just default set for now
    {
      const auto &basis = scene.resources.bases[0].value();
      texture_basis.set(obj_span<const float>(basis.func));
    }
  }

  SceneGLType<met::Emitter>::SceneGLType() {
    met_trace_full();

    // Uniform layout which includes nr. of active components
    struct EmitterUniformBufferLayout {
      alignas(4)   uint n;
      EmitterInfoLayout data[max_supported_emitters];
    };
    
    // Preallocate up to a number of objects
    emitter_info = {{ .size  = sizeof(EmitterUniformBufferLayout),
                      .flags = buffer_create_flags }};
    
    // Obtain writeable, flushable mapping of nr. of components and individual data
    auto *map = emitter_info.map_as<EmitterUniformBufferLayout>(buffer_access_flags).data();
    m_info_map_size = &map->n;
    m_info_map_data = map->data;

    // Same for envmap data
    emitter_envm_info = {{ .size  = sizeof(EmitterInfoLayout),
                           .flags = buffer_create_flags }};
    m_envm_map_data = emitter_envm_info.map_as<EmitterEnvmapInfoLayout>(buffer_access_flags).data();
  }

  void SceneGLType<met::Emitter>::update(const Scene &scene) {
    met_trace_full();

    const auto &emitters = scene.components.emitters;
    guard(!emitters.empty() && emitters);

    // Set appropriate component count, then flush change to buffer
    *m_info_map_size = static_cast<uint>(emitters.size());
    emitter_info.flush(sizeof(uint));

    // Write updated components to mapping
    for (uint i = 0; i < emitters.size(); ++i) {
      const auto &[emitter, state] = emitters[i];
      guard_continue(state);

      // Precompute some data based on type
      auto trf = emitter.transform.affine();
      eig::Vector3f rect_n = 0.f;
      float srfc_area_inv  = 0.f, 
            sphere_r       = 0.f;
      
      if (emitter.type == Emitter::Type::eSphere) {
        sphere_r      = 0.5 * emitter.transform.scaling.x();
        srfc_area_inv = 1.f / (4.f * std::numbers::pi_v<float> * sphere_r * sphere_r);
      } else if (emitter.type == Emitter::Type::eRect) {
        rect_n        = (trf * eig::Vector4f(0, 0, 1, 0)).head<3>().normalized();
        srfc_area_inv = 1.f / emitter.transform.scaling.head<2>().prod();
      }
      
      m_info_map_data[i] = {
        .trf              = trf.matrix(),
        .trf_inv          = trf.matrix().inverse().eval(),
        
        .type             = static_cast<uint>(emitter.type),
        .is_active        = emitter.is_active,
        .illuminant_i     = emitter.illuminant_i,
        .illuminant_scale = emitter.illuminant_scale,

        .center        = (trf * eig::Vector4f(0, 0, 0, 1)).head<3>().eval(),
        .srfc_area_inv = srfc_area_inv,
        .rect_n        = rect_n,
        .sphere_r      = sphere_r,
      };

      // Flush change to buffer; most changes to objects are local,
      // so we flush specific regions instead of the whole
      emitter_info.flush(sizeof(EmitterInfoLayout), 
                         sizeof(EmitterInfoLayout) * i + sizeof(uint) * 4);
    } // for (uint i)

    // Build sampling distribution over emitter's powers
    std::vector<float> emitter_distr(max_supported_emitters, 0.f);
    auto active_emitters = scene.components.emitters
                         | vws::transform([](const auto &comp) { return comp.value; })
                         | vws::filter(&Emitter::is_active)
                         | rng::to<std::vector>();
    
    #pragma omp parallel for
    for (int i = 0; i < emitters.size(); ++i) {
      const auto &[emitter, state] = emitters[i];
      guard_continue(emitter.is_active);
      Spec s  = scene.resources.illuminants[emitter.illuminant_i].value();
      emitter_distr[i] = 1.f / static_cast<float>(active_emitters.size());
      // emitter_distr[i] = s.sum() * emitter.illuminant_scale;
    }
    auto distr = Distribution(cnt_span<float>(emitter_distr));   
    emitter_distr_buffer = distr.to_buffer_std140();

    // Store information on first constant emitter, if one is present and active
    m_envm_map_data->envm_is_present = false;
    for (uint i = 0; i < emitters.size(); ++i) {
      const auto &[emitter, state] = emitters[i];
      if (emitter.is_active && emitter.type == Emitter::Type::eConstant) {
        m_envm_map_data->envm_is_present = true;
        m_envm_map_data->envm_i          = i;
        break;
      }
    }
    emitter_envm_info.flush();
  }

  SceneGLType<met::Mesh>::SceneGLType() {
    met_trace_full();

    // Mesh uniform layout which includes nr. of active meshes
    struct MeshUniformBufferLayout {
      alignas(4) uint n;
      MeshInfoLayout data[max_supported_meshes];
    };

    // Preallocate up to a number of meshes
    mesh_info = {{ .size  = sizeof(MeshUniformBufferLayout), .flags = buffer_create_flags }};

    // Obtain writeable, flushable mapping of nr. of meshes and individual mesh data
    auto *map = mesh_info.map_as<MeshUniformBufferLayout>(buffer_access_flags).data();
    m_buffer_layout_map_size = &map->n;
    m_buffer_layout_map_data = map->data;
  }

  void SceneGLType<met::Mesh>::update(const Scene &scene) {
    met_trace_full();

    const auto &meshes = scene.resources.meshes;

    guard(!meshes.empty() && meshes);

    // Resize cache vectors, which keep cleaned, simplified mesh data around 
    m_meshes.resize(meshes.size());
    m_bvhs.resize(meshes.size());
    transforms.resize(meshes.size());

    // Keep around old, unparameterized texture coordinates for now
    std::vector<std::vector<eig::Array2f>> txuvs(meshes.size());

    // Generate cleaned, simplified mesh data
    #pragma omp parallel for
    for (int i = 0; i < meshes.size(); ++i) {
      const auto &[value, state] = meshes[i];
      guard_continue(state);

      // We simplify a copy of the mesh, reparameterize it so texture UVs are
      // unique and non-overlapping, unitize it to a [0, 1] cube, and finally
      // build a bvh acceleration structure over this mess
      m_meshes[i]   = simplified_mesh<met::Mesh>(value, 12288, 1e-3);
      txuvs[i]      = parameterize_mesh<met::Mesh>(m_meshes[i]);
      transforms[i] = unitize_mesh<met::Mesh>(m_meshes[i]);
      m_bvhs[i]     = create_bvh({ .mesh            = m_meshes[i], 
                                   .n_node_children = 8, 
                                   .n_leaf_children = 3 });
            
      // Set appropriate mesh transform in buffer
      m_buffer_layout_map_data[i].trf = transforms[i];
    }
    
    // Set appropriate mesh count in buffer
    *m_buffer_layout_map_size = static_cast<uint>(meshes.size());

    // Pack mesh/BVH data tightly and fill in corresponding mesh layout info
    {
      // Temporary layout data
      std::vector<uint> verts_size(meshes.size()), // Nr. of verts in mesh i
                        verts_offs(meshes.size()), // Nr. of elems in mesh i
                        nodes_size(meshes.size()), // Nr. of elems in bvh i
                        nodes_offs(meshes.size()), // Nr. of nodes prior to bvh i
                        elems_size(meshes.size()), // Nr. of elems in mesh i
                        elems_offs(meshes.size()); // Nr. of elems prior to mesh i
      
      // Fill in vert/node/elem sizes and scans
      rng::transform(m_meshes, verts_size.begin(), [](const auto &m) -> uint { return m.verts.size(); });
      rng::transform(m_bvhs,   nodes_size.begin(), [](const auto &m) -> uint { return m.nodes.size(); });
      rng::transform(m_meshes, elems_size.begin(), [](const auto &m) -> uint { return m.elems.size(); });
      std::exclusive_scan(range_iter(verts_size), verts_offs.begin(), 0u);
      std::exclusive_scan(range_iter(nodes_size), nodes_offs.begin(), 0u);
      std::exclusive_scan(range_iter(elems_size), elems_offs.begin(), 0u);
      
      // Fill in remainder of mesh layout info
      for (int i = 0; i < meshes.size(); ++i) {
        auto &layout = m_buffer_layout_map_data[i];
        layout.verts_offs = verts_offs[i];
        layout.nodes_offs = nodes_offs[i];
        layout.elems_offs = elems_offs[i];
        layout.verts_size = verts_size[i];
        layout.nodes_size = nodes_size[i];
        layout.elems_size = elems_size[i];
      }

      // Mostly temporary data packing vectors; will be copied to gpu-side buffers after
      std::vector<VertexPack>     verts_packed(verts_size.back() + verts_offs.back()); // Compressed and packed
      std::vector<NodePack>       nodes_packed(nodes_size.back() + nodes_offs.back()); // Compressed and packed
      std::vector<uint>           txuvs_packed(verts_size.back() + verts_offs.back()); // Compressed and packed
      std::vector<eig::Array3u>   elems_packed(elems_size.back() + elems_offs.back()); // Not compressed, just packed
      bvh_prims_cpu.resize(elems_size.back() + elems_offs.back());                     // Compressed and packed
      bvh_txuvs_cpu.resize(elems_size.back() + elems_offs.back());                     // Compressed and packed

      // Pack data tightly into pack data vectors
      #pragma omp parallel for
      for (int i = 0; i < meshes.size(); ++i) {
        const auto &bvh  = m_bvhs[i];
        const auto &mesh = m_meshes[i];

        // Pack vertex data tightly and copy to the correctly offset range
        #pragma omp parallel for
        for (int j = 0; j < mesh.verts.size(); ++j) {
          auto norm = mesh.has_norms() ? mesh.norms[j] : eig::Array3f(0, 0, 1);
          auto txuv = mesh.has_txuvs() ? mesh.txuvs[j] : eig::Array2f(0.5);
          txuv = txuv.unaryExpr([](float f) {
            int i = static_cast<int>(f);
            return (i % 2) ? 1.f - (f - i) : f - i;
          });

          // Vertices are compressed as well as packed
          Vertex v = { .p = mesh.verts[j], .n = norm, .tx = txuv };
          verts_packed[verts_offs[i] + j] = v.pack();

          // We keep the unparameterized texture UVs around; set to 0.5 if not present
          txuvs_packed[verts_offs[i] + j] = pack_unorm_2x16(!txuvs[i].empty() ? txuvs[i][j] : txuv);
        }

        // Pack node data tightly and copy to the correctly offset range
        #pragma omp parallel for
        for (int j = 0; j < bvh.nodes.size(); ++j) {
          nodes_packed[nodes_offs[i] + j] = pack(bvh.nodes[j]);
        }

        // Pack primitive data tightly and copy to the correctly offset range
        // while scattering data into leaf node order for the BVH
        #pragma omp parallel for
        for (int j = 0; j < bvh.prims.size(); ++j) {
          // BVH primitives are packed in bvh order
          auto el = mesh.elems[bvh.prims[j]];
          bvh_prims_cpu[elems_offs[i] + j] = PrimitivePack {
            .v0 = verts_packed[verts_offs[i] + el[0]],
            .v1 = verts_packed[verts_offs[i] + el[1]],
            .v2 = verts_packed[verts_offs[i] + el[2]]
          };
          bvh_txuvs_cpu[elems_offs[i] + j] = eig::Array3u {
            txuvs_packed[verts_offs[i] + el[0]],
            txuvs_packed[verts_offs[i] + el[1]],
            txuvs_packed[verts_offs[i] + el[2]]
          };
        }
        
        // Copy element indices and adjust to refer to the correctly offset range
        rng::transform(mesh.elems, elems_packed.begin() + elems_offs[i], 
          [o = verts_offs[i]](const auto &v) -> eig::AlArray3u { return v + o; });
      }

      // Copy data to buffers
      mesh_verts = {{ .data = cnt_span<const std::byte>(verts_packed)  }};
      mesh_elems = {{ .data = cnt_span<const std::byte>(elems_packed)  }};
      mesh_txuvs = {{ .data = cnt_span<const std::byte>(txuvs_packed)  }};
      bvh_nodes  = {{ .data = cnt_span<const std::byte>(nodes_packed)  }};
      bvh_prims  = {{ .data = cnt_span<const std::byte>(bvh_prims_cpu) }};
    }

    // Define corresponding vertex array object and generate multidraw command info
    array = {{
      .buffers = {{ .buffer = &mesh_verts, .index = 0, .stride = sizeof(eig::Array4u)      },
                  { .buffer = &mesh_txuvs, .index = 1, .stride = sizeof(uint)              }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e4 },
                  { .attrib_index = 1, .buffer_index = 1, .size = gl::VertexAttribSize::e1 }},
      .elements = &mesh_elems
    }};
    draw_commands.resize(meshes.size());
    for (uint i = 0; i < meshes.size(); ++i) {
      draw_commands[i] = { .vertex_count = m_buffer_layout_map_data[i].elems_size * 3,
                           .vertex_first = m_buffer_layout_map_data[i].elems_offs * 3 };
    }

    // Flush changes to layout data
    mesh_info.flush();
  }

  SceneGLType<met::Image>::SceneGLType() {
    met_trace_full();

    // Uniform layout which includes nr. of active components
    struct TextureUniformBufferLayout {
      alignas(4)   uint n;
      TextureInfoLayout data[max_supported_textures];
    };

    // Preallocate up to a number of objects
    texture_info = {{ .size  = sizeof(TextureUniformBufferLayout),
                      .flags = buffer_create_flags }};

    // Obtain writeable, flushable mapping of nr. of components and individual data
    auto *map = texture_info.map_as<TextureUniformBufferLayout>(buffer_access_flags).data();
    m_info_map_size = &map->n;
    m_info_map_data = map->data;
  }

  void SceneGLType<met::Image>::update(const Scene &scene) {
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
      bool is_3f = img.pixel_frmt() == Image::PixelFormat::eRGB;

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
      [mul_3f](const auto &v) { return (v.cast<float>() * mul_3f).max(1.f).cast<uint>().eval(); });
    rng::transform(inputs_1f, inputs_1f.begin(), 
      [mul_1f](const auto &v) { return (v.cast<float>() * mul_1f).max(1.f).cast<uint>().eval(); });

    // Rebuild texture atlases with mips
    texture_atlas_3f = {{ .sizes = inputs_3f, .levels = 1 }};
    texture_atlas_1f = {{ .sizes = inputs_1f, .levels = 1 }};

    // Set appropriate component count, then flush change to buffer
    *m_info_map_size = static_cast<uint>(images.size());
    texture_info.flush(sizeof(uint));

    for (uint i = 0; i < images.size(); ++i) {
      const auto &[img, state] = images[i];

      // Load patch from appropriate atlas (3f or 1f)
      bool is_3f = img.pixel_frmt() == Image::PixelFormat::eRGB;
      auto size  = is_3f ? texture_atlas_3f.capacity() : texture_atlas_1f.capacity();
      auto resrv = is_3f ? texture_atlas_3f.patch(indices[i]) : texture_atlas_1f.patch(indices[i]);

      // Determine UV coordinates of the texture inside the full atlas
      eig::Array2f uv0 = resrv.offs.cast<float>() / size.head<2>().cast<float>(),
                   uv1 = resrv.size.cast<float>() / size.head<2>().cast<float>();

      // Fill in info object in mapped buffer
      m_info_map_data[i] = { .is_3f = is_3f,      .layer = resrv.layer_i,
                             .offs  = resrv.offs, .size  = resrv.size,
                             .uv0   = uv0,        .uv1   = uv1 };

      // Flush change to buffer; most changes to objects are local,
      // so we flush specific regions instead of the whole
      texture_info.flush(sizeof(TextureInfoLayout), 
                         sizeof(TextureInfoLayout) * i + sizeof(uint) * 4);

      // Get a resampled float representation of image data
      auto imgf = img.convert({ .resize_to  = resrv.size,
                                .pixel_type = Image::PixelType::eFloat,
                                .color_frmt = Image::ColorFormat::eLRGB });
      
      // Put image in appropriate place
      if (is_3f) {
        texture_atlas_3f.texture().set(imgf.data<float>(), 0,
          { resrv.size.x(), resrv.size.y(), 1             },
          { resrv.offs.x(), resrv.offs.y(), resrv.layer_i });
      } else {
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

  SceneGLType<met::Spec>::SceneGLType() {
    met_trace_full();
    auto n_layers   = std::min<uint>(gl::state::get_variable_int(gl::VariableName::eMaxArrayTextureLayers), max_supported_constraints);
    spec_texture    = {{ .size = { wavelength_samples, n_layers } }};
    spec_buffer     = {{ .size = spec_texture.size().prod() * sizeof(float), .flags = buffer_create_flags }};
    spec_buffer_map = spec_buffer.map_as<Spec>(buffer_access_flags);
  }

  void SceneGLType<met::Spec>::update(const Scene &scene) {
    met_trace_full();

    const auto &illuminants = scene.resources.illuminants;
    guard(illuminants);
    // fmt::print("Type updated: {}\n", typeid(decltype(illm)::value_type).name());
    
    for (uint i = 0; i < illuminants.size(); ++i) {
      const auto &[value, state] = illuminants[i];
      guard_continue(state);
      spec_buffer_map[i] = value;  
    }

    spec_buffer.flush();
    spec_texture.set(spec_buffer);
  }

  SceneGLType<met::CMFS>::SceneGLType() {
    met_trace_full();
    auto n_layers   = std::min<uint>(gl::state::get_variable_int(gl::VariableName::eMaxArrayTextureLayers), max_supported_constraints);
    cmfs_texture    = {{ .size = { wavelength_samples, n_layers } }};
    cmfs_buffer     = {{ .size = cmfs_texture.size().prod() * sizeof(eig::Array3f), .flags = buffer_create_flags }};
    cmfs_buffer_map = cmfs_buffer.map_as<CMFS>(buffer_access_flags);
  }

  void SceneGLType<met::CMFS>::update(const Scene &scene) {
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

  SceneGLType<met::ColorSystem>::SceneGLType() {
    met_trace_full();
    // ...
  }

  void SceneGLType<met::ColorSystem>::update(const Scene &scene) {
    met_trace_full();

    const auto &colr_systems = scene.components.colr_systems;
    guard(colr_systems                 ||
          scene.components.emitters    ||
          scene.resources.observers    || 
          scene.resources.illuminants  );
    
    // Get indices of active emitter data
    auto active_emitters = scene.components.emitters
                         | vws::filter([](const auto &comp) { return comp.value.is_active; });
    auto illuminants = active_emitters
                     | vws::transform([ ](const auto &comp) { return comp.value.illuminant_i;      })
                     | vws::transform([&](uint i) { return scene.resources.illuminants[i].value(); })
                     | rng::to<std::vector>();
    auto illuminant_s = active_emitters
                      | vws::transform([](const auto &comp) { return comp.value.illuminant_scale; })
                      | rng::to<std::vector>();
    
    // Generate sampling distribution over emitters
    Spec d = 1.f;
    {
      if (!illuminants.empty()) {
        d = 0.f;
        for (uint i = 0; i < illuminants.size(); ++i)
          d += illuminants[i] * illuminant_s[i];
      }
      d /= d.maxCoeff();
    }

    // Multiply by sensor response distribution's Y-curve
    // which we'll generate first
    {
      CMFS cmfs = scene.resources.observers[scene.components.observer_i].value();
      d *= cmfs.array().rowwise().sum();
    }

    // Add defensive sampling for very small wavelength values
    {
      d += (Spec(1) - d) * 0.01f;
    }

    wavelength_distr        = d;
    wavelength_distr_buffer = Distribution(cnt_span<float>(wavelength_distr)).to_buffer_std140();
  }
} // namespace met::detail