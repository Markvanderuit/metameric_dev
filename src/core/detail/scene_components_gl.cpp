#include <metameric/core/detail/scene_components_gl.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/scene.hpp>

namespace met::detail {
  constexpr static auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr static auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  // Packed vertex struct data
  struct VertexPack {
    uint p0; // unorm, 2x16
    uint p1; // unorm, 1x16 + padding 1x16
    uint n;  // snorm, 2x16
    uint tx; // unorm, 2x16
  };
  static_assert(sizeof(VertexPack) == 16);

  // Packed primitive struct data
  struct PrimPack {
    VertexPack v0, v1, v2;
  };
  static_assert(sizeof(PrimPack) == 48);

  // Packed BVH struct data
  struct NodePack {
    uint aabb_pack_0;                 // lo.x, lo.y
    uint aabb_pack_1;                 // hi.x, hi.y
    uint aabb_pack_2;                 // lo.z, hi.z
    uint data_pack;                   // leaf | size | offs
    std::array<uint, 8> child_pack_0; // per child: lo.x | lo.y | hi.x | hi.y
    std::array<uint, 4> child_pack_1; // per child: lo.z | hi.z
  };
  static_assert(sizeof(NodePack) == 64);

  // Helper method to pack vertex data tightly
  VertexPack pack(const eig::Array3f &p, const eig::Array3f &n, const eig::Array2f &tx) {
    auto tx_ = tx.unaryExpr([](float f) {
      int i = static_cast<int>(f);
      if (i % 2) f = 1.f - (f - i);
      else       f = f - i;
      return f;
    }).eval();
    return VertexPack {
      .p0 = pack_unorm_2x16({ p.x(), p.y() }),
      .p1 = pack_unorm_2x16({ p.z(), 0.f   }),
      .n  = pack_snorm_2x16(pack_snorm_2x32_octagonal(n)),
      .tx = pack_unorm_2x16(tx_)
    };
  }

