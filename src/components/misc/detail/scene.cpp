#include <metameric/components/misc/detail/scene.hpp>
#include <metameric/core/packing.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/embree.hpp>
#include <bit>
#include <algorithm>
#include <deque>
#include <execution>
#include <ranges>
#include <vector>

namespace met::detail {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  struct MeshPacking {
    using MeshInfo = RTMeshData::MeshInfo;

    // Packed vertex struct data
    struct VertexPack {
      uint p0; // unorm, 2x16
      uint p1; // unorm, 1x16 + padding 1x16
      uint n;  // snorm, 2x16
      uint tx; // unorm, 2x16
    };

  public:
    std::vector<MeshInfo>       info;
    std::vector<VertexPack>     verts;
    std::vector<eig::Array3u>   elems;
    std::vector<eig::AlArray3u> elems_al;

  public:
    // Default constr.
    MeshPacking() = default;

    // Constr. over a set of meshes
    MeshPacking(std::span<const Mesh> meshes) {
      std::vector<uint> verts_size(meshes.size()), // Nr. of verts in mesh i
                        elems_size(meshes.size()), // Nr. of elems in mesh i
                        verts_offs(meshes.size()), // Nr. of verts prior to mesh i
                        elems_offs(meshes.size()); // Nr. of elems prior to mesh i

      // Fill in vert/elem sizes and scans
      rng::transform(meshes, verts_size.begin(), [](const Mesh &m) -> uint { return m.verts.size(); });
      rng::transform(meshes, elems_size.begin(), [](const Mesh &m) -> uint { return m.elems.size(); });
      std::exclusive_scan(range_iter(verts_size), verts_offs.begin(), 0u);
      std::exclusive_scan(range_iter(elems_size), elems_offs.begin(), 0u);

      // Assign sizes/offsets into info object
      info.resize(meshes.size());
      for (int i = 0; i < meshes.size(); ++i) {
        info[i] = {
          .verts_offs = verts_offs[i], .verts_size = verts_size[i],
          .elems_offs = elems_offs[i], .elems_size = elems_size[i],
        };
      }

      // Resize packed data buffers to total vertex/element lengths across all meshes
      verts.resize(info.back().verts_size + info.back().verts_offs);
      elems.resize(info.back().elems_size + info.back().elems_offs);
      elems_al.resize(info.back().elems_size + info.back().elems_offs);

      // Fill packed data buffers
      #pragma omp parallel for
      for (int i = 0; i < meshes.size(); ++i) {
        // Fit mesh to [0, 1]
        auto [mesh, inv] = unitized_mesh<Mesh>(meshes[i]);

        // Store inverse to undo [0, 1] packing
        info[i].trf = inv;

        // Pack vertex data tightly and copy to the correctly offset range
        #pragma omp parallel for
        for (int j = 0; j < mesh.verts.size(); ++j) {
          // Get uv's, pre-apply a mirrored repeat for some meshes
          eig::Array2f txuv = mesh.has_txuvs() ? mesh.txuvs[j].unaryExpr([](float f) {
            int i = static_cast<int>(f);
            if (i % 2) f = 1.f - (f - i);
            else       f = f - i;
            return f;
          }).eval() : 0.f;

          VertexPack pack = {
            .p0 = pack_unorm_2x16({ mesh.verts[j].x(), mesh.verts[j].y() }),
            .p1 = pack_unorm_2x16({ mesh.verts[j].z(), 0.f               }),
            .n  = pack_snorm_2x16(pack_snorm_2x32_octagonal(mesh.norms[j])),
            .tx = pack_unorm_2x16(txuv)
          };

          verts[info[i].verts_offs + j] = pack;
        } // for (int j)
        
        // Copy element indices and adjust to refer to the offset range as well
        rng::transform(mesh.elems, elems.begin() + info[i].elems_offs, 
          [o = info[i].verts_offs](const auto &v) { return (v + o).eval(); });
      }

      // Keep around aligned element data for some fringe cases
      rng::copy(elems, elems_al.begin());
    }
  };


