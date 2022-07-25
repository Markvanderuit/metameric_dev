#include <metameric/components/views/detail/file_dialog.hpp>
#include <nfd.h>
#include <fmt/core.h>

namespace met::detail {
  bool load_dialog(fs::path &path, const std::string &type_filter) {
    nfdchar_t  *out = nullptr;
    nfdresult_t res = NFD_OpenDialog(type_filter.c_str(), nullptr, &out);

    if (res == NFD_OKAY) {
      path = out;
      free(out);
      return true;
    }
    
    return false;
  }

  bool save_dialog(fs::path &path, const std::string &type_filter) {
    nfdchar_t  *out = nullptr;
    nfdresult_t res = NFD_SaveDialog(type_filter.c_str(), nullptr, &out);

    if (res == NFD_OKAY) {
      path = out;
      free(out);
      return true;
    }
    
    return false;
  }
} // namespace met::detail