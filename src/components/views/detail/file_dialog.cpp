#include <metameric/components/views/detail/file_dialog.hpp>
#include <nfd.h>
#include <fmt/core.h>

namespace met::detail {
  FileDialogResultType open_file_dialog(std::filesystem::path &path, const std::string &type_filter) {
    nfdchar_t  *out = nullptr;
    nfdresult_t res = NFD_OpenDialog(type_filter.c_str(), nullptr, &out);

    if (res == NFD_OKAY) {
      path = out;
      free(out);
    }

    return FileDialogResultType(res);
  }

  FileDialogResultType save_file_dialog(std::filesystem::path &path, const std::string &type_filter) {
    nfdchar_t  *out = nullptr;
    nfdresult_t res = NFD_SaveDialog(type_filter.c_str(), nullptr, &out);

    if (res == NFD_OKAY) {
      path = out;
      free(out);
    }

    return FileDialogResultType(res);
  }
} // namespace met::detail