  RTTextureData::RTTextureData(const Scene &scene) {
    met_trace();
    // Initialize on first run
    // update(scene);
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

    // Keep track of which atlas' position a texture needs to be stuffed in
    std::vector<uint> indices(e_images.size(), std::numeric_limits<uint>::max());\

    // Generate inputs for texture atlas
    std::vector<eig::Array2u> inputs_3f, inputs_1f;
    for (uint i = 0; i < e_images.size(); ++i) {
      const auto &img = e_images[i].value();
      bool is_3f = img.pixel_frmt() == Image::PixelFormat::eRGB;

      indices[i] = is_3f ? inputs_3f.size() : inputs_1f.size();
      
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

      // Load patch from appropriate atlas (3f or 1f)
      bool is_3f = img.pixel_frmt() == Image::PixelFormat::eRGB;
      auto size  = is_3f ? atlas_3f.capacity() 
                         : atlas_1f.capacity();
      auto resrv = is_3f ? atlas_3f.patch(indices[i])
                         : atlas_1f.patch(indices[i]);

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

  RTMeshData::RTMeshData(const Scene &scene) {
    met_trace();
    // Initialize on first run
    // update(scene);
  }

  bool RTMeshData::is_stale(const Scene &scene) const {
    met_trace();
    return scene.resources.meshes.is_mutated();
  }

  void RTMeshData::update(const Scene &scene) {
    met_trace_full();

    // Get external resources
    const auto &e_meshes = scene.resources.meshes;
    guard(!e_meshes.empty());

    // Generate a cleaned, simplified version of each scene mesh
    std::vector<Mesh> meshes(e_meshes.size());
    std::transform(std::execution::par_unseq, range_iter(e_meshes), meshes.begin(), [](const auto &m) { 
      Mesh copy = m.value();
      simplify_mesh(copy, 100'000, 1e-2);
      optimize_mesh(copy);
      return copy;
    });

    // Pack meshes together into one object
    auto mesh_pack = MeshPacking(meshes);

    // Push layout/data to GL buffers
    info_gl  = {{ .data = cnt_span<const std::byte>(mesh_pack.info)     }};
    verts    = {{ .data = cnt_span<const std::byte>(mesh_pack.verts)    }};
    elems    = {{ .data = cnt_span<const std::byte>(mesh_pack.elems)    }};
    elems_al = {{ .data = cnt_span<const std::byte>(mesh_pack.elems_al) }};
    
    // Define corresponding vertex array object
    array = {{
      .buffers = {{ .buffer = &verts, .index = 0, .stride = sizeof(eig::Array4u)           }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e4 }},
      .elements = &elems
    }};

    // Copy over info object for later access
    info = std::move(mesh_pack.info);
  }

  
  RTBVHData::RTBVHData(const Scene &) {
    met_trace();
    // ...
  }

  bool RTBVHData::is_stale(const Scene &scene) const {
    met_trace();
    return scene.resources.meshes.is_mutated();
  }

  void RTBVHData::update(const Scene &scene) {
    met_trace_full();

    // Get external resources
    const auto &e_meshes = scene.resources.meshes;
    guard(!e_meshes.empty());

    // Generate a simplified representation of each scene mesh
    std::vector<Mesh> bl_meshes(e_meshes.size());
    std::transform(std::execution::par_unseq, range_iter(e_meshes), bl_meshes.begin(), [](const auto &m) { 
        Mesh copy = m.value();
        simplify_mesh(copy, 128, 1e-2);
        // renormalize_mesh(copy);
        return copy;
    });

    // Generate an acceleration structure over each scene mesh
    std::vector<BVH> bl_bvhs(e_meshes.size());
    rng::transform(bl_meshes, bl_bvhs.begin(), [](const auto &mesh) { 
      return create_bvh({ 
        .mesh            = mesh,
        .n_node_children = 8, // 2, 4, 8
        .n_leaf_children = 8,
      });
    });

    // Gather node/prim lengths and offsets per bvh
    std::vector<uint> nodes_size, prims_size, nodes_offs, prims_offs;
    std::transform(range_iter(bl_bvhs), std::back_inserter(nodes_size), 
      [](const auto &m) { return static_cast<uint>(m.nodes.size()); });
    std::exclusive_scan(range_iter(nodes_size), std::back_inserter(nodes_offs), 0u);
    std::transform(range_iter(bl_bvhs), std::back_inserter(prims_size), 
      [](const auto &m) { return static_cast<uint>(m.prims.size()); });
    std::exclusive_scan(range_iter(prims_size), std::back_inserter(prims_offs), 0u);

    // Total nodes/prims lengths across all bvhs
    uint n_nodes = nodes_size.at(nodes_size.size() - 1) + nodes_offs.at(nodes_offs.size() - 1);
    uint n_prims = prims_size.at(prims_size.size() - 1) + prims_offs.at(prims_offs.size() - 1);

    // Holders for packed data of all bvhs
    std::vector<BVH::Node> packed_nodes(n_nodes);
    std::vector<uint>      packed_prims(n_prims);
    std::vector<BVHInfo>   info(bl_bvhs.size());
    
    // Fill packed and layout data
    #pragma omp parallel for
    for (int i = 0; i < bl_bvhs.size(); ++i) {
      const auto &bvh = bl_bvhs[i];

      // Copy over packed data to the correctly offset range;
      rng::copy(bvh.nodes, packed_nodes.begin() + nodes_offs[i]);
      rng::copy(bvh.prims, packed_prims.begin() + prims_offs[i]);

      info[i] = {
        .nodes_offs = nodes_offs[i], .nodes_size = nodes_size[i],
        .prims_offs = prims_offs[i], .prims_size = prims_size[i],
      };
    } // for (int i)

    // Push layout/data to GL buffers
    bl_bvh_info  = {{ .data = cnt_span<const std::byte>(info)         }};
    bl_bvh_nodes = {{ .data = cnt_span<const std::byte>(packed_nodes) }};
    bl_bvh_prims = {{ .data = cnt_span<const std::byte>(packed_prims) }};

    fmt::print("BVH pack sizes: {} nodes, {} prims, {} and {} bytes\n", 
      bl_bvh_nodes.size() / sizeof(BVH::Node), 
      bl_bvh_prims.size() / sizeof(uint),
      bl_bvh_nodes.size(), 
      bl_bvh_prims.size()
    );
  }

  RTObjectData::RTObjectData(const Scene &scene) {
    met_trace();
    // Initialize on first run
    // update(scene);
  }

  bool RTObjectData::is_stale(const Scene &scene) const {
    met_trace();

    // Get shared resources
    const auto &e_objects  = scene.components.objects;
    const auto &e_images   = scene.resources.images;
    const auto &e_settings = scene.components.settings;
    
    // Accumulate reasons for returning
    return !info_gl.is_init() || e_objects.is_mutated();
  }
  
  void RTObjectData::update(const Scene &scene) {
      met_trace_full();
            
      // Get external resources
      const auto &e_objects  = scene.components.objects;
      const auto &e_images   = scene.resources.images;
      const auto &e_settings = scene.components.settings;

      guard(!e_objects.empty());

      // Initialize or resize object buffer to accomodate correct nr of objects
      bool handle_resize = false;
      if (!info_gl.is_init() || e_objects.size() != info.size()) {
        info.resize(e_objects.size());
        info_gl = {{ .size  = e_objects.size() * sizeof(ObjectInfo),
                     .flags = gl::BufferCreateFlags::eStorageDynamic }};

        handle_resize = true;
      }
      
      // Process updates to gl-side object info
      for (uint i = 0; i < e_objects.size(); ++i) {
        const auto &component = e_objects[i];
        const auto &object    = component.value;
        
        guard_continue(handle_resize || component.state.is_mutated());

        // Get relevant texture info
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

        // Note; should upgrade to mapped range
        info_gl.set(obj_span<const std::byte>(info[i]), sizeof(ObjectInfo), sizeof(ObjectInfo) * static_cast<size_t>(i));
      } // for (uint i)
  }

  RTUpliftingData::RTUpliftingData(const Scene &scene) {
    met_trace_full();

    // Fixed settings 
    constexpr uint max_upliftings = 8u;

    // Get external resources
    const auto &e_objects  = scene.components.objects;
    const auto &e_settings = scene.components.settings;
    const auto &e_images   = scene.resources.images;

    uint max_texture_layers = 
      std::min(gl::state::get_variable_int(gl::VariableName::eMaxArrayTextureLayers), 2048);
    uint layer_size = max_texture_layers / max_upliftings;

    // Initialize info buffer up to maximum nr of supported upliftings;
    // so no resizing will take place
    {
      // Hardcode up to the nr. of upliftings for now
      info.resize(max_upliftings, { 0, 0 });
      for (uint i = 0; i < max_upliftings; ++i) {
        info[i] = { .elem_offs = layer_size * i, .elem_size = layer_size };
      }

      info_gl = {{ .data  = cnt_span<const std::byte>(info), .flags = gl::BufferCreateFlags::eStorageDynamic }};
    }

    // Pre-allocated up to the maximum size necessary; as this  is actually a reasonable 2mb
    // so no resizing will take place
    {
      spectra_gl_texture = {{ .size  = { wavelength_samples, max_texture_layers } }};
      spectra_gl         = {{ .size  = max_texture_layers * sizeof(SpecPack), 
                              .flags = buffer_create_flags }};
      spectra_gl_mapping = spectra_gl.map_as<SpecPack>(buffer_access_flags);
    }
  }

  bool RTUpliftingData::is_stale(const Scene &scene) const {
    met_trace();
    return false;
  }

  void RTUpliftingData::update(const Scene &scene) {
    met_trace();
    // Never runs; is instead filled in by subsequent uplifting pipeline
  }

  RTObserverData::RTObserverData(const Scene &scene) {
    met_trace_full();
    
    constexpr static auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
    constexpr static auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;
    
    // Initialize buffers/textures for preallocated nr. of layers;
    // very unlikely to exceed this count
    uint n_layers =  std::min(gl::state::get_variable_int(gl::VariableName::eMaxArrayTextureLayers), 256);
    cmfs_gl_texture = {{ .size = { wavelength_samples, n_layers } }};
    cmfs_gl         = {{ .size = cmfs_gl_texture.size().prod() * sizeof(eig::Array3f), .flags = buffer_create_flags }};
    cmfs_gl_mapping = cmfs_gl.map_as<CMFS>(buffer_access_flags);
  }

  bool RTObserverData::is_stale(const Scene &scene) const {
    met_trace();
    return scene.resources.observers.is_mutated();
  }

  void RTObserverData::update(const Scene &scene) {
    met_trace_full();

    uint i = 0;
    for (const auto &[cmfs, state] : scene.resources.observers) {
      guard_continue(state);
      cmfs_gl_mapping[i] = cmfs.transpose().reshaped(wavelength_samples, 3).eval();
      i++;
    }
    cmfs_gl.flush();
    cmfs_gl_texture.set(cmfs_gl);
  }

  RTIlluminantData::RTIlluminantData(const Scene &scene) {
    met_trace_full();
    
    constexpr static auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
    constexpr static auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

    // Initialize buffers/textures for preallocated nr. of layers;
    // very unlikely to exceed this count
    uint n_layers =  std::min(gl::state::get_variable_int(gl::VariableName::eMaxArrayTextureLayers), 256);
    illm_gl_texture = {{ .size = { wavelength_samples, n_layers } }};
    illm_gl         = {{ .size = illm_gl_texture.size().prod() * sizeof(float), .flags = buffer_create_flags }};
    illm_gl_mapping = illm_gl.map_as<Spec>(buffer_access_flags);
  }

  bool RTIlluminantData::is_stale(const Scene &scene) const {
    met_trace();
    return scene.resources.illuminants.is_mutated();
  }

  void RTIlluminantData::update(const Scene &scene) {
    met_trace_full();

    uint i = 0;
    for (const auto &[illm, state] : scene.resources.illuminants) {
      guard_continue(state);
      illm_gl_mapping[i] = illm;
      i++;
    }
    illm_gl.flush();
    illm_gl_texture.set(illm_gl);
  }

  RTColorSystemData::RTColorSystemData(const Scene &scene) {
    met_trace_full();
    
    constexpr static auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
    constexpr static auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

    // Initialize buffers/textures for preallocated nr. of layers;
    // very unlikely to exceed this count
    uint n_layers =  std::min(gl::state::get_variable_int(gl::VariableName::eMaxArrayTextureLayers), 256);
    csys_gl_texture = {{ .size = { wavelength_samples, n_layers } }};
    csys_gl         = {{ .size = csys_gl_texture.size().prod() * sizeof(eig::Array3f), .flags = buffer_create_flags }};
    csys_gl_mapping = csys_gl.map_as<CMFS>(buffer_access_flags);
  }

  bool RTColorSystemData::is_stale(const Scene &scene) const {
    met_trace();
    return scene.resources.illuminants.is_mutated()
        || scene.resources.observers.is_mutated()
        || scene.components.colr_systems.is_mutated();
  }

  void RTColorSystemData::update(const Scene &scene) {
    met_trace_full();

    uint i = 0;
    for (const auto &[csys, state] : scene.components.colr_systems) {
      auto func = scene.get_csys(csys).finalize_direct();
      // Data is transposed and reshaped into a [wvls, 3]-shaped object for gpu-side layout
      csys_gl_mapping[i] = func.transpose().reshaped(wavelength_samples, 3);
      i++;
    }
    csys_gl.flush();
    csys_gl_texture.set(csys_gl);
  }
} // namespace met::detail