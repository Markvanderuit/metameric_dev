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
#include <metameric/core/ranges.hpp>

namespace met {
  bool Object::operator==(const Object &o) const {
    met_trace();

    guard(std::tie(is_active,   transform,   mesh_i,   uplifting_i,   brdf_type, absorption) == 
          std::tie(o.is_active, o.transform, o.mesh_i, o.uplifting_i, o.brdf_type, o.absorption), 
          false);
    guard(eta_minmax.isApprox(o.eta_minmax), false);

    guard(diffuse.index() == o.diffuse.index(), false);
    switch (diffuse.index()) {
      case 0: guard(std::get<Colr>(diffuse).isApprox(std::get<Colr>(o.diffuse)), false); break;
      case 1: guard(std::get<uint>(diffuse) == std::get<uint>(o.diffuse), false); break;
    }

    guard(metallic.index() == o.metallic.index(), false);
    switch (metallic.index()) {
      case 0: guard(std::get<float>(metallic) == std::get<float>(o.metallic), false); break;
      case 1: guard(std::get<uint>(metallic) == std::get<uint>(o.metallic), false); break;
    }

    guard(roughness.index() == o.roughness.index(), false);
    switch (roughness.index()) {
      case 0: guard(std::get<float>(roughness) == std::get<float>(o.roughness), false); break;
      case 1: guard(std::get<uint>(roughness) == std::get<uint>(o.roughness), false); break;
    }

    guard(normalmap == o.normalmap, false);
    if (normalmap)
      guard(*normalmap == *o.normalmap, false);
   
    return true;
  }

  namespace detail {
    // Helper to pack color/uint variant to a uvec2
    inline
    eig::Array2u pack_material_3f(const std::variant<Colr, uint> &v) {
      met_trace();
      std::array<uint, 2> u;
      if (v.index()) {
        u[0] = std::get<1>(v);
        u[1] = 0x00010000;
      } else {
        Colr c = std::get<0>(v);
        u[0] = detail::pack_half_2x16(c.head<2>());
        u[1] = detail::pack_half_2x16({ c.z(), 0 });
      }
      return { u[0], u[1] };
    }

    uint to_10b(const std::variant<float, uint> &v) {
      uint u;
      if (std::holds_alternative<float>(v)) {
        u = (0x1FFu & static_cast<uint>(std::round(std::clamp(std::get<float>(v), 0.f, 1.f) * 511.f)));
      } else {
        u = (0x1FFu & static_cast<uint>(std::get<uint>(v))) | 0x200u;
      }
      return u;
    }

    uint to_8b(float v) {
      return static_cast<uint>(v * 255.f);
    }

    // Helper to pack float/uint variant to a uint
    inline
    uint pack_material_1f(const std::variant<float, uint> &v) {
      met_trace();
      uint u;
      if (v.index()) {
        u = (0x0000FFFF & static_cast<ushort>(std::get<1>(v))) | 0x00010000;
      } else {
        u = (0x0000FFFF & detail::to_float16(std::get<0>(v)));
      }
      return u;
    }

    // Helper to pack uint optional
    inline
    uint pack_optional_1u(const std::optional<uint> &v) {
      met_trace();
      uint u = 0;
      if (v.has_value())
        u = (0x0FFFFFFF & static_cast<ushort>(*v)) | 0x10000000;
      return u;
    }

    SceneGLHandler<met::Object>::SceneGLHandler() {
      met_trace_full();

      // Allocate up to a number of objects and obtain writeable/flushable mapping
      std::tie(object_info, m_object_info_map) = gl::Buffer::make_flusheable_object<BufferLayout>();
    }

