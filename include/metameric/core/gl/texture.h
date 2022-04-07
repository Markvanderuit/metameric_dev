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

  enum class TextureType {
    eDefault,
    eArray,
    eBuffer,
    eCubemap,
    eCubemapArray,
    eMultisample,
    eMultisampleArray
  };

  template <typename T, uint D, TextureType Ty = TextureType::eDefault>
  class AbstractTexture : public AbstractObject {
    using Array = eig::Array<uint, D, 1>;
    using ArrayRef = const eig::Ref<const Array> &;

  protected:
    AbstractTexture() = default;
    AbstractTexture(Array dims, uint levels = 1, T const *ptr = nullptr);
    ~AbstractTexture();
  
  public:
    virtual void get_image_mem(T *ptr, size_t ptr_size, uint level = 0) const;
    virtual void set_image_mem(T const *ptr, size_t ptr_size, uint level = 0);
    virtual void get_subimage_mem(T *ptr, size_t ptr_size, uint level = 0,
                                  ArrayRef dims = Array::Zero(), ArrayRef off = Array::Zero()) const;
    virtual void set_subimage_mem(T const *ptr, size_t ptr_size, uint level = 0, 
                                  ArrayRef dims = Array::Zero(), ArrayRef off = Array::Zero());
    
    // Convenience get_image_mem() variants accepting common STL formats
    template <size_t E>
    auto get_image(std::span<T, E> c, uint level = 0) 
    { get_image_mem(std::data(c), std::size(c) * sizeof(T), level); return c; }
    template <template <typename> class C>
    auto & get_image(C<T> &c, uint level = 0) const
    { get_image_mem(std::data(c), std::size(c) * sizeof(T), level); return c; }                 

    // Convenience set_image_mem() variants accepting common STL formats
    template <size_t E>
    void set_image(std::span<T, E> c, uint level = 0) 
    { set_image_mem(std::data(c), std::size(c) * sizeof(T), level); }
    template <template <typename> class C>
    void set_image(const C<T> &c, uint level = 0) 
    { set_image_mem(std::data(c), std::size(c) * sizeof(T), level); }
    
    // Convenience set_subimage_mem() variants accepting common STL formats
    template <size_t E>
    void set_subimage(std::span<T, E> c, uint level = 0, ArrayRef dims = Array::Zero(), ArrayRef off = Array::Zero()) 
    { set_image_mem(std::data(c), std::size(c) * sizeof(T), level, dims, off); }
    template <template <typename> class C>
    void set_subimage(const C<T> &c, uint level = 0, ArrayRef dims = Array::Zero(), ArrayRef off = Array::Zero()) 
    { set_image_mem(std::data(c), std::size(c) * sizeof(T), level, dims, off); }

    template <template <typename> class C>
    void get_image_as(uint level = 0) const {
      // C<T> c() ...
    }
  };

  template <typename T, uint D, TextureType Ty = TextureType::eDefault>
  class ImageTexture : public AbstractTexture<T, D, Ty> {
    using Array = eig::Array<uint, D, 1>;
    using ArrayRef = const eig::Ref<const Array> &;

  public:
    ImageTexture() = default;
    ImageTexture(Array dims, uint levels = 1, T const *ptr = nullptr);
    ~ImageTexture();

  public:
    void get_image_mem(T *ptr, size_t ptr_size, uint level = 0) const override;
    void set_image_mem(T const *ptr, size_t ptr_size, uint level = 0) override;
    void get_subimage_mem(T *ptr, size_t ptr_size, uint level = 0,
                          ArrayRef dims = Array::Zero(), ArrayRef off = Array::Zero()) const override;
    void set_subimage_mem(T const *ptr, size_t ptr_size, uint level = 0, 
                          ArrayRef dims = Array::Zero(), ArrayRef off = Array::Zero()) override;
  };

  template <uint D, TextureType Ty = TextureType::eDefault>
  class DepthTexture : public AbstractTexture<float, D, Ty> {
    using Array = eig::Array<uint, D, 1>;
    using ArrayRef = const eig::Ref<const Array> &;
  
  public:
    DepthTexture() = default;
    DepthTexture(Array dims, uint levels = 1, float const *ptr = nullptr);
    ~DepthTexture();

  };

  template <typename T, uint Precision, uint Dimensions>
  class TemplatedTexture : public AbstractObject {
    using Array = eig::Array<T, Dimensions, 1>;
    using ArrayRef = const eig::Ref<const Array> &;
    
  public:

  private:

  public:
    TemplatedTexture() = default;
    TemplatedTexture(Array dims, uint levels = 1, const void *ptr = nullptr);
    ~TemplatedTexture();

    void get_image_mem(void *ptr, size_t ptr_size, uint level = 0) const;
    void set_image_mem(void const *ptr, size_t ptr_size, uint level = 0);
    void get_subimage_mem(void *ptr, size_t ptr_size, uint level = 0,
                          ArrayRef dims = Array::Zero(), ArrayRef off = Array::Zero()) const;
    void set_subimage_mem(void const *ptr, size_t ptr_size, uint level = 0, 
                          ArrayRef dims = Array::Zero(), ArrayRef off = Array::Zero());

  };

  /**
   * Thin wrapper around an OpenGL texture object with a number of convenience functions. Does
   * not allow copy construction or assignment.
   */
  class Texture : public AbstractObject {
    using Array = const eig::Ref<const ArrayXi> &;

  public:
    inline uint levels() const { return _levels; }
    inline TextureFormat format() const { return _format; }
    inline ArrayXi dims() const { return _dims; }

  private:
    uint _levels;
    TextureFormat _format;
    ArrayXi _dims;

  public:
    // Base constructors to setup/teardown underlying texture
    Texture() = default;
    Texture(TextureFormat format, 
            Array dims, 
            uint levels = 1, 
            const void *ptr = nullptr);
    ~Texture();

    // Base get/set operations to read/write texture data. Subimage get is not supported r.n.
    void get_image_mem(void *ptr, size_t ptr_size,uint level = 0) const;
    void set_image_mem(void const *ptr, size_t ptr_size, uint level = 0,  Array dims = ArrayXi {0}, Array off = ArrayXi {0});
    /*  void get_subimage(void *ptr,
                      uint level = 0, 
                      const eig::Ref<const ArrayXi> &dims = ArrayXi::Zero(1),
                      const eig::Ref<const ArrayXi> &off = ArrayXi::Zero(1)) const; */

    // Convenience get_image_mem() variants accepting common STL formats
    template <typename T, size_t E>
    auto get_image(std::span<T, E> c, uint level = 0) 
    { get_image_mem(std::data(c), std::size(c) * sizeof(T), level); return c; }
    template <typename C>
    auto & get_image(C &c, uint level = 0) const
    { get_image_mem(std::data(c), std::size(c) * sizeof(C::value_type), level); return c; }

    // Convenience set_image_mem() variants accepting common STL formats
    template <typename T, size_t E>
    void set_image(std::span<T, E> c, uint level = 0, Array dims = ArrayXi {0}, Array off = ArrayXi {0}) 
    { set_image_mem(std::data(c), std::size(c) * sizeof(T), level, dims, off); }
    template <typename C>
    void set_image(const C &c, uint level = 0, Array dims = ArrayXi {0}, Array off = ArrayXi {0}) 
    { set_image_mem(std::data(c), std::size(c) * sizeof(C::value_type), level, dims, off); }

    template <typename C>
    C get_as(uint level = 0) const {
      C c(_dims.prod() * sizeof(C::value_type)); // TODO should test against underlying type
      return get_image(c, level);
    }

    // void clear()

    // Texture copy() const;
    void copy_from(const Texture &o, uint level = 0, Array dims = ArrayXi {0}, Array off = ArrayXi {0});
    // void copy_to(Texture &o) const;

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