#pragma once

#define met_array_decl_op_add(Ty)                               \
constexpr Ty operator+(const Ty &a) const noexcept {            \
  auto b = *this;                                               \
  for (size_type i = 0; i < base_type::size(); ++i)             \
    b[i] += a[i];                                               \
  return b;                                                     \
}                                                               \
constexpr Ty operator+(const value_type &v) const noexcept {    \
  auto b = *this;                                               \
  for (size_type i = 0; i < base_type::size(); ++i)             \
    b[i] += v;                                                  \
  return b;                                                     \
}                                                               \
constexpr Ty & operator+=(const Ty &a) noexcept {               \
  auto &b = *this;                                              \
  for (size_type i = 0; i < base_type::size(); ++i)             \
    b[i] += a[i];                                               \
  return b;                                                     \
}                                                               \
constexpr Ty & operator+=(const value_type &v) noexcept {       \
  auto &b = *this;                                              \
  for (size_type i = 0; i < base_type::size(); ++i)             \
    b[i] += v;                                                  \
  return b;                                                     \
}

#define met_array_decl_op_sub(Ty)                               \
constexpr Ty operator-(const Ty &a) const noexcept {            \
  auto b = *this;                                               \
  for (size_type i = 0; i < base_type::size(); ++i)             \
    b[i] -= a[i];                                               \
  return b;                                                     \
}                                                               \
constexpr Ty operator-(const value_type &v) const noexcept {    \
  auto b = *this;                                               \
  for (size_type i = 0; i < base_type::size(); ++i)             \
    b[i] -= v;                                                  \
  return b;                                                     \
}                                                               \
constexpr Ty & operator-=(const Ty &a) noexcept {               \
  auto &b = *this;                                              \
  for (size_type i = 0; i < base_type::size(); ++i)             \
    b[i] -= a[i];                                               \
  return b;                                                     \
}                                                               \
constexpr Ty & operator-=(const value_type &v) noexcept {       \
  auto &b = *this;                                              \
  for (size_type i = 0; i < base_type::size(); ++i)             \
    b[i] -= v;                                                  \
  return b;                                                     \
}

#define met_array_decl_op_mul(Ty)                               \
constexpr Ty operator*(const Ty &a) const noexcept {            \
  auto b = *this;                                               \
  for (size_type i = 0; i < base_type::size(); ++i)             \
    b[i] *= a[i];                                               \
  return b;                                                     \
}                                                               \
constexpr Ty operator*(const value_type &v) const noexcept {    \
  auto b = *this;                                               \
  for (size_type i = 0; i < base_type::size(); ++i)             \
    b[i] *= v;                                                  \
  return b;                                                     \
}                                                               \
constexpr Ty & operator*=(const Ty &a) noexcept {               \
  auto &b = *this;                                              \
  for (size_type i = 0; i < base_type::size(); ++i)             \
    b[i] *= a[i];                                               \
  return b;                                                     \
}                                                               \
constexpr Ty & operator*=(const value_type &v) noexcept {       \
  auto &b = *this;                                              \
  for (size_type i = 0; i < base_type::size(); ++i)             \
    b[i] *= v;                                                  \
  return b;                                                     \
}

