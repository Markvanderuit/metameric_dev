#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/scene/detail/utility.hpp>

namespace met {
  // Camera and render settings data layout; a simple description
  // of how to render the current scene, either to screen or film.
  struct View {
    bool         draw_frustrum = false; // Draw frustrum in viewport?
    uint         observer_i    = 0;     // Referral to underlying CMFS
    Transform    camera_trf;            // Transform applied to scene camera
    float        camera_fov_y  = 45.f;  // Vertical field of view
    eig::Array2u film_size     = 256;   // Pixel count of film target

  public: // Boilerplate
    bool operator==(const View &o) const {
      guard(film_size.isApprox(o.film_size), false);
      return std::tie(draw_frustrum, observer_i, camera_trf, camera_fov_y)
          == std::tie(o.draw_frustrum, o.observer_i, o.camera_trf, o.camera_fov_y);
    }
  };

  // Template specialization of SceneStateHandler that exposes fine-grained
  // state tracking for object members in the program view
  namespace detail {
    template <>
    struct SceneStateHandler<View> : public SceneStateHandlerBase<View> {
      SceneStateHandler<decltype(View::draw_frustrum)> draw_frustrum;
      SceneStateHandler<decltype(View::observer_i)>    observer_i;
      SceneStateHandler<decltype(View::camera_trf)>    camera_trf;
      SceneStateHandler<decltype(View::camera_fov_y)>  camera_fov_y;
      SceneStateHandler<decltype(View::film_size)>     film_size;

    public:
      bool update(const View &o) override {
        met_trace();
        return m_mutated = 
        ( draw_frustrum.update(o.draw_frustrum)
        | observer_i.update(o.observer_i)
        | camera_trf.update(o.camera_trf) 
        | camera_fov_y.update(o.camera_fov_y)
        | film_size.update(o.film_size)
        );
      }
    };
  } // namespace detail
} // namespace met