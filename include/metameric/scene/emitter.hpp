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
  // Emitter representation in scene data
  struct Emitter {
    // Emitter type; only very basic primitives are supported
    enum class Type : uint { 
      eEnviron = 0u, 
      ePoint   = 1u, 
      eSphere  = 2u, 
      eRect    = 3u
    };

    // Emitter's spectral source
    enum class SpectrumType : uint {
      eIllm = 0u, // a selected illuminant spectrum
      eColr = 1u, // a uplifted color/texture value
    };

  public:
    // Specific emitter type
    Type         type      = Type::eRect;
    SpectrumType spec_type = SpectrumType::eIllm;

    // Scene properties
    bool      is_active = true;
    Transform transform;
    
    // Illuminant data
    std::variant<Colr, uint> color = Colr(1.f); // Color or texture index
    uint  illuminant_i             = 0;         // index to spectrum
    float illuminant_scale         = 1.f;       // scaling applied to emission

  public: // Boilerplater
    bool operator==(const Emitter &o) const;
  };

  namespace detail {
    // Template specialization of SceneGLHandler that provides up-to-date
    // representations of object data on the GL side. Information
    // is updated based on state tracking.
    template <>
    class SceneGLHandler<met::Emitter> : public SceneGLHandlerBase {
      // Per-object block layout
      struct BlockLayout {
        alignas(16) eig::Matrix4f trf;
        // ---
        alignas(4)  uint          is_active;
        alignas(4)  uint          type;
        alignas(4)  uint          spec_type;
        alignas(4)  float         illuminant_scale;
        // ---
        alignas(8)  eig::Array2u  color_data;
        alignas(4)  uint          illuminant_i;
      };
      static_assert(sizeof(BlockLayout) == 96);

      // All-object buffer layout
      struct BufferLayout {
        alignas(4)  uint n;
        alignas(16) std::array<BlockLayout, met_max_objects> data;
      };
      
      // Write mapped persistent emitter data
      BufferLayout *m_emitter_info_map;

      // Single block layout for std140 uniform buffer, mapped for write
      struct EnvBufferLayout {
        alignas(4) bool envm_is_present;
        alignas(4) uint envm_i;
      } *m_envm_info_data;
      static_assert(sizeof(EnvBufferLayout) == 8);
      
    public:
      // This buffer stores one instance of BlockLayout per emitter component
      gl::Buffer emitter_info;

      // This buffer stores information on at most one environment emitter to sample.
      gl::Buffer emitter_envm_info;

      // This buffer stores a sampling distribution based on emitter power and surface
      // This ignores spatially varying emitters r.n.
      gl::Buffer emitter_distr_buffer;

      // Alias data goes here
      gl::Buffer envmap_distr_buffer;
    public:
      // Class constructor and update function handle GL-side data
      SceneGLHandler();
      void update(const Scene &) override;
    };

    
    // Template specialization of SceneStateHandler that exposes fine-grained
    // state tracking for object members in the program view
    template <>
    struct SceneStateHandler<Emitter> : public SceneStateHandlerBase<Emitter> {
      SceneStateHandler<decltype(Emitter::is_active)>        is_active;
      SceneStateHandler<decltype(Emitter::type)>             type;
      SceneStateHandler<decltype(Emitter::spec_type)>        spec_type;
      SceneStateHandler<decltype(Emitter::transform)>        transform;
      SceneStateHandler<decltype(Emitter::color)>            color;
      SceneStateHandler<decltype(Emitter::illuminant_i)>     illuminant_i;
      SceneStateHandler<decltype(Emitter::illuminant_scale)> illuminant_scale;
      
    public:
      bool update(const Emitter &o) override {
        met_trace();
        return m_mutated =
        ( is_active.update(o.is_active)
        | type.update(o.type)
        | spec_type.update(o.spec_type)
        | transform.update(o.transform)
        | color.update(o.color)
        | illuminant_i.update(o.illuminant_i)
        | illuminant_scale.update(o.illuminant_scale)
        );
      }
    };
  } // namespace detail
} // namespace met

template<>
struct fmt::formatter<met::Emitter::Type>{
  template <typename context_ty>
  constexpr auto parse(context_ty& ctx) { 
    return ctx.begin(); 
  }

  template <typename fmt_context_ty>
  constexpr auto format(const met::Emitter::Type& ty, fmt_context_ty& ctx) const {
    std::string s;
    switch (ty) {
      case met::Emitter::Type::eEnviron  : s = "environ"; break;
      case met::Emitter::Type::ePoint    : s = "point"; break;
      case met::Emitter::Type::eRect     : s = "rect"; break;
      case met::Emitter::Type::eSphere   : s = "sphere"; break;
      default                            : s = "undefined"; break;
    }
    return fmt::format_to(ctx.out(), "{}", s);
  }
};

template<>
struct fmt::formatter<met::Emitter::SpectrumType>{
  template <typename context_ty>
  constexpr auto parse(context_ty& ctx) { 
    return ctx.begin(); 
  }

  template <typename fmt_context_ty>
  constexpr auto format(const met::Emitter::SpectrumType& ty, fmt_context_ty& ctx) const {
    std::string s;
    switch (ty) {
      case met::Emitter::SpectrumType::eIllm : s = "spectrum"; break;
      case met::Emitter::SpectrumType::eColr : s = "uplifted"; break;
      default                                : s = "undefined"; break;
    }
    return fmt::format_to(ctx.out(), "{}", s);
  }
};