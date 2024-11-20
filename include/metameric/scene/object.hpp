#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/scene/detail/atlas.hpp>
#include <metameric/scene/detail/utility.hpp>
#include <small_gl/buffer.hpp>

namespace met {
  // Object representation; 
  // A shape represented by a surface mesh, material data, 
  // and underlying uplifting to handle spectral reflectance.
  struct Object {
    // BRDF type; only very basic BRDFs are supported
    enum class BRDFType {
      eNull       = 0,  // null does not interact with scene
      eDiffuse    = 1,  // diffuse is simple lambertian
      eMicrofacet = 2,  // microfacet is simple conductor/dielectric mixture built on ggx
    };
    
  public:
    // Scene placement
    bool      is_active = true;
    Transform transform;

    // Indices to underlying mesh/uplifting
    uint mesh_i      = 0;
    uint uplifting_i = 0;

    // Material data is packed with object; 
    // Most values are a variant; either a specified value, or a texture index
    BRDFType                  brdf_type = BRDFType::eDiffuse;
    std::variant<Colr,  uint> diffuse   = Colr(.5f);
    std::variant<float, uint> metallic  = 0.f;
    std::variant<float, uint> roughness = 0.1f;

  public: // Boilerplate
    bool operator==(const Object &o) const;
  };

  namespace detail {
    // Template specialization of SceneGLHandler that provides up-to-date
    // representations of object data on the GL side. Information
    // is updated based on state tracking.
    template <>
    struct SceneGLHandler<met::Object> : public SceneGLHandlerBase {
      // Helper object that
      // - generates per-object packed brdf data
      // - writes this data to the `texture_brdf` atlas below
      struct ObjectData {
        // Layout for data written to std140 buffer
        struct BlockLayout { uint object_i; };

        // Objects for texture bake
        std::string  m_program_key;
        gl::Sampler  m_sampler;
        gl::Buffer   m_buffer;
        BlockLayout *m_buffer_map;

        // Small private state
        uint m_object_i;
        bool m_is_first_update;
      
      public:
        ObjectData(const Scene &scene, uint object_i);
        void update(const Scene &scene);
      };
    
      // Object cache; helps pack brdf components
      std::vector<ObjectData> object_data;

    private:
      // Per-object block layout for std140 uniform buffer
      struct alignas(16) BlockLayout {
        alignas(16) eig::Matrix4f trf;
        alignas(4)  bool          is_active;
        alignas(4)  uint          mesh_i;
        alignas(4)  uint          uplifting_i;
        alignas(4)  uint          brdf_type;
        alignas(8)  eig::Array2u  albedo_data;
        alignas(4)  uint          metallic_data;
        alignas(4)  uint          roughness_data;
      };
      static_assert(sizeof(BlockLayout) == 96);
      
      // All-object block layout for std140 uniform buffer, mapped for write
      struct BufferLayout {
        alignas(4) uint size;
        std::array<BlockLayout, met_max_objects> data;
      } *m_object_info_map;

    public:
      // Stores one instance of BlockLayout per object component
      gl::Buffer object_info;

      // Stores packing of some brdf parameters (roughness, metallic at fp16)
      detail::TextureAtlas2d1ui texture_brdf; 
    
    public:
      // Class constructor and update function handle GL-side data
      SceneGLHandler();
      void update(const Scene &) override;
    };

    // Template specialization of SceneStateHandler that exposes fine-grained
    // state tracking for object members in the program view
    template <>
    struct SceneStateHandler<Object> : public SceneStateHandlerBase<Object> {    
      SceneStateHandler<decltype(Object::is_active)>   is_active;
      SceneStateHandler<decltype(Object::transform)>   transform;
      SceneStateHandler<decltype(Object::mesh_i)>      mesh_i;
      SceneStateHandler<decltype(Object::uplifting_i)> uplifting_i;
      SceneStateHandler<decltype(Object::brdf_type)>   brdf_type;
      SceneStateHandler<decltype(Object::diffuse)>     diffuse;
      SceneStateHandler<decltype(Object::metallic)>    metallic;
      SceneStateHandler<decltype(Object::roughness)>   roughness;

    public:
      bool update(const Object &o) override {
        met_trace();
        return m_mutated = 
        ( is_active.update(o.is_active)
        | transform.update(o.transform)
        | mesh_i.update(o.mesh_i)
        | uplifting_i.update(o.uplifting_i)
        | brdf_type.update(o.brdf_type)
        | diffuse.update(o.diffuse)
        | metallic.update(o.metallic)
        | roughness.update(o.roughness)
        );
      }
    };
  } // namespace detail
} // namespace met

template<>
struct fmt::formatter<met::Object::BRDFType>{
  template <typename context_ty>
  constexpr auto parse(context_ty& ctx) { 
    return ctx.begin(); 
  }

  template <typename fmt_context_ty>
  constexpr auto format(const met::Object::BRDFType& ty, fmt_context_ty& ctx) const {
    std::string s;
    switch (ty) {
      case met::Object::BRDFType::eNull        : s = "null"; break;
      case met::Object::BRDFType::eDiffuse     : s = "diffuse"; break;
      case met::Object::BRDFType::eMicrofacet  : s = "microfacet"; break;
      default                                  : s = "undefined"; break;
    }
    return fmt::format_to(ctx.out(), "{}", s);
  }
};