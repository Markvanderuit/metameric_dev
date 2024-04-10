#include <metameric/core/utility.hpp>
#include <metameric/components/views/detail/file_dialog.hpp>
#include <nfd.h>

namespace met::detail {
  bool load_dialog(fs::path &path, const std::string &type_filter) {
    nfdchar_t  *out = nullptr;
    nfdresult_t res = NFD_OpenDialog(type_filter.c_str(), nullptr, &out);
    guard(res == NFD_OKAY, false);

    path = out;
    free(out);

    return true;
  }

  bool load_dialog_mult(std::vector<fs::path> &paths, const std::string &type_filter) {
    nfdpathset_t out;
    nfdresult_t  res = NFD_OpenDialogMultiple(type_filter.c_str(), nullptr, &out);
    guard(res == NFD_OKAY, false);
    
    paths.clear();
    for (uint i = 0; i < NFD_PathSet_GetCount(&out); ++i) {
      nfdchar_t *p = NFD_PathSet_GetPath(&out, i);
      paths.push_back(p);
    }

    NFD_PathSet_Free(&out);

    return true;
  }


  bool save_dialog(fs::path &path, const std::string &type_filter) {
    nfdchar_t  *out = nullptr;
    nfdresult_t res = NFD_SaveDialog(type_filter.c_str(), nullptr, &out);
    guard(res == NFD_OKAY, false);
    
    path = out;
    free(out);
    
    return true;
  }
} // namespace met::detail