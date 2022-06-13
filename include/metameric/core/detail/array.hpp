#pragma once

#define met_array_decl_op_add(Ty)                               \
  constexpr Ty operator+(const Ty &a) const noexcept {          \
    auto b = *this;                                             \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      b[i] += a[i];                                             \
    return b;                                                   \
  }                                                             \
  constexpr Ty operator+(const value_type &v) const noexcept {  \
    auto b = *this;                                             \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      b[i] += v;                                                \
    return b;                                                   \
  }                                                             \
  constexpr Ty & operator+=(const Ty &a) noexcept {             \
    auto &b = *this;                                            \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      b[i] += a[i];                                             \
    return b;                                                   \
  }                                                             \
  constexpr Ty & operator+=(const value_type &v) noexcept {     \
    auto &b = *this;                                            \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      b[i] += v;                                                \
    return b;                                                   \
  }

#define met_array_decl_op_sub(Ty)                               \
  constexpr Ty operator-(const Ty &a) const noexcept {          \
    auto b = *this;                                             \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      b[i] -= a[i];                                             \
    return b;                                                   \
  }                                                             \
  constexpr Ty operator-(const value_type &v) const noexcept {  \
    auto b = *this;                                             \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      b[i] -= v;                                                \
    return b;                                                   \
  }                                                             \
  constexpr Ty & operator-=(const Ty &a) noexcept {             \
    auto &b = *this;                                            \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      b[i] -= a[i];                                             \
    return b;                                                   \
  }                                                             \
  constexpr Ty & operator-=(const value_type &v) noexcept {     \
    auto &b = *this;                                            \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      b[i] -= v;                                                \
    return b;                                                   \
  }

#define met_array_decl_op_mul(Ty)                               \
  constexpr Ty operator*(const Ty &a) const noexcept {          \
    auto b = *this;                                             \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      b[i] *= a[i];                                             \
    return b;                                                   \
  }                                                             \
  constexpr Ty operator*(const value_type &v) const noexcept {  \
    auto b = *this;                                             \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      b[i] *= v;                                                \
    return b;                                                   \
  }                                                             \
  constexpr Ty & operator*=(const Ty &a) noexcept {             \
    auto &b = *this;                                            \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      b[i] *= a[i];                                             \
    return b;                                                   \
  }                                                             \
  constexpr Ty & operator*=(const value_type &v) noexcept {     \
    auto &b = *this;                                            \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      b[i] *= v;                                                \
    return b;                                                   \
}

#define met_array_decl_op_div(Ty)                               \
  constexpr Ty operator/(const Ty &a) const noexcept {          \
    auto b = *this;                                             \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      b[i] /= a[i];                                             \
    return b;                                                   \
  }                                                             \
  constexpr Ty operator/(const value_type &v) const noexcept {  \
    auto b = *this;                                             \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      b[i] /= v;                                                \
    return b;                                                   \
  }                                                             \
  constexpr Ty & operator/=(const Ty &a) noexcept {             \
    auto &b = *this;                                            \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      b[i] /= a[i];                                             \
    return b;                                                   \
  }                                                             \
  constexpr Ty & operator/=(const value_type &v) noexcept {     \
    auto &b = *this;                                            \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      b[i] /= v;                                                \
    return b;                                                   \
  }

#define met_array_decl_op_com(Ty)                               \
  constexpr mask_type operator==(const Ty &a) const noexcept {  \
    mask_type m;                                                \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      m[i] = base_type::operator[](i) == a[i];                  \
    return m;                                                   \
  }                                                             \
  constexpr mask_type operator!=(const Ty &a) const noexcept {  \
    mask_type m;                                                \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      m[i] = base_type::operator[](i) != a[i];                  \
    return m;                                                   \
  }                                                             \
  constexpr mask_type operator>=(const Ty &a) const noexcept {  \
    mask_type m;                                                \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      m[i] = base_type::operator[](i) >= a[i];                  \
    return m;                                                   \
  }                                                             \
  constexpr mask_type operator<=(const Ty &a) const noexcept {  \
    mask_type m;                                                \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      m[i] = base_type::operator[](i) <= a[i];                  \
    return m;                                                   \
  }                                                             \
  constexpr mask_type operator>(const Ty &a) const noexcept {   \
    mask_type m;                                                \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      m[i] = base_type::operator[](i) > a[i];                   \
    return m;                                                   \
  }                                                             \
  constexpr mask_type operator<(const Ty &a) const noexcept {   \
    mask_type m;                                                \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      m[i] = base_type::operator[](i) < a[i];                   \
    return m;                                                   \
  }                                                             

