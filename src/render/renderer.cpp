#include <metameric/render/renderer.hpp>

namespace met {
  namespace detail {
    // void GBufferRenderer::render(SceneResourceHandles scene_handles) {
    //   met_trace_full();
    // }
  } // namespace detail

  DirectRenderer::DirectRenderer(DirectRendererCreateInfo info) {
    met_trace_full();
  }

  void DirectRenderer::reset() {
    met_trace_full();
  }

  // void DirectRenderer::sample(SceneResourceHandles scene_handles) {
  //   met_trace_full();
  // }

  PathRenderer::PathRenderer(PathRendererCreateInfo info) {
    met_trace_full();
  }

  void PathRenderer::reset() {
    met_trace_full();
  }

  // void PathRenderer::sample(SceneResourceHandles scene_handles) {
  //   met_trace_full();
  // }
} // namespace met