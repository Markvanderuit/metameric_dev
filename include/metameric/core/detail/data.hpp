#pragma once

#include <metameric/core/data.hpp>

namespace met::detail {
  // Generate a exterior hull around primary texture data, and update project data
  // to accomodate
  void init_convex_hull(ApplicationData &appl_data, uint n_exterior_samples);

  // Generate interior constraints from secondary texture data, and update project data
  // to accomodate
  void init_constraints(ApplicationData &appl_data, uint n_interior_samples,
                        std::span<const ProjectCreateInfo::ImageData> images);
} // namespace met::detail