#define met_array_decl_op_div(Ty)                               \
constexpr Ty operator/(const Ty &a) const noexcept {            \
  auto b = *this;                                               \
  for (size_type i = 0; i < base_type::size(); ++i)             \
    b[i] /= a[i];                                               \
  return b;                                                     \
}                                                               \
constexpr Ty operator/(const value_type &v) const noexcept {    \
  auto b = *this;                                               \
  for (size_type i = 0; i < base_type::size(); ++i)             \
    b[i] /= v;                                                  \
  return b;                                                     \
}                                                               \
constexpr Ty & operator/=(const Ty &a) noexcept {               \
  auto &b = *this;                                              \
  for (size_type i = 0; i < base_type::size(); ++i)             \
    b[i] /= a[i];                                               \
  return b;                                                     \
}                                                               \
constexpr Ty & operator/=(const value_type &v) noexcept {       \
  auto &b = *this;                                              \
  for (size_type i = 0; i < base_type::size(); ++i)             \
    b[i] /= v;                                                  \
  return b;                                                     \
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
    value_type v = base_type::operator[](0);                    \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      v = std::min(v, base_type::operator[](i));                \
    return v;                                                   \
  }                                                             \
  constexpr value_type max_value() const noexcept {             \
    value_type v = base_type::operator[](0);                    \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      v = std::max(v, base_type::operator[](i));                \
    return v;                                                   \
  }                                                             \
  constexpr value_type sum() const noexcept {                   \
    value_type v = base_type::operator[](0);                    \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      v += base_type::operator[](i);                            \
    return v;                                                   \
  }                                                             \
  constexpr value_type prod() const noexcept {                  \
    value_type v = base_type::operator[](0);                    \
    for (size_type i = 0; i < base_type::size(); ++i)           \
      v *= base_type::operator[](i);                            \
    return v;                                                   \
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

#define met_array_decl_operator_add(Base, Ty, T)      \
  constexpr Ty operator+(const Base &a) {             \
    Ty b = *this;                                     \
    for (size_t i = 0; i < size(); ++i)               \
      b[i] *= a[i];                                   \
    return b;                                         \
  }                                                   \
  constexpr Ty operator+(const T &t) {                \
    Ty b = *this;                                     \
    for (size_t i = 0; i < size(); ++i)               \
      b[i] += t;                                      \
    return b;                                         \
  }                                                   \
  constexpr Ty & operator+=(const Base &a) {          \
    for (size_t i = 0; i < size(); ++i)               \
      operator[](i) += a[i];                          \
    return *this;                                     \
  }                                                   \
  constexpr Ty & operator+=(const T &t) {             \
    for (size_t i = 0; i < size(); ++i)               \
      operator[](i) += t;                             \
    return *this;                                     \
  }   
  
#define met_array_decl_operator_sub(Base, Ty, T)      \
  constexpr Ty operator-(const Base &a) {             \
    Ty b = *this;                                     \
    for (size_t i = 0; i < size(); ++i)               \
      b[i] -= a[i];                                   \
    return b;                                         \
  }                                                   \
  constexpr Ty operator-(const T &t) {                \
    Ty b = *this;                                     \
    for (size_t i = 0; i < size(); ++i)               \
      b[i] -= t;                                      \
    return b;                                         \
  }                                                   \
  constexpr Ty & operator-=(const Base &a) {          \
    for (size_t i = 0; i < size(); ++i)               \
      operator[](i) -= a[i];                          \
    return *this;                                     \
  }                                                   \
  constexpr Ty & operator-=(const T &t) {             \
    for (size_t i = 0; i < size(); ++i)               \
      operator[](i) -= t;                             \
    return *this;                                     \
  }   

#define met_array_decl_operator_mul(Base, Ty, T)      \
  constexpr Ty operator*(const Base &a) {             \
    Ty b = *this;                                     \
    for (size_t i = 0; i < size(); ++i)               \
      b[i] *= a[i];                                   \
    return b;                                         \
  }                                                   \
  constexpr Ty operator*(const T &t) {                \
    Ty b = *this;                                     \
    for (size_t i = 0; i < size(); ++i)               \
      b[i] *= t;                                      \
    return b;                                         \
  }                                                   \
  constexpr Ty & operator*=(const Base &a) {          \
    for (size_t i = 0; i < size(); ++i)               \
      operator[](i) *= a[i];                          \
    return *this;                                     \
  }                                                   \
  constexpr Ty & operator*=(const T &t) {             \
    for (size_t i = 0; i < size(); ++i)               \
      operator[](i) *= t;                             \
    return *this;                                     \
  }   

#define met_array_decl_operator_div(Base, Ty, T)      \
  constexpr Ty operator/(const Base &a) {             \
    Ty b = *this;                                     \
    for (size_t i = 0; i < size(); ++i)               \
      b[i] /= a[i];                                   \
    return b;                                         \
  }                                                   \
  constexpr Ty operator/(const T &t) {                \
    Ty b = *this;                                     \
    for (size_t i = 0; i < size(); ++i)               \
      b[i] /= t;                                      \
    return b;                                         \
  }                                                   \
  constexpr Ty & operator/=(const Base &a) {          \
    for (size_t i = 0; i < size(); ++i)               \
      operator[](i) /= a[i];                          \
    return *this;                                     \
  }                                                   \
  constexpr Ty & operator/=(const T &t) {             \
    for (size_t i = 0; i < size(); ++i)               \
      operator[](i) /= t;                             \
    return *this;                                     \
  }   

#define met_array_decl_reductions(Ty, T)              \
  constexpr T prod() const {                          \
    T t = operator[](0);                              \
    for (size_t i = 1; i < size(); ++i)               \
      t *= operator[](i);                             \
    return t;                                         \
  }                                                   \
  constexpr friend T prod(const Ty &a) {              \
    return a.prod();                                  \
  }                                                   \
  constexpr T sum() const {                           \
    T t = operator[](0);                              \
    for (size_t i = 1; i < size(); ++i)               \
      t += operator[](i);                             \
    return t;                                         \
  }                                                   \
  constexpr friend T sum(const Ty &a) {               \
    return a.sum();                                   \
  }                                                   \
  constexpr T mean() const {                          \
    return sum() / static_cast<T>(size());            \
  }                                                   \
  constexpr friend T mean(const Ty &a) {              \
    return a.mean();                                  \
  }                                                   \
  constexpr T min_value() const {                     \
    T t = operator[](0);                              \
    for (size_t i = 1; i < size(); ++i)               \
      t = std::min(t, operator[](i));                 \
    return t;                                         \
  }                                                   \
  constexpr friend T min_value(const Ty &a) {         \
    return a.min_value();                             \
  }                                                   \
  constexpr T max_value() const {                     \
    T t = operator[](0);                              \
    for (size_t i = 1; i < size(); ++i)               \
      t = std::max(t, operator[](i));                 \
    return t;                                         \
  }                                                   \
  constexpr friend T max_value(const Ty &a) {         \
    return a.max_value();                             \
  }                                                  

#define met_array_decl_comparators(Ty, T)             \
  constexpr Ty clamp(const T &min, const T &max)      \
  const {                                             \
    Ty b = *this;                                     \
    for (size_t i = 0; i < size(); ++i)               \
      b[i] = std::clamp(b[i], min, max);              \
    return b;                                         \
  }                                                   \
  constexpr friend Ty clamp(const Ty &a,              \
                            const T &min,             \
                            const T &max) {           \
    return a.clamp(min, max);                         \
  }                                                   \
  constexpr Ty clamp(const Ty &min, const Ty &max)    \
  const {                                             \
    Ty b = *this;                                     \
    for (size_t i = 0; i < size(); ++i)               \
      b[i] = std::clamp(b[i], min[i], max[i]);        \
    return b;                                         \
  }                                                   \
  constexpr friend Ty clamp(const Ty &a,              \
                            const Ty &min,            \
                            const Ty &max) {          \
    return a.clamp(min, max);                         \
  }                                                   \
  constexpr Ty min(const Ty &a) const {               \
    Ty b = *this;                                     \
    for (size_t i = 0; i < size(); ++i)               \
      b[i] = std::min(b[i], a[i]);                    \
    return b;                                         \
  }                                                   \
  constexpr friend Ty min(const Ty &a, const Ty &b) { \
    return a.min(b);                                  \
  }                                                   \
  constexpr Ty max(const Ty &a) const {               \
    Ty b = *this;                                     \
    for (size_t i = 0; i < size(); ++i)               \
      b[i] = std::max(b[i], a[i]);                    \
    return b;                                         \
  }                                                   \
  constexpr friend Ty max(const Ty &a, const Ty &b) { \
    return a.max(b);                                  \
  }                                                   \
  constexpr T dot(const Ty &a) const {                \
    T t = 0;                                          \
    for (size_t i = 0; i < size(); ++i)               \
      t += operator[](i) * a[i];                      \
    return t;                                         \
  }                                                   \
  constexpr friend T dot(const Ty &a, const Ty &b) {  \
    return a.dot(b);                                  \
  }

#define met_array_decl_operators(Base, Ty, T)         \
  met_array_decl_operator_add(Base, Ty, T);           \
  met_array_decl_operator_sub(Base, Ty, T);           \
  met_array_decl_operator_mul(Base, Ty, T);           \
  met_array_decl_operator_div(Base, Ty, T);