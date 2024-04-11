#pragma once

#include <metameric/core/spectrum.hpp>

namespace met {
  // Output moment coefficients
  constexpr static uint moment_coeffs = 12;
  using Moments = eig::Array<float, moment_coeffs, 1>;

  // Compute trigonometric moments representing a given discrete spectral reflectance
  Moments spectrum_to_moments(const Spec &s);

  // Compute a discrete spectral reflectance given trigonometric moments
  Spec         moments_to_spectrum(const Moments &m);
  float        moments_to_reflectance(float wvl, const Moments &m);
  eig::Array4f moments_to_reflectance(const eig::Array4f &wvls, const Moments &m);

  // Bit packing helper
  inline
  eig::Array4u pack_moments_12x10(const Moments &m) {
    constexpr auto pack = [](float f, float scale) -> uint { 
      return static_cast<uint>(std::round((std::clamp(f, -1.f, 1.f) + 1.f) * 0.5f * scale));
    };
    constexpr auto pack_11 = std::bind(pack, std::placeholders::_1, 2048.f);
    constexpr auto pack_10 = std::bind(pack, std::placeholders::_1, 1024.f);
    
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

    u.in.b0  = pack_11(m[0]); u.in.b1  = pack_11(m[1]);  u.in.b2  = pack_10(m[2]);
    u.in.b3  = pack_11(m[3]); u.in.b4  = pack_11(m[4]);  u.in.b5  = pack_10(m[5]);
    u.in.b6  = pack_11(m[6]); u.in.b7  = pack_11(m[7]);  u.in.b8  = pack_10(m[8]);
    u.in.b9  = pack_11(m[9]); u.in.b10 = pack_11(m[10]); u.in.b11 = pack_10(m[11]);

    return u.out;
  }

  // Bit unpacking helper
  inline
  Moments unpack_moments_12x10(const eig::Array4u &p) {
    constexpr auto unpack_11 = [](uint i) -> float { 
      float f = static_cast<float>(i) / 2048.f;
      return f * 2.f - 1.f;
    };
    constexpr auto unpack_10 = [](uint i) -> float {
      float f = static_cast<float>(i) / 1024.f;
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

    return Moments {
      unpack_11(u.out.b0), unpack_11(u.out.b1),  unpack_10(u.out.b2),  
      unpack_11(u.out.b3), unpack_11(u.out.b4),  unpack_10(u.out.b5),  
      unpack_11(u.out.b6), unpack_11(u.out.b7),  unpack_10(u.out.b8),  
      unpack_11(u.out.b9), unpack_11(u.out.b10), unpack_10(u.out.b11),  
    };
  }
} // namespace met