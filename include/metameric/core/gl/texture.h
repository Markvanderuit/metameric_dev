#pragma once

#include <initializer_list>
#include <span>
#include <metameric/core/define.h>
#include <metameric/core/fwd.h>
#include <metameric/core/math.h>
#include <metameric/core/gl/abstract_object.h>

namespace metameric::gl {
  /**
   * This list of values classifies possible internal formats for
   * a texture object. Unorm/Snorm and 8-bit types have been stripped.
   */
  enum class TextureFormat {
    eR32UInt,  eRG32UInt,  eRGB32UInt,  eRGBA32UInt,
    eR32Int,   eRG32Int,   eRGB32Int,   eRGBA32Int,
    eR32Float, eRG32Float, eRGB32Float, eRGBA32Float,
    
    eR16UInt,  eRG16UInt,  eRGB16UInt,  eRGBA16UInt,
    eR16Int,   eRG16Int,   eRGB16Int,   eRGBA16Int,
    eR16Float, eRG16Float, eRGB16Float, eRGBA16Float,

    eDepth32, eDepth24, eDepth24Stencil8, eStencil8,
  };

  /**
   * Thin wrapper around an OpenGL texture object with a number of convenience functions. Does
   * not allow copy construction or assignment.
   */
  class Texture : public AbstractObject {
    inline uint levels() const { return _levels; }
    inline uint dims() const { return _dims; }
    inline uint w() const { return _w; }
    inline uint h() const { return _h; }
    inline uint d() const { return _d; }
    inline TextureFormat format() const { return _format; }

  private:
    uint _levels;
    uint _dims;
    uint _w, _h, _d;
    TextureFormat _format;

  public:
    // Base constructors to setup/teardown underlying texture
    Texture() = default;

    // Texture(TextureFormat format, uint levels, const eig::Vector<int, eig::Dynamic, 1> &dims);

    Texture(TextureFormat format, uint levels, VectorXi dims);
    Texture(TextureFormat format, uint levels,
            uint w, uint h = 1u, uint d = 1u);
    /* Texture(TextureFormat format, const void *ptr, uint levels, 
            uint w, uint h = 1u, uint d = 1u); */
    ~Texture();
    
    void set_image(void const *ptr, uint level = 0, uint xo = 0, uint yo = 0, uint zo = 0);    
  };

  /**
   * Thin abstract class wrapper around OpenGL texture objects. Does 
   * not allow copy construction or assignment.
   */
  /* struct AbstractTexture : public AbstractObject {
    // Internal storage format used in texture.
    inline TextureFormat format() const { return _format; }

    // Number of mipmap levels stored in texture (default = 1).
    inline size_t levels() const { return _levels; }

  protected:
    TextureFormat _format;
    size_t _levels;

  protected:
    AbstractTexture() = default;
    explicit AbstractTexture(size_t levels = 1) noexcept;
    virtual ~AbstractTexture() = default;
  
    inline
    void swap(AbstractTexture &o) {
      using std::swap;
      AbstractObject::swap(o);
      swap(_levels, o._levels);
    }
    
    inline bool operator==(const AbstractTexture &o) const {
      using std::tie;
      return AbstractObject::operator==(o)
        && tie(_levels)
        == tie(o._levels);
    }

    MET_DECLARE_NONCOPYABLE(AbstractTexture);
  };

  class Texture1D {
  public:

  private:
    size_t _w;

  public:
    
  };

  class Texture2D {
  public:

  private:
    size_t _w, _h;
  
  public:
    
  };

  class Texture3D {
  public:

  private:
    size_t _w, _h, _d;
  
  public:
    
  }; */
} // namespace metameric::gl