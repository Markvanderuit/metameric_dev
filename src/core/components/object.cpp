#include <metameric/core/components/object.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/scene.hpp>

namespace met {
  bool Object::operator==(const Object &o) const {
    guard(std::tie(is_active,   transform,   mesh_i,   uplifting_i,   brdf_type) == 
          std::tie(o.is_active, o.transform, o.mesh_i, o.uplifting_i, o.brdf_type), 
          false);

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
   
    return true;
  }

  namespace detail {
    // Helper to pack color/uint variant to a uvec2
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

    // Helper to pack float/uint variant to a uint
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

    SceneGLHandler<met::Object>::SceneGLHandler() {
      met_trace_full();

      // Allocate up to a number of objects and obtain writeable/flushable mapping
      std::tie(object_info, m_object_info_map) = gl::Buffer::make_flusheable_object<BufferLayout>();
    }

    void SceneGLHandler<met::Object>::update(const Scene &scene) {
      met_trace_full();

      const auto &objects = scene.components.objects;
      guard(!objects.empty() && objects);

      // Set appropriate object count, then flush change to buffer
      m_object_info_map->size = static_cast<uint>(objects.size());
      object_info.flush(sizeof(uint));

      // Write updated objects to mapping
      for (uint i = 0; i < objects.size(); ++i) {
        const auto &[object, state] = objects[i];
        guard_continue(state);
        
        // Get mesh transform, incorporate into gl-side object transform
        auto object_trf = object.transform.affine().matrix().eval();
        auto mesh_trf   = scene.resources.meshes.gl.mesh_cache[object.mesh_i].unit_trf;
        auto trf        = (object_trf * mesh_trf).eval();

        // Fill in object struct data
        m_object_info_map->data[i] = {
          .trf            = trf,
          .is_active      = object.is_active,
          .mesh_i         = object.mesh_i,
          .uplifting_i    = object.uplifting_i,
          .brdf_type      = static_cast<uint>(object.brdf_type),
          .albedo_data    = pack_material_3f(object.diffuse),
          .metallic_data  = pack_material_1f(object.metallic),
          .roughness_data = pack_material_1f(object.roughness),
        };

        // Flush change to buffer; most changes to objects are local,
        // so we flush specific regions instead of the whole
        object_info.flush(sizeof(BlockLayout), sizeof(BlockLayout) * i + sizeof(uint));
      } // for (uint i)
    }
  } // namespace detail
} // namespace met