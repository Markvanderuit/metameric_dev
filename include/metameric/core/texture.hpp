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
  using Texture2d3b    = Texture2d<eig::Array<std::byte, 3, 1>>;
  using Texture2d4b    = Texture2d<eig::Array<std::byte, 4, 1>>;
  using Texture2d3b_al = Texture2d<eig::AlArray<std::byte, 3>>;
  using Texture2d3f    = Texture2d<eig::Array3f>;
  using Texture2d4f    = Texture2d<eig::Array4f>;
  using Texture2d3f_al = Texture2d<eig::AlArray3f>;

  namespace detail {
    template <typename T>
    consteval uint texture_dims();

    template <> consteval uint texture_dims<eig::Array3f>()   { return 3; }
    template <> consteval uint texture_dims<eig::AlArray3f>() { return 3; }
    template <> consteval uint texture_dims<eig::Array4f>()   { return 4; }

    template <> consteval uint texture_dims<eig::Array<std::byte, 3, 1>>() { return 3; }
    template <> consteval uint texture_dims<eig::AlArray<std::byte, 3>>()  { return 4; }
    template <> consteval uint texture_dims<eig::Array<std::byte, 4, 1>>() { return 3; }
  } // namespace detail

  namespace io {
    // Load 2d rgb(a) texture from disk
    template <typename T>
    Texture2d<T> load_texture2d(const fs::path &path, bool srgb_to_lrgb = false);

    // Write 2d rgb/a texture to disk
    template <typename T>
    void save_texture2d(const fs::path &path, const Texture2d<T> &texture, bool lrgb_to_srgb = false);

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
   * Helper object to create texture object for a given size or with provided data.
   */
  template <typename T, uint D>
  struct TextureCreateInfo {
    eig::Array<uint, D, 1> size;
    std::span<const T>     data = { };
  };
  
  /**
   * Helper object to load texture object from disk, potentially with 
   * gamma to linear rgb conversion.
   */
  struct TextureLoadInfo {
    fs::path path;
    bool     srgb_to_lrgb = false;
  };
  
  /**
   * Underlying data block for texture objects
   */
  template <typename T, uint D> 
  struct TextureBlock {
  protected:
    using vect              = eig::Array<uint, D, 1>;
    using TextureCreateInfo = TextureCreateInfo<T, D>;

    /* block data */

    std::vector<T> m_data;
    vect           m_size;

    /* constrs */

    TextureBlock() = default;
    TextureBlock(TextureCreateInfo info);
    ~TextureBlock();

  public:
    /* data accessors */

    inline
    std::span<const T> data() const { return m_data; }
    inline
    std::span<T> data()             { return m_data; }

    /* size accessors */

    const vect size() const { return m_size; }
    static constexpr uint dims() { return detail::texture_dims<T>(); }

    /* miscellaneous */

    inline void swap(TextureBlock &o) {
      met_trace();
      using std::swap;
      swap(m_data, o.m_data);
      swap(m_size, o.m_size);
    }

    inline
    bool operator==(const TextureBlock &o) const {
      return std::equal(range_iter(m_data), 
                        range_iter(o.m_data),
                        [](const auto &a, const auto &b) { return a.isApprox(b); })
              && (m_size == o.m_size).all();
    }

    met_declare_noncopyable(TextureBlock);
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

    Texture2d()  = default;
    ~Texture2d() = default;

    Texture2d(TextureCreateInfo info) 
    : Base(info) { }

    Texture2d(TextureLoadInfo info)
    : Texture2d(io::load_texture2d<T>(info.path, info.srgb_to_lrgb)) { }

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

    /* miscellaneous */

    inline
    void swap(Texture2d &o) {
      met_trace();
      using std::swap;
      Base::swap(o);
    }

    inline
    bool operator==(const Texture2d &o) const {
      return Base::operator==(o);
    }

    template <typename T_> 
    Texture2d<T_> convert();

    met_declare_noncopyable(Texture2d);
  };
} // namespace met