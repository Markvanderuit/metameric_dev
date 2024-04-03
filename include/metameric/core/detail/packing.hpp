#pragma once

#include <metameric/core/math.hpp>

namespace met::detail {
  /* 
    Most of this header contains copies of float/snorm/unorm packing routines in the 
    GLM library, adapted for Eigen types. Useful for vertex data packing and stuff.
    - src: https://github.com/g-truc/glm/blob/master/glm/detail/type_half.inl
    - src: https://github.com/g-truc/glm/blob/master/glm/detail/func_packing.inl
  */
  
  // Copied entirely from glm::detail::overflow
  inline
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
  inline
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
  inline
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

  // Pack a pair of floats to half precision floats in a single object
  // follows glm::packHalf2x16
  inline
  uint pack_half_2x16(const eig::Array2f &v) {
    union { short in[2]; uint out; } u;
    u.in[0] = detail::to_float16(v.x());
    u.in[0] = detail::to_float16(v.y());
    return u.out;
  }

  // Inverse of pack_half_2x16
  // follows glm::packHalf2x16
  inline
  eig::Array2f unpack_half_2x16(uint i) {
    union { uint in; short out[2]; } u;
    u.in = i;
    return eig::Array2f(detail::to_float32(u.out[0]), detail::to_float32(u.out[1]));
  }

  // Pack a pair of floats to unsigned [0, 1] shorts in a single object
  // follows glm::packUnorm2x16
  inline
  uint pack_unorm_2x16(const eig::Array2f &v) {
    union { ushort in[2]; uint out; } u;
    auto result = (v.max(0.f).min(1.f) * 65535.f).round().cast<ushort>().eval();
    u.in[0] = result[0];
    u.in[1] = result[1];
    return u.out;
  }

  // Pack a pair of floats to unsigned [0, 1] shorts in a single object, taking
  // the higher nearest value
  inline
  uint pack_unorm_2x16_ceil(const eig::Array2f &v) {
    union { ushort in[2]; uint out; } u;
    auto result = (v.max(0.f).min(1.f) * 65535.f).ceil().cast<ushort>().eval();
    u.in[0] = result[0];
    u.in[1] = result[1];
    return u.out;
  }

  // Pack a pair of floats to unsigned [0, 1] shorts in a single object, taking
  // the lower nearest value
  inline
  uint pack_unorm_2x16_floor(const eig::Array2f &v) {
    union { ushort in[2]; uint out; } u;
    auto result = (v.max(0.f).min(1.f) * 65535.f).floor().cast<ushort>().eval();
    u.in[0] = result[0];
    u.in[1] = result[1];
    return u.out;
  }

  // Inverse of unpack_unorm_2x16
  // follows glm::unpackUnorm2x16
  inline
  eig::Array2f unpack_unorm_2x16(uint i) {
    union { uint in; ushort out[2]; } u;
    u.in = i;
		return eig::Array2f(u.out[0], u.out[1]) * 1.5259021896696421759365224689097e-5f;
  }

  // Pack a pair of floats to [0, 1] shorts in a single object
  // follows glm::packSnorm2x16
  inline
  uint pack_snorm_2x16(const eig::Array2f &v) {
    union { short in[2]; uint out; } u;
    auto result = (v.max(-1.f).min(1.f) * 32767.f).round().cast<short>().eval();
    u.in[0] = result[0];
    u.in[1] = result[1];
    return u.out;
  }

  // Inverse of pack_snorm_2x16
  // follows glm::unpackSnorm2x16
  inline
  eig::Array2f unpack_snorm_2x16(uint i) {
    union { uint in; short out[2]; } u;
    u.in = i;
		return (eig::Array2f(u.out[0], u.out[1]) * 3.0518509475997192297128208258309e-5f).max(-1.f).min(1.f);
  }

  // Pack a quad of floats to [0, 1] chars in a single object
  // follows glm::packUnorm4x8
  inline
  uint pack_unorm_4x8(const eig::Array4f &v) {
    union { uchar in[4]; uint out; } u;
    auto result = (v.max(0.f).min(1.f) * 255.f).round().cast<uchar>().eval();
		u.in[0] = result[0];
		u.in[1] = result[1];
		u.in[2] = result[2];
		u.in[3] = result[3];
    return u.out;
  }

  // Pack a quad of floats to [0, 1] chars in a single object, taking
  // the nearest lower value
  inline
  uint pack_unorm_4x8_floor(const eig::Array4f &v) {
    union { uchar in[4]; uint out; } u;
    auto result = (v.max(0.f).min(1.f) * 255.f).floor().cast<uchar>().eval();
		u.in[0] = result[0];
		u.in[1] = result[1];
		u.in[2] = result[2];
		u.in[3] = result[3];
    return u.out;
  }

  // Pack a quad of floats to [0, 1] chars in a single object, taking
  // the nearest lower value
  inline
  uint pack_unorm_4x8_ceil(const eig::Array4f &v) {
    union { uchar in[4]; uint out; } u;
    auto result = (v.max(0.f).min(1.f) * 255.f).ceil().cast<uchar>().eval();
		u.in[0] = result[0];
		u.in[1] = result[1];
		u.in[2] = result[2];
		u.in[3] = result[3];
    return u.out;
  }

