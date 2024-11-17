#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/detail/file_dialog.hpp>
#include <vector>
#include <nfd.h>

namespace met::detail {
  bool load_dialog(fs::path &path, std::initializer_list<std::string> type_filters) {
    met_trace();

    auto filters 
      = type_filters
      | vws::transform([](const std::string &name) {
        return nfdu8filteritem_t { .name = nullptr, .spec = name.c_str() }; })
      | view_to<std::vector<nfdu8filteritem_t>>();

    nfdchar_t  *out = nullptr;
    nfdresult_t res = NFD_OpenDialog(&out, filters.data(), filters.size(), nullptr);
    guard(res == NFD_OKAY, false);

    path = out;
    free(out);

    return true;
  }

  bool load_dialog_mult(std::vector<fs::path> &paths, std::initializer_list<std::string> type_filters) {
    met_trace();
    
    auto filters 
      = type_filters
      | vws::transform([](const std::string &name) {
        return nfdu8filteritem_t { .name = nullptr, .spec = name.c_str() }; })
      | view_to<std::vector<nfdu8filteritem_t>>();

    const nfdpathset_t *out;
    nfdpathsetsize_t out_count;
    nfdresult_t  res = NFD_OpenDialogMultiple(&out, filters.data(), filters.size(), nullptr);
    guard(res == NFD_OKAY, false);
    NFD_PathSet_GetCount(&out, &out_count);
    
    paths.clear();
    for (uint i = 0; i < out_count; ++i) {
      nfdchar_t *p;
      NFD_PathSet_GetPathU8(&out, i, &p);
      paths.push_back(p);
    }

    NFD_PathSet_Free(&out);

    return true;
  }
  
  bool save_dialog(fs::path &path, std::initializer_list<std::string> type_filters) {
    met_trace();
    
    auto filters 
      = type_filters
      | vws::transform([](const std::string &name) {
        return nfdu8filteritem_t { .name = nullptr, .spec = name.c_str() }; })
      | view_to<std::vector<nfdu8filteritem_t>>();

    nfdchar_t  *out = nullptr;
    nfdresult_t res = NFD_SaveDialog(&out, filters.data(), filters.size(), nullptr, nullptr);
    guard(res == NFD_OKAY, false);
    
    path = out;
    free(out);
    
    return true;
  }
} // namespace met::detail