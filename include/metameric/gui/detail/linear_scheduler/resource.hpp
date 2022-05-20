#pragma once

namespace met::detail {
  template <typename> struct Resource; // fwd

  struct AbstractResource {
    template <typename T>
    T & get() {
      return static_cast<Resource<T> *>(this)->m_object;
    }

    template <typename T>
    const T & get() const {
      return static_cast<Resource<T> *>(this)->m_object;
    }
  };

  template <typename T>
  struct Resource : AbstractResource {
    // Move constructor
    Resource(T &&object)
    : m_object(std::move(object)) {}

    T m_object;
  };
} // namespace met::detail