  // Inverse of pack_unorm_4x8
  // follows glm::unpackUnorm4x8
  inline
  eig::Array4f unpack_unorm_4x8(uint i) {
    union { uint in; uchar out[4]; } u;
		u.in = i;
		return eig::Array4f(u.out[0], u.out[1], u.out[2], u.out[3]) * 0.0039215686274509803921568627451f;
  }
  
  // Octagonal encoding for normal vectors; 3x32f -> 2x32f
  inline
  eig::Array2f pack_unorm_2x32_octagonal(eig::Array3f n) {
    float l1 = n.abs().sum();
    eig::Array2f v = n.head<2>() / n.abs().sum();
    if (n.z() < 0.f)
      v = (1.f - n.head<2>().reverse())
        * (v.head<2>().unaryExpr([](float f) { return f >= 0.f ? 1.f : -1.f; }));
    v = v * 0.5f + 0.5f;
    return v;
  }

  // Octagonal encoding for normal vectors; 3x32f -> 2x32f
  inline
  eig::Array2f pack_snorm_2x32_octagonal(eig::Array3f n) {
    eig::Array2f v = n.head<2>() / n.abs().sum();
    if (n.z() < 0.f)
      v = (1.f - n.head<2>().reverse())
        * (v.head<2>().unaryExpr([](float f) { return f >= 0.f ? 1.f : -1.f; }));
    return v;
  }

  // Octagonal decoding for normal vectors; 3x32f -> 2x32f
  inline
  eig::Array3f unpack_unorm_3x32_octagonal(eig::Array2f v) {
    v = v * 2.f - 1.f;
    eig::Array3f n = { v.x(), v.y(), 1.f - v.abs().sum() };
    if (n.z() < 0.f)
      n.head<2>() = (1.f - n.head<2>().reverse().abs())
                  * (n.head<2>().unaryExpr([](float f) { return f >= 0.f ? 1.f : -1.f; }));
    return n.matrix().normalized().eval();
  }

  // Octagonal decoding for normal vectors; 3x32f -> 2x32f
  inline
  eig::Array3f unpack_snorm_3x32_octagonal(eig::Array2f v) {
    eig::Array3f n = { v.x(), v.y(), 1.f - v.abs().sum() };
    if (n.z() < 0.f)
      n.head<2>() = (1.f - n.head<2>().reverse().abs())
                  * (n.head<2>().unaryExpr([](float f) { return f >= 0.f ? 1.f : -1.f; }));
    return n.matrix().normalized().eval();
  }

  /*
    The rest of this header focuses on bvh/mesh data packing. 
   */

  // FWD
  struct Vertex;
  struct VertexPack;
  struct Primitive;
  struct PrimitivePack;

  // Simple unpacked vertex data
  struct Vertex {
    eig::Vector3f p;
    eig::Vector3f n;
    eig::Vector2f tx;
  
  public:
    VertexPack pack() const;
  };
  
  // Packed vertex struct data
  struct VertexPack {
    uint p0; // unorm, 2x16
    uint p1; // unorm, 1x16 + padding 1x16
    uint n;  // snorm, 2x16
    uint tx; // unorm, 2x16

  public:
    Vertex unpack() const;
  };
  static_assert(sizeof(VertexPack) == 16);

  // Simple unpacked primitive data
  struct Primitive {
    Vertex v0, v1, v2;
  
  public:
    PrimitivePack pack() const;
  };

  // Packed primitive struct data
  struct alignas(64) PrimitivePack {
    VertexPack v0, v1, v2; // 16by remainder. Can we fill in something here?
  
  public:
    Primitive unpack() const;
  };
  static_assert(sizeof(PrimitivePack) == 64);

  // Packing definitions

  inline
  Vertex VertexPack::unpack() const {
    Vertex o;
    o.p << unpack_unorm_2x16(p0), unpack_snorm_2x16(p1).x();
    o.n << unpack_snorm_2x16(p1).y(), unpack_snorm_2x16(n);
    o.n.normalize();
    o.tx << unpack_unorm_2x16(tx);
    return o;
  }

  inline
  VertexPack Vertex::pack() const {
    auto tx_ = tx.unaryExpr([](float f) {
      int i = static_cast<int>(f);
      return (i % 2) ? 1.f - (f - i) : f - i;
    }).eval();
    return VertexPack {
      .p0 = pack_unorm_2x16({ p.x(), p.y() }),
      .p1 = pack_snorm_2x16({ p.z(), n.x() }),
      .n  = pack_snorm_2x16({ n.y(), n.z() }),
      .tx = pack_unorm_2x16(tx_)
    };
  }

  inline
  Primitive PrimitivePack::unpack() const {
      return { .v0 = v0.unpack(), .v1 = v1.unpack(), .v2 = v2.unpack() };
  }

  inline
  PrimitivePack Primitive::pack() const {
      return { .v0 = v0.pack(), .v1 = v1.pack(), .v2 = v2.pack() };
  }
} // namespace met