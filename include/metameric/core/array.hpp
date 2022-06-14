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
  class Array;
  template <size_t Size, 
            template <typename, size_t> typename C = std::array>
  class MaskArray;

  template <typename T, size_t Size, template <typename, size_t> typename C>
  class Array {
    using value_type = T;
    using ref_type   = T &;
    using ptr_type   = T *;
    using cref_type  = const T &;
    using size_type  = size_t;

    using base_type  = Array;
    using mask_type  = MaskArray<Size, C>;

    alignas(sizeof(value_type)) C<value_type, Size> m_cont;
    
  public:
    /* constrs */

    constexpr Array() = default;

    constexpr Array(value_type value) { 
      std::fill(m_cont.begin(), m_cont.end(), value);
    }

    constexpr Array(std::span<value_type> values) {
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

    constexpr auto begin() noexcept { return m_cont.begin(); }

    constexpr auto end() noexcept { return m_cont.end(); }

    constexpr auto begin() const noexcept { return m_cont.begin(); }

    constexpr auto end() const noexcept { return m_cont.end(); }

    /* operators */

    met_array_decl_op_add(Array);
    met_array_decl_op_sub(Array);
    met_array_decl_op_mul(Array);
    met_array_decl_op_div(Array);
    met_array_decl_op_com(Array);

    /* reductions */

    met_array_decl_red_val(Array);
    met_array_decl_mod_val(Array);

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
  class MaskArray : public Array<bool, Size, C> {
    using value_type = bool;
    using ref_type   = bool &;
    using ptr_type   = bool *;
    using cref_type  = const bool &;
    using size_type  = size_t;

    using base_type  = Array<bool, Size, C>;
    using mask_type  = MaskArray;

  public:
    /* constrs */

    using base_type::base_type;

    /* operators */

    met_array_decl_op_com(MaskArray);

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
      return !any();
    }
  };
} // namespace met