#include <metameric/core/math.hpp>

namespace met {
} // namespace met

namespace Eigen {
  Affine3f lookat_rh(const Vector3f &eye,
                     const Vector3f &cen,
                     const Vector3f &up) {
    Vector3f f = (cen - eye).normalized();
    Vector3f s = f.cross(up).normalized();
    Vector3f u = s.cross(f);

    Affine3f trf;
    trf.matrix() << s[0], s[1], s[2],-s.dot(eye),
                    u[0], u[1], u[2],-u.dot(eye),
                   -f[0],-f[1],-f[2], f.dot(eye),
                    0,    0,    0,    1;
    return trf;
  }

  Affine3f lookat_lh(const Vector3f &eye,
                     const Vector3f &cen,
                     const Vector3f &up) {
    Vector3f f = (cen - eye).normalized();
    Vector3f s = up.cross(f).normalized();
    Vector3f u = f.cross(s);

    Affine3f trf;
    trf.matrix() << s[0], s[1], s[2],-s.dot(eye),
                    u[0], u[1], u[2],-u.dot(eye),
                    f[0], f[1], f[2],-f.dot(eye),
                    0,    0,    0,    1;
    return trf;
  }

  Projective3f ortho(float left, float right, float bottom, float top, float near, float far) {
    const float _00 = 2.f / (right - left);
    const float _11 = 2.f / (top - bottom);
    const float _22 = 2.f / (far - near);
    const float _30 = (right + left) / (right - left);
    const float _31 = (top + bottom) / (top - bottom);
    const float _32 = (far + near) / (far - near);

    Projective3f trf;
    trf.matrix() << _00, 0, 0, 0,
                    0, _11, 0, 0,
                    0, 0, _22, 0,
                    _30,_31,_32,0;
    return trf;
  }

  Projective3f perspective_rh_no(float fovy, float aspect, float near, float far) {
    const float tan_half_fovy = std::tan(fovy / 2.f);
    const float _00 = 1.f / (aspect * tan_half_fovy);
    const float _11 = 1.f / tan_half_fovy;
    const float _22 = -(far + near) / (far - near);
    const float _32 = -(2.f * far * near) / (far - near);
    const float _23 = -1;

    Projective3f trf;
    trf.matrix() << _00, 0, 0, 0,
                    0, _11, 0, 0,
                    0, 0,_22,_32,
                    0, 0,_23,  0;
    return trf;
  }
} // namespace Eigen