    void SceneGLHandler<met::Object>::update(const Scene &scene) {
      met_trace_full();

      // Destroy old sync object
      m_fence = { };

      // Get relevant resources
      const auto &objects  = scene.components.objects;
      const auto &images   = scene.resources.images;
      const auto &settings = scene.components.settings;

      // Skip entirely; no objects, delete buffer
      guard(!objects.empty());
      if (objects) {
        // Set object count
        m_object_info_map->n = static_cast<uint>(objects.size());
        
        // Set per-object data
        for (uint i = 0; i < objects.size(); ++i) {
          const auto &[object, state] = objects[i];
          guard_continue(state);
          
          // Get mesh transform, incorporate into gl-side object transform
          auto object_trf = object.transform.affine().matrix().eval();
          auto mesh_trf   = scene.resources.meshes.gl.mesh_cache[object.mesh_i].unit_trf;
          auto trf        = (object_trf * mesh_trf).eval();

          // Pack most brdf data together
          eig::Array2u brdf_data;
          brdf_data[0] = ((to_10b(object.metallic)                    & 0x03FFu)      )  // 10b for metallic
                       | ((to_10b(object.roughness)                   & 0x03FFu) << 10)  // 10b for roughness
                       | ((to_10b(object.transmission)                & 0x03FFu) << 20); // 10b for transmission
          brdf_data[1] = ((to_8b((object.eta_minmax.x() - 1.f) / 3.f) & 0x00FFu)      )  // 8b for eta (minimum)
                       | ((to_8b((object.eta_minmax.y() - 1.f) / 3.f) & 0x00FFu) <<  8)  // 8b for eta (maximum)
                       | ((detail::to_float16(object.absorption)      & 0xFFFFu) << 16); // 16b for fp16 absorption
                                 
          // Fill in object struct data
          m_object_info_map->data[i] = {  
            .trf   = trf,
            .flags = (object.is_active ? 0x80000000 : 0)
                   | ((static_cast<uint>(object.brdf_type) & 0x7) << 28)
                   | ((static_cast<uint>(object.mesh_i) & 0x0FFFFFFF))
          };
        } // for (uint i)
        
        // Write out changes to buffer
        object_info.flush(sizeof(eig::Array4u) + objects.size() * sizeof(BlockLayout));
        gl::sync::memory_barrier(gl::BarrierFlags::eBufferUpdate      |  
                                 gl::BarrierFlags::eUniformBuffer     |
                                 gl::BarrierFlags::eClientMappedBuffer);
      }

      // Flag that the atlas' internal texture has **not** been invalidated by internal resize
      if (texture_brdf.is_init())
        texture_brdf.set_invalitated(false);

      // Handle `texture_brdf` atlas resize
      if (objects || settings.state.texture_size || !texture_brdf.is_init()) {
        // First, ensure atlas exists for us to operate on
        if (!texture_brdf.is_init())
          texture_brdf = {{ .levels  = 1, .padding = 0 }};

        // Gather necessary texture sizes for each object
        // If the texture index was specified, we insert the texture size as an input
        // for the atlas. If a value was specified, we allocate a small patch
        // As we'd like to cram multiple brdf values into a single lookup, we will
        // take the larger size for each patch. Makes baking slightly more expensive
        std::vector<eig::Array2u> inputs(objects.size());
        rng::transform(objects, inputs.begin(), [&](const auto &object) -> eig::Array2u {
          auto metallic_size = object->metallic | visit {
            [&](uint  i) { return images.gl.m_texture_info_map->data[i].size; },
            [&](float f) { return eig::Array2u { 16, 16 }; },
          };
          auto roughness_size = object->roughness | visit {
            [&](uint  i) { return images.gl.m_texture_info_map->data[i].size; },
            [&](float f) { return eig::Array2u { 16, 16 }; },
          };
          auto normal_size = object->normalmap | visit {
            [&](uint i) { return images.gl.m_texture_info_map->data[i].size; },
            [&]()       { return eig::Array2u { 16, 16 }; }
          };
          return metallic_size.cwiseMax(roughness_size).cwiseMax(normal_size).eval();
        });

        // Scale atlas inputs to respect the maximum texture size set in Settings::texture_size
        eig::Array2u maximal = rng::fold_left(inputs, eig::Array2u(0), [](auto a, auto b) { return a.cwiseMax(b).eval(); });
        eig::Array2f scaled  = settings->apply_texture_size(maximal).cast<float>() / maximal.cast<float>();
        for (auto &input : inputs)
          input = (input.cast<float>() * scaled).max(2.f).cast<uint>().eval();
        
        // Regenerate atlas if inputs don't match the atlas' current layout
        // Note; barycentric weights will need a full rebuild, which is detected
        //       by the nr. of objects changing or the texture setting changing. A bit spaghetti-y :S
        texture_brdf.resize(inputs);
        if (texture_brdf.is_invalitated()) {
          // The barycentric texture was re-allocated, which means underlying memory was all invalidated.
          // So in a case of really bad spaghetti-code, we force object-dependent parts to update
          auto &e_scene = const_cast<Scene &>(scene);
          e_scene.components.objects.set_mutated(true);
        }
      }

      // Adjust nr. of ObjectData blocks up to or down to relevant size
      for (uint i = object_data.size(); i < scene.components.objects.size(); ++i)
        object_data.push_back({ scene, i });
      for (uint i = object_data.size(); i > scene.components.objects.size(); --i)
        object_data.pop_back();

      // Generate per-object packed brdf data
      for (auto &data : object_data)
        data.update(scene);

      // Generate sync object for gpu wait
      if (objects)
        m_fence = gl::sync::Fence(gl::sync::time_ns(1));
    }

