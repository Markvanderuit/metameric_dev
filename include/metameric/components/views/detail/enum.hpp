#pragma once

namespace met::detail {
  // Specifier for which type of primitive is being edited in the viewport
  enum class ViewportInputMode : uint {
    eVertex = 0u,
    eEdge   = 1u,
    eFace   = 2u
  };
} // met::detail