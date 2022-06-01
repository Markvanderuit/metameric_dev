#pragma once

#define met_array_decl_operator_add(Base, Ty, T)      \
  Ty operator+(const Base &a) {                       \
    Ty b = *this;                                     \
    for (size_t i = 0; i < size(); ++i)               \
      b[i] *= a[i];                                   \
    return b;                                         \
  }                                                   \
  Ty operator+(const T &t) {                          \
    Ty b = *this;                                     \
    for (size_t i = 0; i < size(); ++i)               \
      b[i] += t;                                      \
    return b;                                         \
  }                                                   \
  Ty & operator+=(const Base &a) {                    \
    for (size_t i = 0; i < size(); ++i)               \
      operator[](i) += a[i];                          \
    return *this;                                     \
  }                                                   \
  Ty & operator+=(const T &t) {                       \
    for (size_t i = 0; i < size(); ++i)               \
      operator[](i) += t;                             \
    return *this;                                     \
  }   
  
#define met_array_decl_operator_sub(Base, Ty, T)      \
  Ty operator-(const Base &a) {                       \
    Ty b = *this;                                     \
    for (size_t i = 0; i < size(); ++i)               \
      b[i] -= a[i];                                   \
    return b;                                         \
  }                                                   \
  Ty operator-(const T &t) {                          \
    Ty b = *this;                                     \
    for (size_t i = 0; i < size(); ++i)               \
      b[i] -= t;                                      \
    return b;                                         \
  }                                                   \
  Ty & operator-=(const Base &a) {                    \
    for (size_t i = 0; i < size(); ++i)               \
      operator[](i) -= a[i];                          \
    return *this;                                     \
  }                                                   \
  Ty & operator-=(const T &t) {                       \
    for (size_t i = 0; i < size(); ++i)               \
      operator[](i) -= t;                             \
    return *this;                                     \
  }   

#define met_array_decl_operator_mul(Base, Ty, T)      \
  Ty operator*(const Base &a) {                       \
    Ty b = *this;                                     \
    for (size_t i = 0; i < size(); ++i)               \
      b[i] *= a[i];                                   \
    return b;                                         \
  }                                                   \
  Ty operator*(const T &t) {                          \
    Ty b = *this;                                     \
    for (size_t i = 0; i < size(); ++i)               \
      b[i] *= t;                                      \
    return b;                                         \
  }                                                   \
  Ty & operator*=(const Base &a) {                    \
    for (size_t i = 0; i < size(); ++i)               \
      operator[](i) *= a[i];                          \
    return *this;                                     \
  }                                                   \
  Ty & operator*=(const T &t) {                       \
    for (size_t i = 0; i < size(); ++i)               \
      operator[](i) *= t;                             \
    return *this;                                     \
  }   

#define met_array_decl_operator_div(Base, Ty, T)      \
  Ty operator/(const Base &a) {                       \
    Ty b = *this;                                     \
    for (size_t i = 0; i < size(); ++i)               \
      b[i] /= a[i];                                   \
    return b;                                         \
  }                                                   \
  Ty operator/(const T &t) {                          \
    Ty b = *this;                                     \
    for (size_t i = 0; i < size(); ++i)               \
      b[i] /= t;                                      \
    return b;                                         \
  }                                                   \
  Ty & operator/=(const Base &a) {                    \
    for (size_t i = 0; i < size(); ++i)               \
      operator[](i) /= a[i];                          \
    return *this;                                     \
  }                                                   \
  Ty & operator/=(const T &t) {                       \
    for (size_t i = 0; i < size(); ++i)               \
      operator[](i) /= t;                             \
    return *this;                                     \
  }   

#define met_array_decl_reductions(Base, Ty, T)        \
  T prod() {                                          \
    T t = operator[](0);                              \
    for (size_t i = 1; i < size(); ++i)               \
      t *= operator[](i);                             \
    return t;                                         \
  }                                                   \
  T sum() {                                           \
    T t = operator[](0);                              \
    for (size_t i = 1; i < size(); ++i)               \
      t += operator[](i);                             \
    return t;                                         \
  }

/* 
  a.clamp(x, y)
  a.min_value()
  a.max_value()
  clamp(a, x, y)
  min(a, b)
  max(a, b)
  dot(a, b)
 */

#define met_array_decl_operators(Base, Ty, T)         \
  met_array_decl_operator_add(Base, Ty, T);           \
  met_array_decl_operator_sub(Base, Ty, T);           \
  met_array_decl_operator_mul(Base, Ty, T);           \
  met_array_decl_operator_div(Base, Ty, T);