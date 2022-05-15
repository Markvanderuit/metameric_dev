#pragma once

namespace met {
  struct TextureViewCreateInfo {

  };

  struct TextureView {
    TextureView() = default;
    TextureView(TextureViewCreateInfo info);
    ~TextureView();

  private:
    // ...
  };
} // namespace met