  // Helper method to pack BVH node data tightly
  NodePack pack(const BVH::Node &node) {
    // Obtain a merger of the child bounding boxes
    constexpr auto merge = [](const BVH::AABB &a, const BVH::AABB &b) -> BVH::AABB {
      return { .minb = a.minb.cwiseMin(b.minb), .maxb = a.maxb.cwiseMax(b.maxb) };
    };
    auto aabb = rng::fold_left_first(node.child_aabb.begin(), 
                                     node.child_aabb.begin() + node.size(), merge).value();

    NodePack p;

    // 3xu32 packs AABB lo, ex
    auto b_lo_in = aabb.minb;
    auto b_ex_in = (aabb.maxb - aabb.minb).eval();
    p.aabb_pack_0 = pack_unorm_2x16_floor({ b_lo_in.x(), b_lo_in.y() });
    p.aabb_pack_1 = pack_unorm_2x16_ceil ({ b_ex_in.x(), b_ex_in.y() });
    p.aabb_pack_2 = pack_unorm_2x16_floor({ b_lo_in.z(), 0 }) 
                  | pack_unorm_2x16_ceil ({ 0, b_ex_in.z() });

    // 1xu32 packs node type, child offset, child count
    p.data_pack = node.offs_data | (node.size_data << 27);

    // Child AABBs are packed in 6 bytes per child
    p.child_pack_0.fill(0);
    p.child_pack_1.fill(0);
    for (uint i = 0; i < node.size(); ++i) {
      auto b_lo_safe = ((node.child_aabb[i].minb - b_lo_in) / b_ex_in).eval();
      auto b_hi_safe = ((node.child_aabb[i].maxb - b_lo_in) / b_ex_in).eval();
      auto pack_0 = pack_unorm_4x8_floor((eig::Array4f() << b_lo_safe.head<2>(), 0, 0).finished())
                  | pack_unorm_4x8_ceil ((eig::Array4f() << 0, 0, b_hi_safe.head<2>()).finished());
      auto pack_1 = pack_unorm_4x8_floor((eig::Array4f() << b_lo_safe.z(), 0, 0, 0).finished())
                  | pack_unorm_4x8_ceil ((eig::Array4f() << 0, b_hi_safe.z(), 0, 0).finished());
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

  GLPacking<met::Object>::GLPacking() {
    met_trace_full();

    // Mesh uniform layout which includes nr. of active meshes
    struct ObjectUniformBufferLayout {
      alignas(4) uint n;
      ObjectInfoLayout data[max_supported_objects];
    };

    // Preallocate up to a number of meshes
    object_info = {{ .size  = sizeof(ObjectUniformBufferLayout),
                     .flags = buffer_create_flags }};

    // Obtain writeable, flushable mapping of nr. of meshes and individual mesh data
    auto *map = object_info.map_as<ObjectUniformBufferLayout>(buffer_access_flags).data();
    m_buffer_map_size = &map->n;
    m_buffer_map_data = map->data;
  }
  
  void GLPacking<met::Object>::update(std::span<const detail::Component<met::Object>> objects, const Scene &scene) {
    met_trace_full();

    guard(!objects.empty());
    guard(scene.components.objects);
    fmt::print("Type updated: {}\n", typeid(decltype(objects)::value_type).name());

    // Set appropriate object count, then flush change to buffer
    *m_buffer_map_size = static_cast<uint>(objects.size());
    object_info.flush(sizeof(uint));

    // Write updated objects to mapping
    for (uint i = 0; i < objects.size(); ++i) {
      const auto &[object, state] = objects[i];
      guard_continue(state);
      
      // Get relevant texture info
      bool is_albedo_sampled = object.diffuse.index() != 0;

      m_buffer_map_data[i] = {
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

      // Flush change to buffer; most changes to objects are local,
      // so we flush specific regions instead of the whole
      object_info.flush(sizeof(ObjectInfoLayout), 
                        sizeof(ObjectInfoLayout) * i + sizeof(uint) * 4);
    } // for (uint i)
  }

  /* uint GLPacking<met::Uplifting>::get_texture_offset(uint uplifting_i) {
    met_trace();

    uint supported_layers = 
      std::min(gl::state::get_variable_int(gl::VariableName::eMaxArrayTextureLayers), 2048);
    uint layer_size = supported_layers / max_supported_upliftings;
    
    return layer_size * uplifting_i;
  } */

  GLPacking<met::Uplifting>::GLPacking() {
    met_trace_full();
    
    const uint requested_layers = max_supported_spectra * max_supported_upliftings;

    // Ensure nr. of allocated layers remains below what the device supports
    const uint supported_layers = 
      std::min(gl::state::get_variable_int(gl::VariableName::eMaxArrayTextureLayers), 2048);
    debug::check_expr(supported_layers >= requested_layers,
      fmt::format("This OpenGL device supports texture arrays up to {} layers,\
                   \nbut {} layers were requested for GLPacking<Uplifting>!",
                   supported_layers, requested_layers));    

    // Initialize array texture to hold packed spectrum data, for fast sampled
    // access during rendering
    texture_spectra = {{ .size  = { wavelength_samples, requested_layers } }};

    // Initialize texture atlas to hold per-object barycentric weights, for
    // fast access during rendering; this is resized when necessary in update()
    texture_weights = {{ .levels  = 1, .padding = 0 }};
  }

  void GLPacking<met::Uplifting>::update(std::span<const detail::Component<met::Uplifting>> upliftings, const Scene &scene) {
    // Get relevant resources
    const auto &e_objects  = scene.components.objects;
    const auto &e_images   = scene.resources.images;
    const auto &e_settings = scene.components.settings.value;
    
    // Barycentric texture has not been touched yet
    scene.components.upliftings.gl.texture_weights.set_invalitated(false);

    // Only rebuild if there are upliftings and objects
    guard(!upliftings.empty() && !e_objects.empty());
    guard(scene.components.upliftings                || 
          scene.components.objects                   ||
          scene.components.settings.state.texture_size);
    fmt::print("Type updated: {}\n", typeid(decltype(upliftings)::value_type).name());
    
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
    eig::Array2u clamped_4f = e_settings.apply_texture_size(maximal_4f);
    eig::Array2f scaled_4f  = clamped_4f.cast<float>() / maximal_4f.cast<float>();
    for (auto &input : inputs)
      input = (input.cast<float>() * scaled_4f).max(2.f).cast<uint>().eval();

    // Test if the necessitated inputs match exactly to the atlas' reserved patches
    bool is_exact_fit = rng::equal(inputs, texture_weights.patches(),
      eig::safe_approx_compare<eig::Array2u>, {}, &detail::TextureAtlasBase::PatchLayout::size);

    // Regenerate atlas if inputs don't match the atlas' current layout
    // Note; barycentric weights will need a full rebuild, which is detected
    //       by the nr. of objects changing or the texture setting changing. A bit spaghetti.
    texture_weights.resize(inputs);
    if (texture_weights.is_invalitated()) {
      // The barycentric texture was re-allocated, which means underlying memory was all invalidated.
      // So in a case of really bad spaghetti-code, we force object-dependent parts of the pipeline 
      // to rerun here. But uhh, code smell much?
      auto &e_scene = const_cast<Scene &>(scene);
      e_scene.components.objects.set_mutated(true);

      fmt::print("Rebuilt texture weights atlas\n");
      for (const auto &patch : texture_weights.patches()) {
        fmt::print("\toffs = {}, size = {}, uv0 = {}, uv1 = {}\n", patch.offs, patch.size, patch.uv0, patch.uv1);
      }
    }
  }

  /* void GLPacking<met::Emitter>::update(std::span<const detail::Component<met::Emitter>>, const Scene &scene) {
    // TODO 
  } */

  /* void GLPacking<met::ColorSystem>::update(std::span<const detail::Component<met::ColorSystem>>, const Scene &scene) {
    // TODO
  } */

  GLPacking<met::Mesh>::GLPacking() {
    met_trace_full();

    // Mesh uniform layout which includes nr. of active meshes
    struct MeshUniformBufferLayout {
      alignas(4) uint n;
      MeshInfoLayout data[max_supported_meshes];
    };

    // Preallocate up to a number of meshes
    mesh_info = {{ .size  = sizeof(MeshUniformBufferLayout),
                       .flags = buffer_create_flags }};

    // Obtain writeable, flushable mapping of nr. of meshes and individual mesh data
    auto *map = mesh_info.map_as<MeshUniformBufferLayout>(buffer_access_flags).data();
    m_buffer_layout_map_size = &map->n;
    m_buffer_layout_map_data = map->data;
  }

  void GLPacking<met::Mesh>::update(std::span<const detail::Resource<met::Mesh>> meshes, const Scene &scene) {
    met_trace_full();

    guard(!meshes.empty());
    guard(scene.resources.meshes);
    fmt::print("Type updated: {}\n", typeid(decltype(meshes)::value_type).name());

    // Resize cache vectors, which keep cleaned, simplified mesh data around 
    m_meshes.resize(meshes.size());
    m_bvhs.resize(meshes.size());

    // Generate cleaned, simplified mesh data
    #pragma omp parallel for
    for (int i = 0; i < meshes.size(); ++i) {
      const auto &[value, state] = meshes[i];
      guard_continue(state);

      // Simplified copy of mesh, inverse matrix to undo [0, 1] packing, and acceleration structure
      auto [copy, inv] = unitized_mesh<met::Mesh>(simplified_mesh<met::Mesh>(value, 65536, 1e-4));
      auto bvh         = create_bvh({ .mesh = copy, .n_node_children = 8, .n_leaf_children = 3 });

      // Store both processed mesh and bvh
      m_meshes[i]                = copy;
      m_bvhs[i]                  = bvh;
      m_buffer_layout_map_data[i].trf = inv;
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

      // Temporary pack data vectors
      std::vector<VertexPack>     verts_packed(verts_size.back() + verts_offs.back());
      std::vector<PrimPack>       prims_packed(elems_size.back() + elems_offs.back());
      std::vector<NodePack>       nodes_packed(nodes_size.back() + nodes_offs.back());
      std::vector<eig::Array3u>   elems_packed(elems_size.back() + elems_offs.back()); // Not packed
      std::vector<eig::AlArray3u> elems_packed_al(elems_packed.size());                // Not packed

      // Pack mesh/bvh data tightly into pack data vectors
      // #pragma omp parallel for
      for (int i = 0; i < meshes.size(); ++i) {
        const auto &bvh  = m_bvhs[i];
        const auto &mesh = m_meshes[i];

        // Pack vertex data tightly and copy to the correctly offset range
        // #pragma omp parallel for
        for (int j = 0; j < mesh.verts.size(); ++j) {
          verts_packed[verts_offs[i] + j] = pack(mesh.verts[j], mesh.norms[j], mesh.txuvs[j]);
        }

        // Pack node data tightly and copy to the correctly offset range
        // #pragma omp parallel for
        for (int j = 0; j < bvh.nodes.size(); ++j) {
          nodes_packed[nodes_offs[i] + j] = pack(bvh.nodes[j]);
        }

        // Pack primitive data tightly and copy to the correctly offset range
        // while scattering data into leaf node order for the BVH
        // #pragma omp parallel for
        for (int j = 0; j < bvh.prims.size(); ++j) {
          auto el = mesh.elems[bvh.prims[j]];
          prims_packed[elems_offs[i] + j] = {
            .v0 = pack(mesh.verts[el[0]], mesh.norms[el[0]], mesh.txuvs[el[0]]),
            .v1 = pack(mesh.verts[el[1]], mesh.norms[el[1]], mesh.txuvs[el[1]]),
            .v2 = pack(mesh.verts[el[2]], mesh.norms[el[2]], mesh.txuvs[el[2]])
          };
        }

        // Copy element indices and adjust to refer to the correctly offset range
        rng::transform(mesh.elems, elems_packed.begin() + elems_offs[i], 
          [o = verts_offs[i]](const auto &v) -> eig::AlArray3u { return v + o; });
      }

      // Keep around aligned element data for some fringe cases
      rng::copy(elems_packed, elems_packed_al.begin());

      // Copy data to buffers
      mesh_verts    = {{ .data = cnt_span<const std::byte>(verts_packed)    }};
      bvh_prims    = {{ .data = cnt_span<const std::byte>(prims_packed)    }};
      bvh_nodes    = {{ .data = cnt_span<const std::byte>(nodes_packed)    }};
      mesh_elems    = {{ .data = cnt_span<const std::byte>(elems_packed)    }};
      mesh_elems_al = {{ .data = cnt_span<const std::byte>(elems_packed_al) }};
    }

    // Define corresponding vertex array object and generate multidraw command info
    array = {{
      .buffers = {{ .buffer = &mesh_verts, .index = 0, .stride = sizeof(eig::Array4u)    }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e4 }},
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

  void GLPacking<met::Image>::update(std::span<const detail::Resource<met::Image>> images, const Scene &scene) {
    met_trace_full();
    
    guard(!images.empty());
    guard(scene.resources.images || scene.components.settings.state.texture_size);
    fmt::print("Type updated: {}\n", typeid(decltype(images)::value_type).name());

    // Get texture settings, which are relevant
    const auto &e_settings = scene.components.settings.value;

    // Keep track of which atlas' position a texture needs to be stuffed in
    std::vector<uint> indices(images.size(), std::numeric_limits<uint>::max());

    // Generate inputs for texture atlas generation
    std::vector<eig::Array2u> inputs_3f, inputs_1f;
    for (uint i = 0; i < images.size(); ++i) {
      const auto &[img, state] = images[i];
      bool is_3f = img.pixel_frmt() == Image::PixelFormat::eRGB;

      indices[i] = is_3f 
                 ? inputs_3f.size() 
                 : inputs_1f.size();
      
      if (is_3f) inputs_3f.push_back(img.size());
      else       inputs_1f.push_back(img.size());
    } // for (uint i)

    // Determine maximum texture sizes, and texture scaling necessary to uphold size settings
    eig::Array2u maximal_3f = rng::fold_left(inputs_3f, eig::Array2u(0), 
      [](auto a, auto b) { return a.cwiseMax(b).eval(); });
    eig::Array2u maximal_1f = rng::fold_left(inputs_1f, eig::Array2u(0), 
      [](auto a, auto b) { return a.cwiseMax(b).eval(); });
    eig::Array2u clamped_3f = e_settings.apply_texture_size(maximal_3f);
    eig::Array2u clamped_1f = e_settings.apply_texture_size(maximal_1f);
    eig::Array2f scaled_3f  = clamped_3f.cast<float>() / maximal_3f.cast<float>();
    eig::Array2f scaled_1f  = clamped_1f.cast<float>() / maximal_1f.cast<float>();

    // Scale input sizes by appropriate texture scaling
    for (auto &input : inputs_3f)
      input = (input.cast<float>() * scaled_3f).max(1.f).cast<uint>().eval();
    for (auto &input : inputs_1f)
      input = (input.cast<float>() * scaled_1f).max(1.f).cast<uint>().eval();

    // Rebuild texture atlases with mips
    texture_atlas_3f = {{ .sizes = inputs_3f, .levels = 1 + clamped_3f.log2().maxCoeff() }};
    texture_atlas_1f = {{ .sizes = inputs_1f, .levels = 1 + clamped_1f.log2().maxCoeff() }};

    std::vector<TextureInfoLayout> info(images.size());
    for (uint i = 0; i < images.size(); ++i) {
      const auto &[img, state] = images[i];

      // Load patch from appropriate atlas (3f or 1f)
      bool is_3f = img.pixel_frmt() == Image::PixelFormat::eRGB;
      auto size  = is_3f ? texture_atlas_3f.capacity() 
                         : texture_atlas_1f.capacity();
      auto resrv = is_3f ? texture_atlas_3f.patch(indices[i])
                         : texture_atlas_1f.patch(indices[i]);

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
        texture_atlas_3f.texture().set(imgf.data<float>(), 0,
          { resrv.size.x(), resrv.size.y(), 1             },
          { resrv.offs.x(), resrv.offs.y(), resrv.layer_i });
      } else {
        texture_atlas_1f.texture().set(imgf.data<float>(), 0,
          { resrv.size.x(), resrv.size.y(), 1             },
          { resrv.offs.x(), resrv.offs.y(), resrv.layer_i });
      }
    } // for (uint i)

    // Fill in mip data using OpenGL's base functions
    if (texture_atlas_3f.texture().is_init()) texture_atlas_3f.texture().generate_mipmaps();
    if (texture_atlas_1f.texture().is_init()) texture_atlas_1f.texture().generate_mipmaps();

    // Push layout data
    texture_info = {{ .data = cnt_span<const std::byte>(info) }};
  }

  GLPacking<met::Spec>::GLPacking() {
    met_trace_full();
    auto n_layers   = std::min<uint>(gl::state::get_variable_int(gl::VariableName::eMaxArrayTextureLayers), max_supported_spectra);
    spec_texture    = {{ .size = { wavelength_samples, n_layers } }};
    spec_buffer     = {{ .size = spec_texture.size().prod() * sizeof(float), .flags = buffer_create_flags }};
    spec_buffer_map = spec_buffer.map_as<Spec>(buffer_access_flags);
  }

  void GLPacking<met::Spec>::update(std::span<const detail::Resource<met::Spec>> illm, const Scene &scene) {
    met_trace_full();

    guard(scene.resources.illuminants);
    fmt::print("Type updated: {}\n", typeid(decltype(illm)::value_type).name());
    
    for (uint i = 0; i < illm.size(); ++i) {
      const auto &[value, state] = illm[i];
      guard_continue(state);
      spec_buffer_map[i] = value;  
    }

    spec_buffer.flush();
    spec_texture.set(spec_buffer);
  }

  GLPacking<met::CMFS>::GLPacking() {
    met_trace_full();
    auto n_layers   = std::min<uint>(gl::state::get_variable_int(gl::VariableName::eMaxArrayTextureLayers), max_supported_spectra);
    cmfs_texture    = {{ .size = { wavelength_samples, n_layers } }};
    cmfs_buffer     = {{ .size = cmfs_texture.size().prod() * sizeof(eig::Array3f), .flags = buffer_create_flags }};
    cmfs_buffer_map = cmfs_buffer.map_as<CMFS>(buffer_access_flags);
  }

  void GLPacking<met::CMFS>::update(std::span<const detail::Resource<met::CMFS>> cmfs, const Scene &scene) {
    met_trace_full();

    guard(scene.resources.observers);
    fmt::print("Type updated: {}\n", typeid( decltype(cmfs)::value_type).name());
    
    for (uint i = 0; i < cmfs.size(); ++i) {
      const auto &[value, state] = cmfs[i];
      guard_continue(state);

      // Premultiply with RGB
      CMFS to_xyz = (value.array()           /* * illuminant */ * wavelength_ssize)
                  / (value.array().col(1)    /* * illuminant */ * wavelength_ssize).sum();
      CMFS to_rgb = (models::xyz_to_srgb_transform * to_xyz.matrix().transpose()).transpose();
      
      cmfs_buffer_map[i] = to_rgb.transpose().reshaped(wavelength_samples, 3);
    }

    cmfs_buffer.flush();
    cmfs_texture.set(cmfs_buffer);
  }
}