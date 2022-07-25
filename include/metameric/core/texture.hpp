#pragma once

#include <metameric/core/io.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <filesystem>
#include <vector>

namespace met {
  // FWD
  template <typename T, uint D> struct TextureBlock;
  template <typename T>         struct Texture2d;
  using Texture2d3f    = Texture2d<eig::Array3f>;
  using Texture2d4f    = Texture2d<eig::Array4f>;
  using Texture2d3f_al = Texture2d<eig::AlArray3f>;

  namespace detail {
    template <typename T>
    consteval uint texture_dims();

    template <> consteval uint texture_dims<eig::Array3f>()   { return 3; }
    template <> consteval uint texture_dims<eig::AlArray3f>() { return 3; }
    template <> consteval uint texture_dims<eig::Array4f>()   { return 4; }
  } // namespace detail

  namespace io {
    // Load 2d rgb(a) texture from disk
    template <typename T>
    Texture2d<T> load_texture2d(const fs::path &path);

    // Write 2d rgb/a texture to disk
    template <typename T>
    void save_texture2d(const fs::path &path, const Texture2d<T> &texture);

    // Convert to aligned/unaligned backed types
    Texture2d3f    as_unaligned(const Texture2d3f_al &aligned);
    Texture2d3f_al as_aligned(const Texture2d3f &unaligned);

    // Linearize/delinearize texture data
    template <typename T>
    void to_srgb(Texture2d<T> &lrgb);
    template <typename T>
    void to_lrgb(Texture2d<T> &srgb);
    template <typename T>
    Texture2d<T> as_srgb(const Texture2d<T> &lrgb);
    template <typename T>
    Texture2d<T> as_lrgb(const Texture2d<T> &srgb);
  } // namespace io
  
  /**
   * Helper object to create texture object.
   */
  template <typename T, uint D>
  struct TextureCreateInfo {
    using vect = eig::Array<int, D, 1>;

  public:
    vect size;
    std::span<T> data = { };
  };
  
  /**
   * Helper object to load texture object from disk.
   */
  struct TextureLoadInfo {
    fs::path path;
  };
  
  /**
   * Underlying data block for texture objects
   */
  template <typename T, uint D>
  struct TextureBlock {
  protected:
    using vect              = eig::Array<int, D, 1>;
    using TextureCreateInfo = TextureCreateInfo<T, D>;

    /* block data */

    std::vector<T> m_data;
    vect           m_size;

    /* constrs */

    TextureBlock() = default;
    TextureBlock(TextureCreateInfo info);

  public:
    /* data accessors */

    inline
    std::span<const T> data() const { return m_data; }
    inline
    std::span<T> data()             { return m_data; }

    /* size accessors */

    const vect size() const { return m_size; }
    constexpr static uint dims() { return detail::texture_dims<T>(); }
  };

  /**
   * Two-dimensional texture object.
   */
  template <typename T>
  struct Texture2d : public TextureBlock<T, 2> {
    using Base              = TextureBlock<T, 2>;
    using TextureCreateInfo = Base::TextureCreateInfo;

  public:
    /* constrs */

    Texture2d() = default;

    Texture2d(TextureCreateInfo info) 
    : Base(info) { }

    Texture2d(TextureLoadInfo info)
    : Texture2d(io::load_texture2d<T>(info.path)) { }

    /* data accessors */

    inline
    const T &operator[](const eig::Array2i &v) const { 
      return this->m_data[v.y() * this->m_size.x() + v.x()];
    }

    inline
    T &operator[](const eig::Array2i &v) { 
      return this->m_data[v.y() * this->m_size.x() + v.x()];
    }
    
    inline
    const T &operator()(uint i, uint j) const { 
      return this->m_data[j * this->m_size.x() + i];
    }

    inline
    T &operator()(uint i, uint j) {
      return this->m_data[j * this->m_size.x() + i];
    }
  };
} // namespace met