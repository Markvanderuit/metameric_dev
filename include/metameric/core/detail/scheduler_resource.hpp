#pragma once

#include <utility>

namespace met::detail {
  template <typename> struct Resource; // fwd

  struct AbstractResource {
    template <typename T>
    T & get_as() {
      return static_cast<Resource<T> *>(this)->m_object;
    }

    template <typename T>
    const T & get_as() const {
      return static_cast<Resource<T> *>(this)->m_object;
    }
  };

  template <typename T>
  struct Resource : AbstractResource {
    Resource(T &&object)
    : m_object(std::move(object)) { }

    T m_object;
  };
} // namespace met::detail