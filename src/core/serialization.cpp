#include <metameric/core/serialization.hpp>
#include <metameric/core/image.hpp>
#include <type_traits>

namespace met::io {
  /* Specialized serialization for vectors of generic objects */
  
  // Currently unused
  /* template <>
  void to_stream(const std::vector<size_t> &v, std::ostream &str) {
    met_trace();
    str.write(reinterpret_cast<const char *>(v.data()), 
              sizeof(std::decay_t<decltype(v)>::value_type) * v.size());
  } */

  // Currently unused
  /* template <>
  void fr_stream(std::vector<size_t> &v, std::istream &str) {
    met_trace();
    str.read(reinterpret_cast<char *>(v.data()), 
             sizeof(std::decay_t<decltype(v)>::value_type) * v.size());
  } */

  
  /* template <typename Ty> requires (!is_serializable<Ty> && !is_approx_comparable<Ty>)
  void to_stream(const Ty &ty, std::ostream &str) {

  }

  template <typename Ty> requires (!is_serializable<Ty> && !is_approx_comparable<Ty>)
  void fr_stream(Ty &ty, std::istream &str) {

  } */

  /* template <>
  inline
  void to_stream<std::byte>(const std::vector<std::byte> &v, std::ostream &str) {
    met_trace();
    str.write(reinterpret_cast<const char *>(v.data()), 
              sizeof(std::decay_t<decltype(v)>::value_type) * v.size());
  }

  template <>
  inline
  void fr_stream<std::byte>(std::vector<std::byte> &v, std::istream &str) {
    met_trace();
    str.read(reinterpret_cast<char *>(v.data()), 
             sizeof(std::decay_t<decltype(v)>::value_type) * v.size());
  } */

  /* Specialized serialization for generic objects *//* 

  template <>
  inline
  void to_stream<size_t>(const size_t &ty, std::ostream &str) {
    met_trace();
    str.write(reinterpret_cast<const char *>(&ty), sizeof(std::decay_t<decltype(ty)>));
  }

  template <>
  inline
  void fr_stream<size_t>(size_t &ty, std::istream &str) {
    met_trace();
    str.read(reinterpret_cast<char *>(&ty), sizeof(std::decay_t<decltype(ty)>));
  } */

  /* template <>
  void to_stream<std::string>(const std::string &ty, std::ostream &str) {
    met_trace();
    size_t size = ty.size();
    to_stream(size, str);
    str.write(ty.data(), size);
  }

  template <>
  void fr_stream<std::string>(std::string &ty, std::istream &str) {
    met_trace();
    size_t size = 0;
    fr_stream(size, str);
    ty.resize(size);
    str.read(ty.data(), size);
  } */
} // namespace met::io