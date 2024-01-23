#pragma once

#include <metameric/core/scene.hpp>
#include <metameric/render/path.hpp>
#include <metameric/render/sensor.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/utility.hpp>

namespace met {
  namespace detail {
    class BaseQuery {
    protected:
      gl::Buffer m_paths; // Query render target

    public:
      const gl::Buffer &paths() const { return m_paths; }

      virtual const gl::Buffer &query(const PathQuery &sensor, const Scene &scene) = 0;
    };
  } // namespace detail

  struct PathQueryPrimitiveCreateInfo {
    // Maximum path length
    uint max_depth = path_max_depth;
  };

  class PathQueryPrimitive : public detail::BaseQuery {
    gl::Program m_program;

  public:
    using InfoType = PathQueryPrimitiveCreateInfo;

    PathQueryPrimitive(PathQueryPrimitiveCreateInfo info);

    const gl::Buffer &query(const PathQuery &sensor, const Scene &scene) override;
  };
} // namespace met