    SceneGLHandler<met::Object>::ObjectData::ObjectData(const Scene &scene, uint object_i)
    : m_object_i(object_i) {
      met_trace_full();

      // Build shader in program cache, if it is not loaded already
      auto &cache = scene.m_cache_handle.getw<gl::ProgramCache>();
      std::tie(m_program_key, std::ignore) = cache.set({{ 
        .type       = gl::ShaderType::eCompute,
        .glsl_path  = "shaders/scene/bake_object_brdf.comp",
        .spirv_path = "shaders/scene/bake_object_brdf.comp.spv",
        .cross_path = "shaders/scene/bake_object_brdf.comp.json",
      }});

      // Initialize uniform buffers and writeable, flushable mappings
      std::tie(m_buffer, m_buffer_map) = gl::Buffer::make_flusheable_object<BlockLayout>();
      m_buffer_map->object_i = m_object_i;
      m_buffer.flush();

      // Linear texture sampler
      m_sampler = {{ .min_filter = gl::SamplerMinFilter::eLinear, 
                     .mag_filter = gl::SamplerMagFilter::eLinear }};
    }

    void SceneGLHandler<met::Object>::ObjectData::update(const Scene &scene) {
      met_trace_full();

      // Get handles to relevant scene data
      const auto &object   = scene.components.objects[m_object_i];
      const auto &settings = scene.components.settings;
      const auto &mesh     = scene.resources.meshes[object->mesh_i];

      // Find relevant patch in the texture atlas
      const auto &atlas = scene.components.objects.gl.texture_brdf;
      const auto &patch = atlas.patch(m_object_i);

      // We continue only after careful checking of internal state, as the bake
      // is relatively expensive and doesn't always need to happen. Careful in
      // this case means "ewwwwwww"
      bool is_active 
         = m_is_first_update            // First run, demands render anyways
        || atlas.is_invalitated()       // Texture atlas re-allocated, demands re-render
        || object.state.roughness       // Different albedo value set on object
        || object.state.metallic        // Different value set on object
        || object.state.normalmap       // Different value set on object
        || object.state.eta_minmax      // Different value set on object
        || object.state.mesh_i          // Different mesh attached to object
        || object.state.absorption      // Different value set on object
        || object.state.eta_minmax      // Different value set on object
        || scene.resources.meshes       // User loaded/deleted a mesh;
        || scene.resources.images       // User loaded/deleted an image;
        || settings.state.texture_size; // Texture size setting changed
      guard(is_active);
      fmt::print("Object: baking object {} brdf data ({}x{})\n", 
        m_object_i, patch.size.x(), patch.size.y());

      // Fill in block data
      *m_buffer_map = {
        .object_i                 = m_object_i,
        .object_metallic_data     = detail::pack_material_1f(object->metallic),
        .object_roughness_data    = detail::pack_material_1f(object->roughness),
        .object_transmission_data = detail::pack_material_1f(object->transmission),
        .object_albedo_data       = detail::pack_material_3f(object->diffuse),
        .object_normalmap_data    = detail::pack_optional_1u(object->normalmap),
        .object_misc_data         = ((to_8b((object->eta_minmax.x() - 1.f) / 3.f) & 0x00FFu)      ) // 8b for eta (minimum)
                                  | ((to_8b((object->eta_minmax.y() - 1.f) / 3.f) & 0x00FFu) <<  8) // 8b for eta (maximum)
                                  | ((detail::to_float16(object->absorption)      & 0xFFFFu) << 16) // 16b for fp16 
      };
      m_buffer.flush();

      // Get relevant program handle, bind, then bind resources to corresponding targets
      auto &cache = scene.m_cache_handle.getw<gl::ProgramCache>();
      auto &program = cache.at(m_program_key);
      program.bind();
      program.bind("b_buff_unif",       m_buffer);
      program.bind("b_buff_atlas",      atlas.buffer());
      program.bind("b_atlas",           atlas.texture());
      if (!scene.resources.images.empty()) {
        program.bind("b_buff_textures",  scene.resources.images.gl.texture_info);
        program.bind("b_txtr_3f",        scene.resources.images.gl.texture_atlas_3f.texture(), m_sampler);  
        program.bind("b_txtr_1f",        scene.resources.images.gl.texture_atlas_1f.texture(), m_sampler);  
      }

      // Insert relevant barriers
      gl::sync::memory_barrier(gl::BarrierFlags::eTextureFetch       |
                               gl::BarrierFlags::eClientMappedBuffer |
                               gl::BarrierFlags::eStorageBuffer      | 
                               gl::BarrierFlags::eUniformBuffer      );

      // Dispatch compute region of patch size
      auto dispatch_ndiv = ceil_div(patch.size, 16u);
      gl::dispatch_compute({ .groups_x = dispatch_ndiv.x(),
                             .groups_y = dispatch_ndiv.y() });

      // Finally; set entry state to false
      m_is_first_update = false;
    }
  } // namespace detail
} // namespace met