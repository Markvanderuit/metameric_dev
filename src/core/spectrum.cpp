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
    eig::Matrix3f xyz_to_rec709_transform {{ 3.2409699419f,-1.5373831776f,-0.4986107603 },
                                           {-0.9692436363f, 1.8759675015f, 0.0415550574 },
                                           { 0.0556300797f,-0.2039769589f, 1.0569715142 }};
    eig::Matrix3f xyz_to_rec2020_transform {{ 1.7166511880f,-0.3556707838f,-0.2533662814f },
                                            {-0.6666843518f, 1.6164812366f, 0.0157685458f },
                                            { 0.0176398574f,-0.0427706133f, 0.9421031212f }};
    eig::Matrix3f xyz_to_ap1_transform {{ 1.6410233797f,-0.3248032942f,-0.2364246952f },
                                        {-0.6636628587f, 1.6153315917f, 0.0167563477f },
                                        { 0.0117218943f,-0.0082844420f, 0.9883948585f }};
    eig::Matrix3f rec2020_to_xyz_transform = xyz_to_rec2020_transform.inverse().eval();
    eig::Matrix3f rec709_to_xyz_transform = xyz_to_rec709_transform.inverse().eval();
    eig::Matrix3f ap1_to_xyz_transform = xyz_to_ap1_transform.inverse().eval();
    
    // Color matching functions
    CMFS cmfs_cie_xyz = io::cmfs_from_data(cie_wavelength_values, cie_xyz_values_x, cie_xyz_values_y, cie_xyz_values_z);

    // Illuminant spectra
    Spec emitter_cie_e       = 1.f;
    Spec emitter_cie_d65     = io::spectrum_from_data(cie_wavelength_values, cie_d65_values);
    Spec emitter_cie_fl2     = io::spectrum_from_data(cie_wavelength_values, cie_fl2_values);
    Spec emitter_cie_fl11    = io::spectrum_from_data(cie_wavelength_values, cie_fl11_values);
    Spec emitter_cie_ledb1   = io::spectrum_from_data(cie_wavelength_values, cie_ledb1_values);
    Spec emitter_cie_ledrgb1 = io::spectrum_from_data(cie_wavelength_values, cie_ledrgb1_values);
  } // namespace models 

  CMFS ColrSystem::finalize_indirect(const Spec &sd) const {
    Spec indirect = sd.pow(n_scatters - 1) * illuminant;

    CMFS to_xyz = (cmfs.array().colwise() * indirect   * wavelength_ssize)
                / (cmfs.array().col(1)    * illuminant * wavelength_ssize).sum();
    CMFS to_rgb = (models::xyz_to_srgb_transform * to_xyz.matrix().transpose()).transpose();

    return to_rgb;
  }

  CMFS ColrSystem::finalize_direct() const {
    CMFS to_xyz = (cmfs.array().colwise() * illuminant * wavelength_ssize)
                / (cmfs.array().col(1)    * illuminant * wavelength_ssize).sum();
    CMFS to_rgb = (models::xyz_to_srgb_transform * to_xyz.matrix().transpose()).transpose();

    return to_rgb;
  }

} // namespace met
