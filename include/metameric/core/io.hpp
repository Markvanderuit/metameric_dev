// Copyright (C) 2024 Mark van de Ruit, Delft University of Technology.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <metameric/core/fwd.hpp>
#include <filesystem>
#include <string>

namespace met {
  namespace fs = std::filesystem;

  namespace io {
    // Return a copy of a provided path with a given extension (re)-placed
    inline
    fs::path path_with_ext(fs::path path, std::string_view ext) {
      return path.replace_extension(ext);
    }

    // Simple string load/save to/from file
    std::string load_string(const fs::path &path);
    void        save_string(const fs::path &path, const std::string &string);

    // Simple spectrum load/save to/from file
    // Input should be a text file, containing a single wavelength and measured value per line, and
    // optional comments marked with '#'. This is the same format used in the Mitsuba renderer
    Spec load_spec(const fs::path &path);
    void save_spec(const fs::path &path, const Spec &s);

    // Simple cmfs load/save to/from file
    // Input should be a text file, containing a single wavelength and three values per line, and
    // optional comments marked with '#'. This is similar to the spectrum format described above.
    CMFS load_cmfs(const fs::path &path);
    void save_cmfs(const fs::path &path, const CMFS &s);

    // Simple basis function load from file
    // Input should be a text file, containing a single wavelength and 'm' values per line, and
    // optional comments marked with '#'. This is similar to the spectrum format described above.
    // See CmakeLists.txt, 'MET_WAVELENGTH_BASES' for the value of 'm'.
    Basis load_basis(const fs::path &path);

    // Load a discrete spectral distribution from sequentially increasing wvl/value data
    Spec spectrum_from_data(std::span<const float> wvls, std::span<const float> values, bool remap=false);

    // Load a discrete trio of color matching functions from sequentially increasing wvl/value data
    CMFS cmfs_from_data(std::span<const float> wvls,  std::span<const float> values_x,
                        std::span<const float> values_y, std::span<const float> values_z);

    // Load a set of basis functions from sequentially increasing wvl/value data
    Basis basis_from_data(std::span<const float>     wvls_mean, 
                          std::span<const float>     sgnl_mean, 
                          std::span<const float>     wvls_func, 
                          std::span<Basis::vec_type> sgnl_func);

    // Split a discrete spectral distribution or color matching functions into sequentially 
    // increasing wvl/value/*/* data
    std::array<std::vector<float>, 2> spectrum_to_data(const Spec &s);
    std::array<std::vector<float>, 4> cmfs_to_data(const CMFS &s);
  } // namespace io
} // namespace met