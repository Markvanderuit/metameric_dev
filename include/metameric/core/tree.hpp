#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <ranges>
#include <vector>

namespace met {
  // FWD
  struct BasisTreeNode;

  struct BasisTreeNode {
  public: /* public data components */
    // Node data
    uint         depth;
    Chromaticity bbox_min, bbox_max; // Bounding box
    Spec         basis_mean; // Average of spectra used to build basis functions
    Basis        basis;      // Basis functions over spectra in bounding box region
    
    // Child node data
    std::vector<BasisTreeNode> children;
    
  public: /* public methods */
    std::pair<Spec, Basis> traverse(const Chromaticity &xy) const {
      if (depth == 1)
        return { basis_mean, basis };

      // fmt::print("{}\n", depth);
      if (auto it = std::ranges::find_if(children, [&](const auto &n) { return n.is_in_node(xy); }); it != children.end()) {
        return it->traverse(xy);
      } else {
        return { basis_mean, basis };
      }
    }

    bool is_in_node(const Chromaticity &xy) const {
      return xy.max(bbox_min).min(bbox_max).isApprox(xy);
    }
  };
}