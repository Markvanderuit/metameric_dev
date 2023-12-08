#pragma once

#include <metameric/core/math.hpp>

namespace met {
  /* 
    This header contains copies of float/snorm/unorm packing routines in the GLM library,
    adapted for Eigen types. Useful for vertex data packing and stuff.
    - src: https://github.com/g-truc/glm/blob/master/glm/detail/type_half.inl
    - src: https://github.com/g-truc/glm/blob/master/glm/detail/func_packing.inl
  */

  namespace detail {
    // Copied entirely from glm::detail::overflow
    float overflow() {
      volatile float f = 1e10;
      for (int i = 0; i < 10; ++i)
        f = f * f; // this will overflow before the for loop terminates
      return f;
    }

    // Copied entirely from glm::detail::uif32
    union uif32 {
      constexpr uif32() : i(0) {}
      constexpr uif32(float f_) : f(f_) {}
      constexpr uif32(uint  i_) : i(i_) {}

      float f;
      uint i;
    };

    // Copied entirely from glm::detail::toFloat32
    float to_float32(short value) {
      int s = (value >> 15) & 0x00000001;
      int e = (value >> 10) & 0x0000001f;
      int m =  value        & 0x000003ff;

      if (e == 0) {
        if (m == 0) {
          detail::uif32 result;
          result.i = static_cast<unsigned int>(s << 31);
          return result.f;
        } else {
          while (!(m & 0x00000400)) {
            m <<= 1;
            e -=  1;
          }

          e += 1;
          m &= ~0x00000400;
        }
      }
      else if (e == 31) {
        if (m == 0) {
          uif32 result;
          result.i = static_cast<unsigned int>((s << 31) | 0x7f800000);
          return result.f;
        } else {
          uif32 result;
          result.i = static_cast<unsigned int>((s << 31) | 0x7f800000 | (m << 13));
          return result.f;
        }
      }
      
      e = e + (127 - 15);
      m = m << 13;
      
      uif32 result;
      result.i = static_cast<unsigned int>((s << 31) | (e << 23) | m);
      return result.f;
    }
    
    // Copied entirely from glm::detail::toFloat16
    short to_float16(float f) {
      uif32 entry;
      entry.f = f;
      int i = static_cast<int>(entry.i);

      int s =  (i >> 16) & 0x00008000;
      int e = ((i >> 23) & 0x000000ff) - (127 - 15);
      int m =   i        & 0x007fffff;

      if (e <= 0) {
        if (e < -10)
          return short(s);
        
        m = (m | 0x00800000) >> (1 - e);
        
        if (m & 0x00001000)
          m += 0x00002000;

        return short(s | (m >> 13));
      } else if (e == 0xff - (127 - 15)) {
        if (m == 0) {
          return short(s | 0x7c00);
        } else {
          m >>= 13;
          return short(s | 0x7c00 | m | (m == 0));
        }
      } else {
        if(m &  0x00001000) {
          m += 0x00002000;

          if (m & 0x00800000) {
            m =  0;
            e += 1;
          }
        }
        
        if (e > 30) {
          overflow();
          return short(s | 0x7c00);
        }

        return short(s | (e << 10) | (m >> 13));
      }
    }
  } // namespace detail

  // Pack a pair of floats to half precision floats in a single object
  // follows glm::packHalf2x16
  uint pack_half_2x16(const eig::Array2f &v) {
    union { short in[2]; uint out; } u;
    u.in[0] = detail::to_float16(v.x());
    u.in[0] = detail::to_float16(v.y());
    return u.out;
  }

  // Inverse of pack_half_2x16
  // follows glm::packHalf2x16
  eig::Array2f unpack_half_2x16(uint i) {
    union { uint in; short out[2]; } u;
    u.in = i;
    return eig::Array2f(detail::to_float32(u.out[0]), detail::to_float32(u.out[1]));
  }

  // Pack a pair of floats to unsigned [0, 1] shorts in a single object
  // follows glm::packUnorm2x16
  uint pack_unorm_2x16(const eig::Array2f &v) {
    union { ushort in[2]; uint out; } u;
    eig::Array2us result = (v.max(0.f).min(1.f) * 65535.f).round().cast<ushort>();
    u.in[0] = result[0];
    u.in[1] = result[1];
    return u.out;
  }

  // Inverse of unpack_unorm_2x16
  // follows glm::unpackUnorm2x16
  eig::Array2f unpack_unorm_2x16(uint i) {
    union { uint in; ushort out[2]; } u;
    u.in = i;
		return eig::Array2f(u.out[0], u.out[1]) * 1.5259021896696421759365224689097e-5f;
  }

  // Pack a pair of floats to [0, 1] shorts in a single object
  // follows glm::packSnorm2x16
  uint pack_snorm_2x16(const eig::Array2f &v) {
    union { short in[2]; uint out; } u;
    eig::Array2s result = (v.max(-1.f).min(1.f) * 32767.f).round().cast<short>();
    u.in[0] = result[0];
    u.in[1] = result[1];
    return u.out;
  }

  // Inverse of pack_snorm_2x16
  // follows glm::unpackSnorm2x16
  eig::Array2f unpack_snorm_2x16(uint i) {
    union { uint in; short out[2]; } u;
    u.in = i;
		return (eig::Array2f(u.out[0], u.out[1]) * 3.0518509475997192297128208258309e-5f).max(-1.f).min(1.f);
  }


// Octagonal encoding for normal vectors; 3x32f -> 2x32f
  eig::Array2f pack_snorm_2x32_octagonal(const eig::Array3f &n) {
    float l1 = n.abs().sum();
    eig::Array2f v = n.head<2>() * (1.f / l1);
    if (n.z() < 0.f)
      v = (1.f - n.head<2>().reverse())
        * (v.head<2>().unaryExpr([](float f) { return f >= 0.f ? 1.f : -1.f; }));
    return v;
  }

  // Octagonal decoding for normal vectors; 3x32f -> 2x32f
  eig::Array3f unpack_snorm_3x32_octagonal(const eig::Array2f &v) {
    eig::Array3f n = { v.x(), v.y(), 1.f - v.abs().sum() };
    if (n.z() < 0.f)
      n.head<2>() = (1.f - n.head<2>().reverse().abs())
                  * (n.head<2>().unaryExpr([](float f) { return f >= 0.f ? 1.f : -1.f; }));
    return n.matrix().normalized().eval();
  }
} // namespace met