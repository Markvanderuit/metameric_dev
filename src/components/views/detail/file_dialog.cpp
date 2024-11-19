#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/detail/file_dialog.hpp>
#include <vector>
#include <tinyfiledialogs.h>

namespace met::detail {
  bool load_dialog(fs::path &path, std::initializer_list<std::string> type_filters) {
    met_trace();

    // Build list of filter patterns
    auto filter_ptr = type_filters
                    | vws::transform([](const std::string &name) { return name.c_str(); })
                    | view_to<std::vector<const char *>>();

    auto c_str = tinyfd_openFileDialog("Load file", nullptr, filter_ptr.size(), filter_ptr.data(), nullptr, 0); 
    if (c_str) {
      path = std::string(c_str);
      return true;
    } else {
      return false;
    }
  }

  bool save_dialog(fs::path &path, std::initializer_list<std::string> type_filters) {
    met_trace();

    // Build list of filter patterns
    auto filter_ptr = type_filters
                    | vws::transform([](const std::string &name) { return name.c_str(); })
                    | view_to<std::vector<const char *>>();
    
    auto c_str = tinyfd_saveFileDialog("Save file", nullptr, filter_ptr.size(), filter_ptr.data(), nullptr);
    if (c_str) {
      path = std::string(c_str);
      return true;
    } else {
      return false;
    }
  }
} // namespace met::detail