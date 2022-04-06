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
  public:
    inline uint levels() const { return _levels; }
    inline TextureFormat format() const { return _format; }
    inline ArrayXi dims() const { return _dims; }

  private:
    using ArrayRef = const eig::Ref<const ArrayXi> &;

    uint _levels;
    TextureFormat _format;
    ArrayXi _dims;

  public:
    // Base constructors to setup/teardown underlying texture
    Texture() = default;
    Texture(TextureFormat format, ArrayRef dims, uint levels = 1, const void *ptr = nullptr);
    ~Texture();

  public:
    void set_image_mem(void const *ptr,
                       size_t ptr_size,
                       uint level = 0, 
                       ArrayRef dims = ArrayXi {0},
                       ArrayRef off = ArrayXi {0});
    void get_image_mem(void *ptr, 
                       size_t ptr_size,
                       uint level = 0) const;
    /*  void get_subimage(void *ptr,
                      uint level = 0, 
                      const eig::Ref<const ArrayXi> &dims = ArrayXi::Zero(1),
                      const eig::Ref<const ArrayXi> &off = ArrayXi::Zero(1)) const; */

    // Convenience set_image_mem() variants accepting common STL formats
    template <typename T, size_t E>
    void set_image(std::span<T, E> c, 
                   uint level = 0,
                   ArrayXi dims = ArrayXi {0},
                   ArrayXi off = ArrayXi {0}) {
      set_image_mem(std::data(c), std::size(c) * sizeof(T), level, dims, off);
    }
    template <typename C>
    void set_image(const C &c, 
                   uint level = 0,
                   ArrayXi dims = ArrayXi {0},
                   ArrayXi off = ArrayXi {0}) {
      set_image_mem(std::data(c), std::size(c) * sizeof(C::value_type), level, dims, off);
    }

    // Convenience get_image_mem() variants accepting common STL formats
    template <typename T, size_t E>
    void get_image(std::span<T, E> c, uint level = 0) {
      get_image_mem(std::data(c), std::size(c) * sizeof(T), level);
    }
    template <typename C>
    void get_image(C &c, uint level = 0) {
      get_image_mem(std::data(c), std::size(c) * sizeof(C::value_type), level);
    }

  public:
    inline void swap(Texture &o) {
      using std::swap;
      AbstractObject::swap(o);
      swap(_levels, o._levels);
      swap(_format, o._format);
      swap(_dims, o._dims);
    }
    
    inline bool operator==(const Texture &o) const {
      using std::tie;
      return AbstractObject::operator==(o)
        && tie(_levels, _format)
        == tie(o._levels, o._format)
        && (_dims.array() == o._dims.array()).all();
    }

    // Enable move constr/assign, but disallow direct copies to prevent accidental usage
    MET_DECLARE_NONCOPYABLE(Texture);
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