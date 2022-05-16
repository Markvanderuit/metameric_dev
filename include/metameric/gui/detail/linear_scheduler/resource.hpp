#pragma once

namespace met::detail {
  template <typename> struct Resource; // fwd

  struct AbstractResource {
    template <typename T>
    T & get() {
      return static_cast<Resource<T> *>(this)->_object;
    }

    template <typename T>
    const T & get() const {
      return static_cast<Resource<T> *>(this)->_object;
    }
  };

  template <typename T>
  struct Resource : AbstractResource {
    // Move constructor
    Resource(T &&object)
    : _object(std::move(object)) {}

    T _object;
  };
} // namespace met::detail