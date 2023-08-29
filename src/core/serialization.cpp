#include <metameric/core/serialization.hpp>
#include <type_traits>

namespace met::io {
  /* Specialized serialization for vectors of generic objects */

  /* template <>
  void to_stream(const std::vector<size_t> &v, std::ostream &str) {
    met_trace();
    str.write(reinterpret_cast<const char *>(v.data()), 
              sizeof(std::decay_t<decltype(v)>::value_type) * v.size());
  } */

  /* template <>
  void fr_stream(std::vector<size_t> &v, std::istream &str) {
    met_trace();
    str.read(reinterpret_cast<char *>(v.data()), 
             sizeof(std::decay_t<decltype(v)>::value_type) * v.size());
  } */

  /* Specialized serialization for generic objects */

  template <>
  void to_stream(const size_t &ty, std::ostream &str) {
    met_trace();
    str.write(reinterpret_cast<const char *>(&ty), sizeof(std::decay_t<decltype(ty)>));
  }

  template <>
  void fr_stream(size_t &ty, std::istream &str) {
    met_trace();
    str.read(reinterpret_cast<char *>(&ty), sizeof(std::decay_t<decltype(ty)>));
  }

  template <>
  void to_stream(const std::string &ty, std::ostream &str) {
    met_trace();
    size_t size = ty.size();
    to_stream(size, str);
    str.write(ty.data(), size);
  }

  template <>
  void fr_stream(std::string &ty, std::istream &str) {
    met_trace();
    size_t size = 0;
    fr_stream(size, str);
    ty.resize(size);
    str.read(ty.data(), size);
  }
} // namespace met::io