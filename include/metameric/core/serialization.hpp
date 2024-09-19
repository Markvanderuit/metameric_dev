#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/utility.hpp>
#include <istream>
#include <ostream>

namespace met::io {
  // Simple serializable contract to avoid use of interfaces on currently
  // aggregate types such as MeshData
  template <typename Ty>
  concept is_serializable = requires(Ty ty, std::istream &is, std::ostream &os) {
    { ty.to_stream(os) };
    { ty.fr_stream(is) };
  };

  // Serialization for eigen dense types
  template <typename Ty> requires (!is_serializable<Ty> && eig::is_approx_comparable<Ty>)
  void to_stream(const Ty &ty, std::ostream &str) {
    str.write(reinterpret_cast<const char *>(ty.data()), sizeof(std::decay_t<decltype(ty)>));
  }
  template <typename Ty> requires (!is_serializable<Ty> && eig::is_approx_comparable<Ty>)
  void fr_stream(Ty &ty, std::istream &str) {
    str.read(reinterpret_cast<char *>(ty.data()), sizeof(std::decay_t<decltype(ty)>));
  }

  // Serialization for most types
  template <typename Ty> requires (!is_serializable<Ty> && !eig::is_approx_comparable<Ty>)
  void to_stream(const Ty &ty, std::ostream &str) {
    met_trace();
    str.write(reinterpret_cast<const char *>(&ty), sizeof(std::decay_t<decltype(ty)>));
  }
  template <typename Ty> requires (!is_serializable<Ty> && !eig::is_approx_comparable<Ty>)
  void fr_stream(Ty &ty, std::istream &str) {
    met_trace();
    str.read(reinterpret_cast<char *>(&ty), sizeof(std::decay_t<decltype(ty)>));
  }

  // Serialization for std::string
  inline void to_stream(const std::string &ty, std::ostream &str) {
    met_trace();
    size_t size = ty.size();
    to_stream(size, str);
    str.write(ty.data(), size);
  }
  inline void fr_stream(std::string &ty, std::istream &str) {
    met_trace();
    size_t size = 0;
    fr_stream(size, str);
    ty.resize(size);
    str.read(ty.data(), size);
  }

  // Serialization for std::vector of most types
  template <typename Ty> requires (!is_serializable<Ty> && !eig::is_approx_comparable<Ty>)
  void to_stream(const std::vector<Ty> &v, std::ostream &str) {
    met_trace();
    size_t n = v.size();
    to_stream(n, str);
    str.write(reinterpret_cast<const char *>(v.data()), sizeof(std::decay_t<decltype(v)>::value_type) * v.size());
  }
  template <typename Ty> requires (!is_serializable<Ty> && !eig::is_approx_comparable<Ty>)
  void fr_stream(std::vector<Ty> &v, std::istream &str) {
    met_trace();
    size_t n = 0;
    fr_stream(n, str);
    v.resize(n);
    str.read(reinterpret_cast<char *>(v.data()), sizeof(std::decay_t<decltype(v)>::value_type) * v.size());
  }

  // Serialization for objects fulfilling is_serializable contract
  template <typename Ty> requires (is_serializable<Ty>)
  void to_stream(const Ty &ty, std::ostream &str) { 
    ty.to_stream(str);
  }
  template <typename Ty> requires (is_serializable<Ty>)
  void fr_stream(Ty &ty, std::istream &str) { 
    ty.fr_stream(str);
  }

  // Serialization for vectors of objects fulfilling is_serializable contract
  template <typename Ty> requires (is_serializable<Ty>)
  void to_stream(const std::vector<Ty> &v, std::ostream &str) {
    met_trace();
    size_t n = v.size();
    to_stream(n, str);
    for (const auto &ty : v)
      to_stream(ty, str);
  }
  template <typename Ty> requires (is_serializable<Ty>)
  void fr_stream(std::vector<Ty> &v, std::istream &str) {
    met_trace();
    size_t n = 0;
    fr_stream(n, str);
    v.resize(n);
    for (auto &ty : v)
      fr_stream(ty, str);
  }

  // Serialization for vectors of eigen types
  template <typename Ty> requires (!is_serializable<Ty> && eig::is_approx_comparable<Ty>)
  void to_stream(const std::vector<Ty> &v, std::ostream &str) {
    met_trace();
    size_t n = v.size();
    to_stream(n, str);
    str.write(reinterpret_cast<const char *>(v.data()), sizeof(std::decay_t<decltype(v)>::value_type) * n);
  }
  template <typename Ty> requires (!is_serializable<Ty> && eig::is_approx_comparable<Ty>)
  void fr_stream(std::vector<Ty> &v, std::istream &str) {
    met_trace();
    size_t n = 0;
    fr_stream(n, str);
    v.resize(n);
    str.read(reinterpret_cast<char *>(v.data()), sizeof(std::decay_t<decltype(v)>::value_type) * n);
  }
} // namespace met::io