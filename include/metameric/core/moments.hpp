#pragma once

#include <metameric/core/spectrum.hpp>

namespace met {
  // Output moment coefficients
  constexpr static uint moment_samples = 11; // TODO shift to moment_coefficients
  using Moments = eig::Array<float, moment_samples + 1, 1>;

  // Compute trigonometric moments representing a given discrete spectral reflectance
  Moments spectrum_to_moments(const Spec &s);

  // Compute a discrete spectral reflectance given trigonometric moments
  Spec moments_to_spectrum(const Moments &m);

  // Bit packing helper
  inline
  eig::Array4u pack_moments_12x10(const Moments &m) {
    constexpr auto pack = [](float f) -> int {
      return static_cast<int>(std::round((std::clamp(f, -1.f, 1.f) * 512.f)));
    };
    
    union pack_t { 
      struct {
        int b0 : 10 = 0; int b1  : 10 = 0; int b2  : 10 = 0; int p0 : 2 = 0;
        int b3 : 10 = 0; int b4  : 10 = 0; int b5  : 10 = 0; int p1 : 2 = 0;
        int b6 : 10 = 0; int b7  : 10 = 0; int b8  : 10 = 0; int p2 : 2 = 0;
        int b9 : 10 = 0; int b10 : 10 = 0; int b11 : 10 = 0; int p3 : 2 = 0;
      } in;
      
      eig::Array4u out; 
      
      pack_t() { std::memset(&in, 0, sizeof(pack_t::in)); };
    } u;

    u.in.b0  = pack(m[0]); u.in.b1  = pack(m[1]);  u.in.b2  = pack(m[2]);
    u.in.b3  = pack(m[3]); u.in.b4  = pack(m[4]);  u.in.b5  = pack(m[5]);
    u.in.b6  = pack(m[6]); u.in.b7  = pack(m[7]);  u.in.b8  = pack(m[8]);
    u.in.b9  = pack(m[9]); u.in.b10 = pack(m[10]); u.in.b11 = pack(m[11]);

    return u.out;
  }

  // Bit unpacking helper
  inline
  Moments unpack_moments_12x10(const eig::Array4u &p) {
    constexpr auto unpack = [](int i) -> float {
      return static_cast<float>(i) / 512.f;
    };
    
    union pack_t { 
      eig::Array4u in; 

      struct {
        int b0 : 10 = 0; int b1  : 10 = 0; int b2  : 10 = 0; int p0 : 2 = 0;
        int b3 : 10 = 0; int b4  : 10 = 0; int b5  : 10 = 0; int p1 : 2 = 0;
        int b6 : 10 = 0; int b7  : 10 = 0; int b8  : 10 = 0; int p2 : 2 = 0;
        int b9 : 10 = 0; int b10 : 10 = 0; int b11 : 10 = 0; int p3 : 2 = 0;
      } out;
      
      pack_t() { std::memset(&in, 0, sizeof(pack_t::in)); };
    } u;

    u.in = p;

    return Moments({
      unpack(u.out.b0), unpack(u.out.b1),  unpack(u.out.b2),  
      unpack(u.out.b3), unpack(u.out.b4),  unpack(u.out.b5),  
      unpack(u.out.b6), unpack(u.out.b7),  unpack(u.out.b8),  
      unpack(u.out.b9), unpack(u.out.b10), unpack(u.out.b11),  
    });
  }
} // namespace met