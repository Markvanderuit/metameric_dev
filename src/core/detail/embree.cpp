#include <metameric/core/detail/embree.hpp>
#include <metameric/core/utility.hpp>
#include "embree4/rtcore.h"

namespace met::detail {
  void emb_error_callback(void *user_p, enum RTCError err, const char *str) {
    // ...
  }

  RTCDevice emb_device_init() {
    RTCDevice device = rtcNewDevice(nullptr);
    rtcSetDeviceErrorFunction(device, emb_error_callback, NULL);
    return device;
  }

  void emb_device_dstr(RTCDevice dev) {
    rtcReleaseDevice(dev);
  }

  RTCScene emb_init_scene(RTCDevice dev) {
    // Create new scene object
    RTCScene scene = rtcNewScene(dev);
    
    // Create new geometry object
    RTCGeometry geom = rtcNewGeometry(dev, RTC_GEOMETRY_TYPE_TRIANGLE);

    size_t n_tris = 3, n_elems = 1;
    size_t tri_size = 3 * sizeof(float), elem_size = 3 * sizeof(uint);

    auto verts_p = static_cast<eig::Array3f *>(rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, tri_size, n_tris));
    auto elems_p = static_cast<eig::Array3u *>(rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, elem_size, n_elems));
    auto verts = std::span(verts_p, n_tris);
    auto elems = std::span(elems_p, tri_size);

    // Set up with single triangle data 
    verts[0] = { 0, 0, 0 };
    verts[1] = { 1, 0, 0 };
    verts[2] = { 0, 1, 0 };
    elems[0] = { 0, 1, 2 };

    // Commit geometry to scene, attach to scene, then let it go
    rtcCommitGeometry(geom);
    rtcAttachGeometry(scene, geom);
    rtcReleaseGeometry(geom);

    // Commit scene data, starting BVH build
    rtcCommitScene(scene);


    return nullptr;
  }
} // met::detail