#define met_array_decl_red_val(Ty)                              \
  constexpr value_type min_value() const noexcept {             \
    return *std::min_element(base_type::begin(),                \
                             base_type::end());                 \
  }                                                             \
  constexpr value_type max_value() const noexcept {             \
    return *std::max_element(base_type::begin(),                \
                             base_type::end());                 \
  }                                                             \
  constexpr value_type sum() const noexcept {                   \
    return std::reduce(++base_type::begin(),                      \
                       base_type::end(),                        \
                       *base_type::begin(),                     \
                       std::plus<>());                          \
  }                                                             \
  constexpr value_type prod() const noexcept {                  \
    return std::reduce(++base_type::begin(),                      \
                       base_type::end(),                        \
                       *base_type::begin(),                     \
                       std::multiplies<>());                    \
  }                                                             

#define met_array_decl_mod_val(Ty)                              \
  constexpr Ty min(const Ty &a) const noexcept {                \
    Ty b;                                                       \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      b[i] = std::min(base_type::operator[](i), a[i]);          \
    return b;                                                   \
  }                                                             \
  constexpr Ty max(const Ty &a) const noexcept {                \
    Ty b;                                                       \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      b[i] = std::max(base_type::operator[](i), a[i]);          \
    return b;                                                   \
  }                                                             \
  constexpr Ty clamp(const Ty &min,                             \
                     const Ty &max) const noexcept {            \
    Ty b;                                                       \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      b[i] = std::clamp(base_type::operator[](i),               \
                        min[i], max[i]);                        \
    return b;                                                   \
  }                                                             \
  constexpr Ty clamp(const value_type &min,                     \
                     const value_type &max) const noexcept {    \
    Ty b;                                                       \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      b[i] = std::clamp(base_type::operator[](i), min, max);    \
    return b;                                                   \
  }                                                             \
  constexpr value_type dot(const Ty &a) const noexcept {        \
    value_type v = 0;                                           \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      v += base_type::operator[](i) * a[i];                     \
    return v;                                                   \
  }                                                             \
  friend constexpr                                              \
  Ty min(const Ty &a, const Ty &b) noexcept {                   \
    Ty c;                                                       \
    for (size_type i = 0; i < a.size(); ++i)                    \
      c[i] = std::min(a[i], b[i]);                              \
    return c;                                                   \
  }                                                             \
  friend constexpr                                              \
  Ty max(const Ty &a, const Ty &b) noexcept {                   \
    Ty c;                                                       \
    for (size_type i = 0; i < a.size(); ++i)                    \
      c[i] = std::max(a[i], b[i]);                              \
    return c;                                                   \
  }                                                             \
  friend constexpr                                              \
  Ty clamp(const Ty &a, const Ty &min, const Ty &max) noexcept {\
    Ty b;                                                       \
    for (size_type i = 0; i < a.size(); ++i)                    \
      b[i] = std::clamp(a[i], min[i], max[i]);                  \
    return b;                                                   \
  }                                                             \
  friend constexpr                                              \
  Ty clamp(const Ty &a, const value_type &min,                  \
                        const value_type &max) noexcept {       \
    Ty b;                                                       \
    for (size_type i = 0; i < a.size(); ++i)                    \
      b[i] = std::clamp(a[i], min, max);                        \
    return b;                                                   \
  }                                                             \
  friend constexpr                                              \
  value_type dot(const Ty &a, const Ty &b) noexcept {           \
    value_type v = 0;                                           \
    for (size_type i = 0; i < a.size(); ++i)                    \
      v += a[i] * b[i];                                         \
    return v;                                                   \
  }                                                             