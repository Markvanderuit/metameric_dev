#pragma once

#include <metameric/core/fwd.hpp>
#include <small_gl/buffer.hpp>

namespace met {
  // Emitter representation in scene data
  struct Emitter {
    // Emitter type; only very basic primitives are supported
    enum class Type : uint { 
      eConstant = 0u, 
      ePoint    = 1u, 
      eSphere   = 2u, 
      eRect     = 3u
    };

  public:
    // Specific emitter type
    Type type = Type::eRect;
    
    // Scene properties
    bool      is_active = true;
    Transform transform;

    // Spectral data references a scene resource 
    uint  illuminant_i     = 0;    // index to spectral illuminant
    float illuminant_scale = 1.f;  // power multiplier

  public: // Boilerplater
    bool operator==(const Emitter &o) const {
      return std::tie(type, is_active, transform, illuminant_i, illuminant_scale) 
          == std::tie(o.type, o.is_active, o.transform, o.illuminant_i, o.illuminant_scale);
    }
  };

  namespace detail {
    // Template specialization of SceneGLHandler that provides up-to-date
    // representations of object data on the GL side. Information
    // is updated based on state tracking.
    template <>
    class SceneGLHandler<met::Emitter> : public SceneGLHandlerBase {
      // Per-object block layout for std140 uniform buffer
      struct alignas(16) EmBlockLayout {
        alignas(16) eig::Matrix4f trf;
        alignas(4)  uint          type;
        alignas(4)  bool          is_active;
        alignas(4)  uint          illuminant_i;
        alignas(4)  float         illuminant_scale;
      };
      static_assert(sizeof(EmBlockLayout) == 80);
      
      // All-object block layout for std140 uniform buffer, mapped for write
      struct EmBufferLayout {
        alignas(4) uint size;
        std::array<EmBlockLayout, met_max_emitters> data;
      } *m_em_info_map;

      // Single block layout for std140 uniform buffer, mapped for write
      struct EnvBufferLayout {
        alignas(4) bool envm_is_present;
        alignas(4) uint envm_i;
      } *m_envm_info_data;

    public:
      // This buffer stores one instance of EmBlockLayout per emitter component
      gl::Buffer emitter_info;

      // This buffer stores information on at most one environment emitter to sample.
      gl::Buffer emitter_envm_info;

      // Thiis buffer stores a sampling distribution based on emitter power and surface
      gl::Buffer emitter_distr_buffer;

    public:
      // Class constructor and update function handle GL-side data
      SceneGLHandler();
      void update(const Scene &) override;
    };
  } // namespace detail
} // namespace met

namespace std {
  // Format Emitter::Type, wich is an enum class
  template <>
  struct std::formatter<met::Emitter::Type> : std::formatter<string_view> {
    auto format(const met::Emitter::Type& ty, std::format_context& ctx) const {
      std::string s;
      switch (ty) {
        case met::Emitter::Type::eConstant : s = "constant"; break;
        case met::Emitter::Type::ePoint    : s = "point"; break;
        case met::Emitter::Type::eRect     : s = "rect"; break;
        case met::Emitter::Type::eSphere   : s = "sphere"; break;
      };
      return std::formatter<std::string_view>::format(s, ctx);
    }
  };
} // namespace std