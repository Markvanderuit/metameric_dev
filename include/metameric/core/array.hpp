#pragma once

#include <array>
#include <span>
#include <sstream>
#include <vector>
#include <metameric/core/detail/array.hpp>

namespace met {
  // FWD
  template <typename T, size_t Size, 
            template <typename, size_t> typename C = std::array>
  class _Array;
  template <size_t Size, 
            template <typename, size_t> typename C = std::array>
  class _MaskArray;

  template <typename T, size_t Size, template <typename, size_t> typename C>
  class _Array {
    using value_type = T;
    using ref_type   = T &;
    using ptr_type   = T *;
    using cref_type  = const T &;
    using size_type  = size_t;

    using cont_type  = C<value_type, Size>;
    using base_type  = _Array;
    using mask_type  = _MaskArray<Size, C>;

    alignas(sizeof(value_type)) cont_type m_cont;
    
  public:
    /* constrs */

    constexpr _Array() = default;

    constexpr _Array(value_type value) { 
      std::fill(m_cont.begin(), m_cont.end(), value);
    }

    constexpr _Array(std::span<value_type> values) {
      std::copy(values.begin(), values.end(), m_cont.begin());
    }

    /* accessors */

    constexpr size_type size() const noexcept { return Size; }

    constexpr ref_type at(size_type i) { return m_cont[i]; }

    constexpr cref_type at(size_type i) const { return m_cont[i]; }

    constexpr ref_type operator[](size_type i) { return m_cont[i]; }

    constexpr cref_type operator[](size_type i) const { return m_cont[i]; }

    constexpr ptr_type data() noexcept { return m_cont.data(); }

    /* iterators */

    constexpr friend auto begin(_Array &a) noexcept { return a.m_cont.begin(); }

    constexpr friend auto end(_Array &a) noexcept { return a.m_cont.end(); }

    constexpr auto begin() noexcept { return m_cont.begin(); }

    constexpr auto end() noexcept { return m_cont.end(); }

    /* operators */

    met_array_decl_op_add(_Array);
    met_array_decl_op_sub(_Array);
    met_array_decl_op_mul(_Array);
    met_array_decl_op_div(_Array);
    met_array_decl_op_com(_Array);

    /* reductions */

    met_array_decl_red_val(_Array);
    met_array_decl_mod_val(_Array);

    /* misc */

    constexpr std::string to_string() const {
      std::stringstream ss;
      ss << "[ ";
      for (size_t i = 0; i < size() - 1; ++i)
        ss << operator[](i) << ", ";
      ss << operator[](size() - 1) << " ]";
      return ss.str();
    }
  };

  template <size_t Size, template <typename, size_t> typename C>
  class _MaskArray : public _Array<bool, Size, C> {
    using value_type = bool;
    using ref_type   = bool &;
    using ptr_type   = bool *;
    using cref_type  = const bool &;
    using size_type  = size_t;

    using base_type  = _Array<bool, Size, C>;
    using mask_type  = _MaskArray;

  public:
    /* constrs */

    using base_type::base_type;

    /* operators */

    met_array_decl_op_com(_MaskArray);

    /* reductions */

    constexpr value_type all() const noexcept {
      value_type v = base_type::operator[](0);
      for (size_type i = 0; i < base_type::size(); ++i)
        v &= base_type::operator[](i);
      return v;
    }

    constexpr value_type any() const noexcept {
      value_type v = base_type::operator[](0);
      for (size_type i = 0; i < base_type::size(); ++i)
        v |= base_type::operator[](i);
      return v;
    }

    constexpr value_type none() const noexcept {
      value_type v = base_type::operator[](0);
      for (size_type i = 0; i < base_type::size(); ++i)
        v &= base_type::operator[](i);
      return !v;
    }
  };

  /* begin/end */  
  
  template <typename T>
  class Array {
  public:
    constexpr virtual 
    size_t size() const = 0;

    constexpr virtual 
    T & operator[](size_t i) = 0;

    constexpr virtual 
    T operator[](size_t i) const = 0;
  };

  template <typename T, size_t Size>
  class StaticArray : public Array<T> {
    using Base = Array<T>;

    std::array<T, Size> m_container;

  public:
    /* accessors */

    constexpr
    size_t size() const override {
      return Size;
    }

    constexpr
    T & operator[](size_t i) override {
      return m_container[i];
    }

    inline constexpr
    T operator[](size_t i) const override {
      return m_container[i];
    }

    /* constructors */

    StaticArray() = default;

    StaticArray(T value) { 
      m_container.fill(value);
    }

    StaticArray(std::span<T> values) {
      std::copy(values.begin(), values.end(), m_container.begin());
    }

    /* operators */

    met_array_decl_operators(Base, StaticArray, T);
    met_array_decl_reductions(StaticArray, T);
    met_array_decl_comparators(StaticArray, T);
  };
  // met_array_decl_operands(StaticArray<float>, float);
  // met_array_decl_operands(StaticArray<uint>, uint);
  // met_array_decl_operands(StaticArray<int>, int);

  template <typename T>
  class DynamicArray : public Array<T> {
    using Base = Array<T>;

    std::vector<T> m_container;

  public:
    /* accessors */

    constexpr
    size_t size() const override { return m_container.size(); }

    constexpr
    T & operator[](size_t i) override {
      return m_container[i];
    }

    constexpr
    T operator[](size_t i) const override {
      return m_container[i];
    }

    /* constructors */

    DynamicArray() = default;

    DynamicArray(size_t size, T value = 0)
    : m_container(size, value) { }
    
    DynamicArray(std::span<T> values) {
      m_container.assign(values.begin(), values.end());
    }  

    /* operators */

    met_array_decl_operators(Base, DynamicArray, T);
    met_array_decl_reductions(DynamicArray, T);
    met_array_decl_comparators(DynamicArray, T);
  };
  // met_array_decl_operands(DynamicArray<float>, float);
  // met_array_decl_operands(DynamicArray<uint>, uint);
  // met_array_decl_operands(DynamicArray<int>, int);
} // namespace met