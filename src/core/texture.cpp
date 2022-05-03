#include <metameric/core/texture.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace met {
  Texture::Texture(TextureLoadInfo info) {
    // // Check that file path exists
    // expr_check(std::filesystem::exists(path),
    //   fmt::format("failed to resolve path \"{}\"", path.string()));

    const std::string path_str = info.path.string();

    int x, y, n;
    unsigned char *data = stbi_load(path_str.c_str(), &x, &y, &n, 0);
    // ...
    stbi_image_free(data);
  }

  Texture::~Texture() {

  }
} // namespace met