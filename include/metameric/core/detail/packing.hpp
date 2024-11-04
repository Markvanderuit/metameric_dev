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
    u.in[1] = detail::to_float16(v.y());
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

  // Pack a quad of floats to [0, 1] chars in a single object
  // follows glm::packSnorm4x8
  inline
  uint pack_snorm_4x8(const eig::Array4f &v) {
    union { schar in[4]; uint out; } u;
    auto result = (v.max(-1.f).min(1.f) * 127.f).round().cast<schar>().eval();
		u.in[0] = result[0];
		u.in[1] = result[1];
		u.in[2] = result[2];
		u.in[3] = result[3];
    return u.out;
  }

  // Inverse of pack_unorm_4x8
  // follows glm::unpackUnorm4x8
  inline
  eig::Array4f unpack_snorm_4x8(uint i) {
    union { uint in; schar out[4]; } u;
		u.in = i;
		return (eig::Array4f(u.out[0], u.out[1], u.out[2], u.out[3]) * 0.0078740157480315f)
      .cwiseMax(-1.f).cwiseMin(1.f).eval();
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
    The following focuses on specific 8/12-component eigen packs, which come up when dealing
    with moment/basis coefficients.
   */

  inline
  eig::Array4u pack_half_8x16(const eig::Vector<float, 8> &v) {
    eig::Array4u p;
    p[0] = pack_half_2x16(v(eig::seqN(0, 2)));
    p[1] = pack_half_2x16(v(eig::seqN(2, 2)));
    p[2] = pack_half_2x16(v(eig::seqN(4, 2)));
    p[3] = pack_half_2x16(v(eig::seqN(6, 2)));
    return p;
  }

  inline
  eig::Vector<float, 8> unpack_half_8x16(const eig::Array4u &p) {
    eig::Vector<float, 8> v;
    v(eig::seqN(0, 2)) = detail::unpack_half_2x16(p[0]);
    v(eig::seqN(2, 2)) = detail::unpack_half_2x16(p[1]);
    v(eig::seqN(4, 2)) = detail::unpack_half_2x16(p[2]);
    v(eig::seqN(6, 2)) = detail::unpack_half_2x16(p[3]);
    return v;
  }

  inline
  eig::Array4u pack_snorm_8(const eig::Vector<float, 8> &v) {
    eig::Array4u p;
    p[0] = pack_snorm_2x16(v(eig::seqN(0, 2)));
    p[1] = pack_snorm_2x16(v(eig::seqN(2, 2)));
    p[2] = pack_snorm_2x16(v(eig::seqN(4, 2)));
    p[3] = pack_snorm_2x16(v(eig::seqN(6, 2)));
    return p;
  }

  inline
  eig::Vector<float, 8> unpack_snorm_8(const eig::Array4u &p) {
    eig::Vector<float, 8> v;
    v(eig::seqN(0, 2)) = detail::unpack_snorm_2x16(p[0]);
    v(eig::seqN(2, 2)) = detail::unpack_snorm_2x16(p[1]);
    v(eig::seqN(4, 2)) = detail::unpack_snorm_2x16(p[2]);
    v(eig::seqN(6, 2)) = detail::unpack_snorm_2x16(p[3]);
    return v;
  }

  // Pack 12 signed norm-bounded values into 11 and 10 bits, respectively
  inline
  eig::Array4u pack_snorm_12(const eig::Vector<float, 12> &v) {
    constexpr auto pack = [](float f, uint bits) -> uint { 
      float f_ = std::clamp((f + 1.f) * .5f, 0.f, 1.f); // to [0, 1]
      return static_cast<uint>(std::clamp(std::round(f_ * static_cast<float>((1 << bits) - 1)), 0.f, static_cast<float>(1 << bits)));
    };
    constexpr auto pack_11 = std::bind(pack, std::placeholders::_1, 11);
    constexpr auto pack_10 = std::bind(pack, std::placeholders::_1, 10);

    union pack_t { 
      struct {
        uint b0 : 11; uint b1  : 11; uint b2  : 10; /* uint p0 : 2; */
        uint b3 : 11; uint b4  : 11; uint b5  : 10; /* uint p1 : 2; */
        uint b6 : 11; uint b7  : 11; uint b8  : 10; /* uint p2 : 2; */
        uint b9 : 11; uint b10 : 11; uint b11 : 10; /* uint p3 : 2; */
      } in;
      
      eig::Array4u out; 
      
      pack_t() { std::memset(&in, 0u, sizeof(pack_t::in)); };
    } u;

    u.in.b0  = pack_11(v[0]); u.in.b1  = pack_11(v[1]);  u.in.b2  = pack_10(v[2]);
    u.in.b3  = pack_11(v[3]); u.in.b4  = pack_11(v[4]);  u.in.b5  = pack_10(v[5]);
    u.in.b6  = pack_11(v[6]); u.in.b7  = pack_11(v[7]);  u.in.b8  = pack_10(v[8]);
    u.in.b9  = pack_11(v[9]); u.in.b10 = pack_11(v[10]); u.in.b11 = pack_10(v[11]);

    return u.out;
  }
  
  inline
  eig::Vector<float, 12> unpack_snorm_12(const eig::Array4u &p) {
    constexpr auto unpack_11 = [](uint i) -> float { 
      float f = static_cast<float>(i) / static_cast<float>((1 << 11) - 1);
      return f * 2.f - 1.f;
    };
    constexpr auto unpack_10 = [](uint i) -> float {
    float f = static_cast<float>(i) / static_cast<float>((1 << 10) - 1);
      return f * 2.f - 1.f;
    };
    
    union pack_t { 
      eig::Array4u in; 

      struct {
        uint b0 : 11; uint b1  : 11; uint b2  : 10;
        uint b3 : 11; uint b4  : 11; uint b5  : 10;
        uint b6 : 11; uint b7  : 11; uint b8  : 10;
        uint b9 : 11; uint b10 : 11; uint b11 : 10;
      } out;
      
      pack_t() { std::memset(&in, 0, sizeof(pack_t::in)); };
    } u;

    u.in = p;

    return eig::Vector<float, 12> {
      unpack_11(u.out.b0), unpack_11(u.out.b1),  unpack_10(u.out.b2),  
      unpack_11(u.out.b3), unpack_11(u.out.b4),  unpack_10(u.out.b5),  
      unpack_11(u.out.b6), unpack_11(u.out.b7),  unpack_10(u.out.b8),  
      unpack_11(u.out.b9), unpack_11(u.out.b10), unpack_10(u.out.b11),  
    };
  }

  inline
  eig::Array4u pack_snorm_16(const eig::Vector<float, 16> &v) {
    return (eig::Array4u() << pack_snorm_4x8(v(eig::seq(0,  3 ))),
                              pack_snorm_4x8(v(eig::seq(4,  7 ))),
                              pack_snorm_4x8(v(eig::seq(8,  11))),
                              pack_snorm_4x8(v(eig::seq(12, 15)))).finished();
  }
  
  inline
  eig::Vector<float, 16> unpack_snorm_16(const eig::Array4u &p) {
    return (eig::Vector<float, 16>() << unpack_snorm_4x8(p[0]),
                                        unpack_snorm_4x8(p[1]),
                                        unpack_snorm_4x8(p[2]),
                                        unpack_snorm_4x8(p[3])).finished();
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
    auto tx_ = tx/* .unaryExpr([](float f) {
      int   i = static_cast<int>(f);
      float a = f - static_cast<float>(i);
      return (i % 2) ? 1.f - a : a;
    }).eval() */;
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