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
    // Scene placement
    bool      is_active = true;
    Transform transform;

    // Indices to underlying mesh/uplifting
    uint mesh_i      = 0;
    uint uplifting_i = 0;

    // Material data is packed with object; 
    // Most values are a variant; either a specified value, or a texture index
    std::variant<Colr,  uint> albedo         = Colr(.5f);        // for albedo/microfacet/dielectric with absorption
    std::variant<float, uint> metallic        = 0.0f;             // for microfacet brdf
    std::variant<float, uint> alpha       = 1.0f;             // for microfacet brdf
    std::variant<float, uint> transmission    = 0.0f;             // for microfacet brdf
    eig::Array2f              eta_minmax      = { 1.25f, 1.25f }; // for dielectric brdf
    float                     absorption      = 0.f;              // for dielectric brdf
    std::optional<uint>       normalmap       = { };              // optional normalmap texture inndex
    float                     clearcoat       = 0.f;              // for clearcoat layer
    float                     clearcoat_alpha = 0.f;              // for clearcoat layer

    // Scalar modifiers to uv for wrapping; factored out during uplift bake
    eig::Array2f uv_offset = { 0, 0 };
    eig::Array2f uv_extent = { 1, 1 };

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
        struct BlockLayout { 
          alignas(4) uint         object_i; 
          alignas(4) uint         object_metallic_data; 
          alignas(4) uint         object_roughness_data; 
          alignas(4) uint         object_transmission_data; 
          // ---
          alignas(8) eig::Array2f uv_offset;
          alignas(8) eig::Array2f uv_extent;
          // ---
          alignas(8) eig::Array2u object_albedo_data; 
          alignas(4) uint         object_normalmap_data; 
          alignas(4) uint         object_data_y; 
          alignas(4) uint         object_data_z; 
        };
        static_assert(sizeof(BlockLayout) == 56);

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
      // Per-object block layout
      struct BlockLayout {
        alignas(16) eig::Matrix4f trf;
        alignas(4)  uint          flags;
      };
      static_assert(sizeof(BlockLayout) == 80);

      // All-object buffer layout
      struct BufferLayout {
        alignas(4)  uint n;
        alignas(16) std::array<BlockLayout, met_max_objects> data;
      };

      // Write mapped persistent object data
      BufferLayout *m_object_info_map;

    public:
      // Stores one instance of BlockLayout per object component
      gl::Buffer object_info;

      // Stores packing of some brdf parameters (alpha, metallic, normalmap)
      detail::TextureAtlas2d4f texture_brdf; 
    
    public:
      // Class constructor and update function handle GL-side data
      SceneGLHandler();
      void update(const Scene &) override;
    };

    // Template specialization of SceneStateHandler that exposes fine-grained
    // state tracking for object members in the program view
    template <>
    struct SceneStateHandler<Object> : public SceneStateHandlerBase<Object> {    
      SceneStateHandler<decltype(Object::is_active)>          is_active;
      SceneStateHandler<decltype(Object::transform)>          transform;
      SceneStateHandler<decltype(Object::mesh_i)>             mesh_i;
      SceneStateHandler<decltype(Object::uplifting_i)>        uplifting_i;
      SceneStateHandler<decltype(Object::albedo)>             albedo;
      SceneStateHandler<decltype(Object::metallic)>           metallic;
      SceneStateHandler<decltype(Object::alpha)>              alpha;
      SceneStateHandler<decltype(Object::transmission)>       transmission;
      SceneStateHandler<decltype(Object::eta_minmax)>         eta_minmax;
      SceneStateHandler<decltype(Object::absorption)>         absorption;
      SceneStateHandler<decltype(Object::normalmap)>          normalmap;
      SceneStateHandler<decltype(Object::clearcoat)>          clearcoat;
      SceneStateHandler<decltype(Object::clearcoat_alpha)>    clearcoat_alpha;
      SceneStateHandler<decltype(Object::uv_offset)>          uv_offset;
      SceneStateHandler<decltype(Object::uv_extent)>           uv_extent;

    public:
      bool update(const Object &o) override {
        met_trace();
        return m_mutated = 
        ( is_active.update(o.is_active)
        | transform.update(o.transform)
        | mesh_i.update(o.mesh_i)
        | uplifting_i.update(o.uplifting_i)
        | albedo.update(o.albedo)
        | metallic.update(o.metallic)
        | alpha.update(o.alpha)
        | transmission.update(o.transmission)
        | eta_minmax.update(o.eta_minmax)
        | absorption.update(o.absorption)
        | clearcoat.update(o.clearcoat)
        | clearcoat_alpha.update(o.clearcoat_alpha)
        | normalmap.update(o.normalmap)
        | uv_offset.update(o.uv_offset)
        | uv_extent.update(o.uv_extent)
        );
      }
    };
  } // namespace detail
} // namespace met