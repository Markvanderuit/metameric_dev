#pragma once

#include <array>
#include <span>
#include <vector>
#include <metameric/core/detail/array.hpp>

namespace met {
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