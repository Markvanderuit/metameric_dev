#pragma once

#include <metameric/core/math.hpp>
#include <filesystem>

namespace met::detail {
  enum class FileDialogResultType : uint {
    eError  = 0,
    eOkay   = 1,
    eCancel = 2
  };

  FileDialogResultType open_file_dialog(std::filesystem::path &path, const std::string &type_filter = "");
  FileDialogResultType save_file_dialog(std::filesystem::path &path, const std::string &type_filter = "");
} // namespace met::detail