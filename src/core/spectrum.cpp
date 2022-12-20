#include <metameric/core/io.hpp>
#include <metameric/core/spectrum.hpp>
#include <array>
#include <algorithm>
#include <ranges>

namespace met {
  namespace models {
    #include <metameric/core/detail/spectrum_models.ext>
    
    // Linear color space transformations
    eig::Matrix3f xyz_to_srgb_transform {{ 3.240479f, -1.537150f,-0.498535f },
                                         {-0.969256f,  1.875991f, 0.041556f },
                                         { 0.055648f, -0.204043f, 1.057311f }};
    eig::Matrix3f srgb_to_xyz_transform {{ 0.412453f, 0.357580f, 0.180423f },
                                         { 0.212671f, 0.715160f, 0.072169f },
                                         { 0.019334f, 0.119193f, 0.950227f }};

    // Color matching functions
    CMFS cmfs_cie_xyz = (xyz_to_srgb_transform * 
                         io::cmfs_from_data(cie_wavelength_values, cie_xyz_values_x, cie_xyz_values_y, cie_xyz_values_z).transpose()
                        ).transpose();

    // Illuminant spectra
    Spec emitter_cie_e       = 1.f;
    Spec emitter_cie_d65     = io::spectrum_from_data(cie_wavelength_values, cie_d65_values);
    Spec emitter_cie_fl2     = io::spectrum_from_data(cie_wavelength_values, cie_fl2_values);
    Spec emitter_cie_fl11    = io::spectrum_from_data(cie_wavelength_values, cie_fl11_values);
    Spec emitter_cie_ledb1   = io::spectrum_from_data(cie_wavelength_values, cie_ledb1_values);
    Spec emitter_cie_ledrgb1 = io::spectrum_from_data(cie_wavelength_values, cie_ledrgb1_values);
  } // namespace models 
} // namespace met
