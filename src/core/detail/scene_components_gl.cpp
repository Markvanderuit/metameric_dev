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

  // Helper method to pack BVH node data tightly
  GLPacking<met::Mesh>::NodePack pack(const BVH::Node &node) {
    // Obtain a merger of the child bounding boxes
    constexpr auto merge = [](const BVH::AABB &a, const BVH::AABB &b) -> BVH::AABB {
      return { .minb = a.minb.cwiseMin(b.minb), .maxb = a.maxb.cwiseMax(b.maxb) };
    };
    auto aabb = rng::fold_left_first(node.child_aabb.begin(), 
                                     node.child_aabb.begin() + node.size(), merge).value();

    GLPacking<met::Mesh>::NodePack p;

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

  GLPacking<met::Object>::GLPacking() {
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
    m_buffer_map_size = &map->n;
    m_buffer_map_data = map->data;
  }
  
  void GLPacking<met::Object>::update(std::span<const detail::Component<met::Object>> objects, const Scene &scene) {
    met_trace_full();

    guard(!objects.empty());
    guard(scene.components.objects);
    // fmt::print("Type updated: {}\n", typeid(decltype(objects)::value_type).name());

    // Set appropriate object count, then flush change to buffer
    *m_buffer_map_size = static_cast<uint>(objects.size());
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

      m_buffer_map_data[i] = {
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

  GLPacking<met::Uplifting>::GLPacking() {
    met_trace_full();
    
    const uint requested_layers = max_supported_constraints * max_supported_upliftings;

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
    texture_barycentrics = {{ .levels  = 1, .padding = 0 }};
    texture_coefficients = {{ .levels  = 1, .padding = 0 }};

    // Initialize warped phase data for spectral MESE method
    auto warp_data = generate_warped_phase();
    texture_warp = {{ .size = { static_cast<uint>(warp_data.size()) },
                      .data = cnt_span<const float>(warp_data)      }};
  }

  void GLPacking<met::Uplifting>::update(std::span<const detail::Component<met::Uplifting>> upliftings, const Scene &scene) {
    // Get relevant resources
    const auto &e_objects  = scene.components.objects;
    const auto &e_images   = scene.resources.images;
    const auto &e_settings = scene.components.settings.value;
    
    // Barycentric texture has not been touched yet
    scene.components.upliftings.gl.texture_barycentrics.set_invalitated(false);
    scene.components.upliftings.gl.texture_coefficients.set_invalitated(false);

    // Only rebuild if there are upliftings and objects
    guard(!upliftings.empty() && !e_objects.empty());
    guard(scene.components.upliftings                  || 
          scene.components.objects                     ||
          scene.components.settings.state.texture_size ||
          !texture_barycentrics.texture().is_init()    ||
          !texture_barycentrics.buffer().is_init()     );
    
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
    bool is_exact_fit = rng::equal(inputs, texture_barycentrics.patches(),
      eig::safe_approx_compare<eig::Array2u>, {}, &detail::TextureAtlasBase::PatchLayout::size);

    // Regenerate atlas if inputs don't match the atlas' current layout
    // Note; barycentric weights will need a full rebuild, which is detected
    //       by the nr. of objects changing or the texture setting changing. A bit spaghetti.
    texture_barycentrics.resize(inputs);
    texture_coefficients.resize(inputs);
    if (texture_barycentrics.is_invalitated()) {
      // The barycentric texture was re-allocated, which means underlying memory was all invalidated.
      // So in a case of really bad spaghetti-code, we force object-dependent parts of the pipeline 
      // to rerun here. But uhh, code smell much?
      auto &e_scene = const_cast<Scene &>(scene);
      e_scene.components.objects.set_mutated(true);

      fmt::print("Rebuilt texture atlases\n");
      for (const auto &patch : texture_barycentrics.patches()) {
        fmt::print("\toffs = {}, size = {}, uv0 = {}, uv1 = {}\n", patch.offs, patch.size, patch.uv0, patch.uv1);
      }
    }
  }

  GLPacking<met::Emitter>::GLPacking() {
    met_trace_full();

    // Uniform layout which includes nr. of active components
    struct EmitterUniformBufferLayout {
      alignas(4)   uint n;
      EmitterInfoLayout data[max_supported_objects];
    };
    
    // Preallocate up to a number of meshes
    emitter_info = {{ .size  = sizeof(EmitterUniformBufferLayout),
                      .flags = buffer_create_flags }};

    // Obtain writeable, flushable mapping of nr. of components and individual data
    auto *map = emitter_info.map_as<EmitterUniformBufferLayout>(buffer_access_flags).data();
    m_buffer_map_size = &map->n;
    m_buffer_map_data = map->data;
  }

  void GLPacking<met::Emitter>::update(std::span<const detail::Component<met::Emitter>> emitters, const Scene &scene) {
    met_trace_full();

    guard(!emitters.empty());
    guard(scene.components.emitters);

    // Set appropriate component count, then flush change to buffer
    *m_buffer_map_size = static_cast<uint>(emitters.size());
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
      
      m_buffer_map_data[i] = {
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
  }

  GLPacking<met::Mesh>::GLPacking() {
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

  void GLPacking<met::Mesh>::update(std::span<const detail::Resource<met::Mesh>> meshes, const Scene &scene) {
    met_trace_full();

    guard(!meshes.empty());
    guard(scene.resources.meshes);
    // fmt::print("Type updated: {}\n", typeid(decltype(meshes)::value_type).name());

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
      m_meshes[i]   = simplified_mesh<met::Mesh>(value, 16384, 1e-3);
      txuvs[i]      = parameterize_mesh<met::Mesh>(m_meshes[i]);
      transforms[i] = unitize_mesh<met::Mesh>(m_meshes[i]);
      m_bvhs[i]     = create_bvh({ .mesh = m_meshes[i], .n_node_children = 8, .n_leaf_children = 3 });
            
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

  void GLPacking<met::Image>::update(std::span<const detail::Resource<met::Image>> images, const Scene &scene) {
    met_trace_full();
    
    guard(!images.empty());
    guard(scene.resources.images || scene.components.settings.state.texture_size);
    // fmt::print("Type updated: {}\n", typeid(decltype(images)::value_type).name());

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
    auto n_layers   = std::min<uint>(gl::state::get_variable_int(gl::VariableName::eMaxArrayTextureLayers), max_supported_constraints);
    spec_texture    = {{ .size = { wavelength_samples, n_layers } }};
    spec_buffer     = {{ .size = spec_texture.size().prod() * sizeof(float), .flags = buffer_create_flags }};
    spec_buffer_map = spec_buffer.map_as<Spec>(buffer_access_flags);
  }

  void GLPacking<met::Spec>::update(std::span<const detail::Resource<met::Spec>> illm, const Scene &scene) {
    met_trace_full();

    guard(scene.resources.illuminants);
    // fmt::print("Type updated: {}\n", typeid(decltype(illm)::value_type).name());
    
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
    auto n_layers   = std::min<uint>(gl::state::get_variable_int(gl::VariableName::eMaxArrayTextureLayers), max_supported_constraints);
    cmfs_texture    = {{ .size = { wavelength_samples, n_layers } }};
    cmfs_buffer     = {{ .size = cmfs_texture.size().prod() * sizeof(eig::Array3f), .flags = buffer_create_flags }};
    cmfs_buffer_map = cmfs_buffer.map_as<CMFS>(buffer_access_flags);
  }

  void GLPacking<met::CMFS>::update(std::span<const detail::Resource<met::CMFS>> cmfs, const Scene &scene) {
    met_trace_full();

    guard(scene.resources.observers);
    // fmt::print("Type updated: {}\n", typeid( decltype(cmfs)::value_type).name());
    
    // Whitepoint for normalization
    // Spec illuminant = models::emitter_cie_d65;

    for (uint i = 0; i < cmfs.size(); ++i) {
      const auto &[value, state] = cmfs[i];
      guard_continue(state);

      // Premultiply with RGB and normalize s.t. a unit spectrum has 1 luminance
      ColrSystem csys = { .cmfs = value, .illuminant = Spec(1)  };
      CMFS to_rgb = csys.finalize();
      cmfs_buffer_map[i] = to_rgb.transpose().reshaped(wavelength_samples, 3);
    }

    cmfs_buffer.flush();
    cmfs_texture.set(cmfs_buffer);
  }

  GLPacking<met::ColorSystem>::GLPacking() {
    met_trace_full();
    // ...
  }

  void GLPacking<met::ColorSystem>::update(std::span<const detail::Component<met::ColorSystem>>, const Scene &scene) {
    met_trace_full();

    guard(scene.components.colr_systems ||
          scene.components.emitters     ||
          scene.resources.observers     || 
          scene.resources.illuminants   );
    
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