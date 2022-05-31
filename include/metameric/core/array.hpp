#pragma once

#include <array>
#include <iterator>
#include <vector>

namespace met {
  template <typename T>
  class VirtualArray {
    using Iterator = std::iterator<std::input_iterator_tag, T>;
    
  public:
    /* data access */

    virtual T* data();
    virtual size_t size();
    
    /* scalar operators */

    VirtualArray operator+(const T &t) {
      VirtualArray a = *this;
      for (size_t i = 0; i < size(); ++i)
        a[i] += t;
      return a;
    }

    VirtualArray operator-(const T &t) {
      VirtualArray a = *this;
      for (size_t i = 0; i < size(); ++i)
        a[i] -= t;
      return a;
    }
    
    VirtualArray operator*(const T &t) {
      VirtualArray a = *this;
      for (size_t i = 0; i < size(); ++i)
        a[i] *= t;
      return a;
    }

    VirtualArray operator/(const T &t) {
      VirtualArray a = *this;
      for (size_t i = 0; i < size(); ++i)
        a[i] /= t;
      return a;
    }

    VirtualArray& operator+=(const T &t) {
      for (size_t i = 0; i < size(); ++i)
        operator[](i) += t;
      return *this;
    }

    VirtualArray& operator-=(const T &t) {
      for (size_t i = 0; i < size(); ++i)
        operator[](i) -= t;
      return *this;
    }

    VirtualArray& operator*=(const T &t) {
      for (size_t i = 0; i < size(); ++i)
        operator[](i) *= t;
      return *this;
    }

    VirtualArray& operator/=(const T &t) {
      for (size_t i = 0; i < size(); ++i)
        operator[](i) /= t;
      return *this;
    }

    /* vector operators */

    virtual VirtualArray operator+(const VirtualArray &a) = 0;
    virtual VirtualArray operator-(const VirtualArray &a) = 0;
    virtual VirtualArray operator*(const VirtualArray &a) = 0;
    virtual VirtualArray operator/(const VirtualArray &a) = 0;

    virtual VirtualArray& operator+=(const VirtualArray &a) = 0;
    virtual VirtualArray& operator-=(const VirtualArray &a) = 0;
    virtual VirtualArray& operator*=(const VirtualArray &a) = 0;
    virtual VirtualArray& operator/=(const VirtualArray &a) = 0;
  };

  template <typename T, size_t Size>
  class StaticArray {
    std::array<T, Size> m_c;

  public:
    /* constr/destr */

    StaticArray();
    StaticArray(T v);
    StaticArray(T* vp);

    /* Accessors */

    size_t size() const;
    T* data() const;

    T& operator[](const size_t i);
    T operator[](const size_t i) const;

    /* scalar operators */

    StaticArray operator+(const T &t);
    StaticArray operator-(const T &t);
    StaticArray operator*(const T &t);
    StaticArray operator/(const T &t);

    StaticArray& operator+=(const T &t);
    StaticArray& operator-=(const T &t);
    StaticArray& operator*=(const T &t);
    StaticArray& operator/=(const T &t);

    /* vector operators */

    StaticArray operator+(const StaticArray &a);
    StaticArray operator-(const StaticArray &a);
    StaticArray operator*(const StaticArray &a);
    StaticArray operator/(const StaticArray &a);

    StaticArray& operator+=(const StaticArray &a);
    StaticArray& operator-=(const StaticArray &a);
    StaticArray& operator*=(const StaticArray &a);
    StaticArray& operator/=(const StaticArray &a);

    /* reductive operators */

    T sum();
    T prod();
    T mean();
    T length();
    T min();
    T max();
    bool is_all_zero() const;
    bool is_any_zero() const;

    /* component-wise special operators */

    StaticArray abs() const;
    StaticArray sqrt() const;
    StaticArray safe_sqrt() const;
    StaticArray log() const;
    StaticArray exp() const;
    StaticArray pow(T t) const;
    StaticArray clamp(T t_min, T t_max) const;
    StaticArray clamp(const StaticArray & a_min, const StaticArray & a_max) const;
  };

  template <typename T>
  class DynamicArray {
    std::vector<T> m_c;

  public:

  };
} // namespace met