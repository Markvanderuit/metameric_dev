#pragma once

#include <filesystem>

namespace met {
  struct TextureLoadInfo {
    std::filesystem::path path;
  };

  struct Texture {
    Texture() = default;
    Texture(TextureLoadInfo info);
    ~Texture();

  private:
    // ...
  };
} // namespace met